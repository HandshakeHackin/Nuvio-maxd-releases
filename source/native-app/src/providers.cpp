#include "providers.h"
#ifdef NUVIO_HAVE_PROVIDERS
#include "http.h"
extern "C" {
#include <quickjs.h>
}
#include <algorithm>
#include <arpa/inet.h>
#include <chrono>
#include <curl/curl.h>
#include <fstream>
#include <mutex>
#include <netinet/in.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <sys/socket.h>
#include <thread>

namespace providers {
namespace {
using Clock = std::chrono::steady_clock;
std::string runtime_source;
std::mutex config_mutex;
std::string tmdb_key;
struct CachedScript { std::string code; Clock::time_point expires; };
std::mutex scripts_mutex;
std::map<std::string,CachedScript> scripts;
std::string configured_tmdb_key() {
  std::lock_guard<std::mutex> lock(config_mutex); return tmdb_key;
}
struct Timer {
  int id;
  Clock::time_point when;
  JSValue fn, args;
};
struct Execution {
  const std::atomic<bool> *cancel = nullptr;
  Clock::time_point deadline;
  unsigned requests = 0;
  std::string diagnostic;
  int next_timer = 0;
  std::vector<Timer> timers;
  bool stopped() const {
    return (cancel && *cancel) || Clock::now() >= deadline;
  }
};
int interrupt(JSRuntime *, void *opaque) {
  return static_cast<Execution *>(opaque)->stopped();
}
std::string js_string(JSContext *c, JSValueConst v) {
  size_t n = 0;
  const char *s = JS_ToCStringLen(c, &n, v);
  if (!s)
    return {};
  std::string out(s, n);
  JS_FreeCString(c, s);
  return out;
}
std::string b64(const std::string &s) {
  if (s.empty())
    return {};
  std::string out(4 * ((s.size() + 2) / 3), '\0');
  EVP_EncodeBlock(reinterpret_cast<unsigned char *>(out.data()),
                  reinterpret_cast<const unsigned char *>(s.data()), s.size());
  return out;
}
// Reject local/private destinations after DNS resolution, including redirects.
bool public_address(const sockaddr *address) {
  if (address->sa_family == AF_INET) {
    auto ip =
        ntohl(reinterpret_cast<const sockaddr_in *>(address)->sin_addr.s_addr);
    return (ip >> 24) != 0 && (ip >> 24) != 10 && (ip >> 24) != 127 &&
           (ip >> 24) < 224 && (ip & 0xffff0000u) != 0xa9fe0000u &&
           (ip & 0xfff00000u) != 0xac100000u &&
           (ip & 0xffff0000u) != 0xc0a80000u &&
           (ip & 0xffc00000u) != 0x64400000u;
  }
  if (address->sa_family == AF_INET6) {
    const auto *b =
        reinterpret_cast<const sockaddr_in6 *>(address)->sin6_addr.s6_addr;
    // Public global-unicast only. Blocks mapped IPv4, loopback, ULA and
    // link-local.
    return (b[0] & 0xe0) == 0x20;
  }
  return false;
}
struct Transfer {
  Execution *execution;
  std::string body;
  json headers = json::object();
  bool overflow = false;
  bool blocked_address = false;
};
curl_socket_t open_socket(void *opaque, curlsocktype purpose,
                          struct curl_sockaddr *a) {
  auto &t = *static_cast<Transfer *>(opaque);
  if (purpose != CURLSOCKTYPE_IPCXN || !public_address(&a->addr)) {
    t.blocked_address = true;
    return CURL_SOCKET_BAD;
  }
  return socket(a->family, a->socktype, a->protocol);
}
// Never include curl's error buffer: it can contain a signed URL or token.
std::string transport_error(CURLcode rc, const Transfer &t) {
  if (t.overflow) return "Provider response exceeds 4 MB";
  if (t.blocked_address) return "Provider destination is not a public address";
  if (t.execution->stopped()) return "Provider request cancelled or deadline reached";
  return "Provider request failed: " + std::string(curl_easy_strerror(rc)) +
         " (curl " + std::to_string(static_cast<int>(rc)) + ")";
}
size_t write_body(char *p, size_t s, size_t n, void *v) {
  try {

    auto &t = *static_cast<Transfer *>(v);
    if (s && n > SIZE_MAX / s)
      return 0;
    auto bytes = s * n;
    if (t.body.size() + bytes > 4u * 1024 * 1024) {
      t.overflow = true;
      return 0;
    }
    t.body.append(p, bytes);
    return bytes;

  } catch (...) {
    return 0;
  }
}
size_t write_header(char *p, size_t s, size_t n, void *v) {
  try {

    auto &t = *static_cast<Transfer *>(v);
    std::string h(p, s * n);
    auto colon = h.find(':');
    if (colon != std::string::npos && colon < 256 && h.size() < 16384)
      t.headers[lower(trim(h.substr(0, colon)))] = trim(h.substr(colon + 1));
    return s * n;

  } catch (...) {
    return 0;
  }
}
int progress(void *opaque, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
  return static_cast<Transfer *>(opaque)->execution->stopped();
}
std::string resolve_url(const std::string &base, const std::string &relative) {
  CURLU *u = curl_url();
  if (!u)
    return {};
  char *value = nullptr;
  std::string out;
  if (curl_url_set(u, CURLUPART_URL, base.c_str(), 0) == CURLUE_OK &&
      curl_url_set(u, CURLUPART_URL, relative.c_str(), 0) == CURLUE_OK &&
      curl_url_get(u, CURLUPART_URL, &value, 0) == CURLUE_OK) {
    out = value;
    curl_free(value);
  }
  curl_url_cleanup(u);
  return out;
}
std::string origin(const std::string &url) {
  CURLU *u = curl_url();
  if (!u)
    return {};
  char *scheme = nullptr, *host = nullptr, *port = nullptr;
  std::string out;
  if (curl_url_set(u, CURLUPART_URL, url.c_str(), 0) == CURLUE_OK &&
      curl_url_get(u, CURLUPART_SCHEME, &scheme, 0) == 0 &&
      curl_url_get(u, CURLUPART_HOST, &host, 0) == 0) {
    curl_url_get(u, CURLUPART_PORT, &port, CURLU_DEFAULT_PORT);
    out = lower(std::string(scheme) + "://" + host + ":" + (port ? port : ""));
  }
  curl_free(scheme);
  curl_free(host);
  curl_free(port);
  curl_url_cleanup(u);
  return out;
}
json request(Execution &e, json req) {
  if (e.stopped())
    return {{"error", "Provider request limit or deadline reached"}};
  auto url = jstr(req, "url"), method = jstr(req, "method", "GET"),
       body = jstr(req, "body");
  auto headers = jobj(req, "headers");
  const bool tmdb = origin(url)=="https://api.themoviedb.org:443";
  if (tmdb) {
    auto key_at=url.find("api_key=");
    if (key_at!=std::string::npos) {
      auto key=url.substr(key_at+8);key=key.substr(0,key.find('&'));
      if (key.empty() || key=="undefined" || key=="null") {
        e.diagnostic="Provider metadata access is not configured in this build. Update the app or contact the maintainer.";
        return {{"error",e.diagnostic}};
      }
    }
  }
  if (method != "GET" && method != "HEAD" && method != "POST")
    return {{"error", "Unsupported provider HTTP method"}};
  for (int redirect = 0; redirect <= 5; ++redirect) {
    if (++e.requests > 32 || e.stopped())
      return {{"error", "Provider request limit or deadline reached"}};
    const bool https = starts_with(lower(url), "https://");
    if (!https && !starts_with(lower(url), "http://"))
      return {{"error", "Provider requests require HTTP or HTTPS"}};
    if (url.size() > 16384 || body.size() > 1024 * 1024 || origin(url).empty())
      return {{"error", "Invalid provider request"}};
    auto *c = curl_easy_init();
    if (!c)
      return {{"error", "Could not initialize provider request"}};
    Transfer t{&e, {}, json::object(), false, false};
    struct curl_slist *list = nullptr;
    if (headers.is_object())
      for (auto it = headers.begin(); it != headers.end(); ++it)
        if (it.value().is_string()) {
          auto key = it.key(), value = it.value().get<std::string>();
          if (key.empty() || key.find_first_of("\r\n:") != std::string::npos ||
              value.find_first_of("\r\n") != std::string::npos ||
              key.size() + value.size() > 8192)
            continue;
          if (lower(key) == "host" || lower(key) == "content-length" ||
              lower(key) == "connection")
            continue;
          list = curl_slist_append(list, (key + ": " + value).c_str());
        }
    http_setup_handle(c);
    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(c, CURLOPT_PROXY,
                     ""); // Always validate the actual resolved destination.
    curl_easy_setopt(c, CURLOPT_OPENSOCKETFUNCTION, open_socket);
    curl_easy_setopt(c, CURLOPT_OPENSOCKETDATA, &t);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT_MS, 5000L);
    auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                         e.deadline - Clock::now())
                         .count();
    curl_easy_setopt(
        c, CURLOPT_TIMEOUT_MS,
        std::max(1L, std::min(10000L, static_cast<long>(remaining))));
    curl_easy_setopt(c, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(c, CURLOPT_USERAGENT, kUserAgent);
    if (!http_ca_bundle().empty())
      curl_easy_setopt(c, CURLOPT_CAINFO, http_ca_bundle().c_str());
    curl_easy_setopt(c, CURLOPT_HTTPHEADER, list);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, write_body);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &t);
    curl_easy_setopt(c, CURLOPT_HEADERFUNCTION, write_header);
    curl_easy_setopt(c, CURLOPT_HEADERDATA, &t);
    curl_easy_setopt(c, CURLOPT_XFERINFOFUNCTION, progress);
    curl_easy_setopt(c, CURLOPT_XFERINFODATA, &t);
    curl_easy_setopt(c, CURLOPT_NOPROGRESS, 0L);
    if (method == "HEAD")
      curl_easy_setopt(c, CURLOPT_NOBODY, 1L);
    if (method == "POST") {
      curl_easy_setopt(c, CURLOPT_POST, 1L);
      curl_easy_setopt(c, CURLOPT_POSTFIELDS, body.data());
      curl_easy_setopt(c, CURLOPT_POSTFIELDSIZE_LARGE,
                       static_cast<curl_off_t>(body.size()));
    }
    auto rc = curl_easy_perform(c);
    long status = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &status);
    curl_slist_free_all(list);
    curl_easy_cleanup(c);
    if (tmdb && (status==401 || status==403))
      e.diagnostic="Provider metadata authorization was rejected (HTTP "+std::to_string(status)+"). Contact the maintainer.";
    else if ((status==403 || status==429) && e.diagnostic.empty())
      e.diagnostic="Provider website refused the request (HTTP "+std::to_string(status)+"). Try again later.";
    if (rc != CURLE_OK)
      return {{"error", transport_error(rc, t)}};
    auto location = jstr(t.headers, "location");
    if (status >= 300 && status < 400 && !location.empty() &&
        jstr(req, "redirect") != "manual") {
      auto next = resolve_url(url, location);
      if (https && !starts_with(lower(next), "https://"))
        return {{"error", "Insecure provider redirect refused"}};
      if (origin(next) != origin(url) && headers.is_object()) {
        for (auto it = headers.begin(); it != headers.end();) {
          auto k = lower(it.key());
          if (k == "authorization" || k == "cookie" ||
              k == "proxy-authorization")
            it = headers.erase(it);
          else
            ++it;
        }
      }
      if (status == 303 ||
          ((status == 301 || status == 302) && method == "POST")) {
        method = "GET";
        body.clear();
      }
      url = next;
      continue;
    }
    return {{"status", status},
            {"body", b64(t.body)},
            {"headers", t.headers},
            {"url", url}};
  }
  return {{"error", "Too many provider redirects"}};
}
JSValue native_fetch(JSContext *c, JSValueConst, int argc, JSValueConst *argv) {
  try {

    auto *e = static_cast<Execution *>(JS_GetContextOpaque(c));
    if (argc != 1)
      return JS_ThrowTypeError(c, "Invalid request");
    auto input = js_string(c, argv[0]);
    if (input.size() > 2 * 1024 * 1024)
      return JS_ThrowRangeError(c, "Request too large");
    auto req = json::parse(input, nullptr, false);
    if (!req.is_object())
      return JS_ThrowTypeError(c, "Invalid request");
    auto response=request(*e, req);
    if (!jstr(response,"error").empty() && e->diagnostic.empty())
      e->diagnostic=jstr(response,"error");
    auto out = response.dump(-1, ' ', false, json::error_handler_t::replace);
    return JS_NewStringLen(c, out.data(), out.size());

  } catch (...) {
    return JS_ThrowInternalError(c, "Provider host operation failed");
  }
}
JSValue random_bytes(JSContext *c, JSValueConst, int argc, JSValueConst *argv) {
  try {

    int32_t n = 0;
    if (argc != 1 || JS_ToInt32(c, &n, argv[0]) < 0 || n < 0 || n > 65536)
      return JS_ThrowRangeError(c, "Invalid random byte count");
    std::vector<unsigned char> b(n);
    if (n && RAND_bytes(b.data(), n) != 1)
      return JS_ThrowInternalError(c, "Random generator failed");
    return JS_NewArrayBufferCopy(c, b.data(), b.size());

  } catch (...) {
    return JS_ThrowInternalError(c, "Provider host operation failed");
  }
}
JSValue timer(JSContext *c, JSValueConst, int argc, JSValueConst *argv) {
  try {

    auto *e = static_cast<Execution *>(JS_GetContextOpaque(c));
    int64_t delay = 0;
    if (argc != 3 || !JS_IsFunction(c, argv[0]))
      return JS_ThrowTypeError(c, "Invalid timer");
    if (e->timers.size() >= 128)
      return JS_ThrowRangeError(c, "Too many provider timers");
    JS_ToInt64(c, &delay, argv[1]);
    int id = ++e->next_timer;
    e->timers.push_back(
        {id,
         Clock::now() +
             std::chrono::milliseconds(std::clamp<int64_t>(delay, 0, 40000)),
         JS_DupValue(c, argv[0]), JS_DupValue(c, argv[2])});
    return JS_NewInt32(c, id);

  } catch (...) {
    return JS_ThrowInternalError(c, "Provider host operation failed");
  }
}
JSValue clear_timer(JSContext *c, JSValueConst, int argc, JSValueConst *argv) {
  auto *e = static_cast<Execution *>(JS_GetContextOpaque(c));
  int32_t id = 0;
  if (argc)
    JS_ToInt32(c, &id, argv[0]);
  for (auto it = e->timers.begin(); it != e->timers.end(); ++it)
    if (it->id == id) {
      JS_FreeValue(c, it->fn);
      JS_FreeValue(c, it->args);
      e->timers.erase(it);
      break;
    }
  return JS_UNDEFINED;
}
std::string exception(JSContext *c) {
  auto v = JS_GetException(c);
  auto s = js_string(c, v);
  JS_FreeValue(c, v);
#ifdef PROVIDER_TEST_DIAGNOSTICS
  fprintf(stderr, "JS exception: %s\n", s.c_str());
#endif

  if (s.find("Unsupported provider module:") != std::string::npos &&
      s.size() < 160 && s.find("http") == std::string::npos)
    return s;
  return "Provider script failed (unsupported API, invalid response, or "
         "resource limit)";
}
} // namespace
bool initialize(const std::string &path) {
  std::ifstream f(path, std::ios::binary);
  std::string source((std::istreambuf_iterator<char>(f)), {});
  if (source.empty() || source.size() > 4 * 1024 * 1024)
    return false;
  runtime_source = std::move(source);
  return true;
}
bool configure(const std::string &path) {
  json config;
  const bool loaded=load_json(path,config) && config.is_object();
  auto key=loaded ? jstr(jobj(config,"tmdb"),"api_key") : "";
  const bool valid=key.empty() || (key.size()==32 && key.find_first_not_of("0123456789abcdefABCDEF")==std::string::npos);
  std::lock_guard<std::mutex> lock(config_mutex);
  tmdb_key=valid ? key : "";
  return loaded && valid;
}
void clear_script_cache() { std::lock_guard<std::mutex> lock(scripts_mutex);scripts.clear(); }
json execute(const std::string &source, const json &arguments,
             const json &settings, std::string &error,
             const std::atomic<bool> *cancel, int timeout_ms, bool describe_settings) {
  error.clear();
  if (runtime_source.empty()) {
    error = "Provider runtime is unavailable";
    return {};
  }
  if (source.size() > 1024 * 1024) {
    error = "Provider script exceeds 1 MB";
    return {};
  }
  Execution e;
  e.cancel = cancel;
  e.deadline = Clock::now() +
               std::chrono::milliseconds(std::clamp(timeout_ms, 1, 40000));
  auto *rt = JS_NewRuntime();
  if (!rt) {
    error = "Could not allocate provider runtime";
    return {};
  }
  JS_SetMemoryLimit(rt, 64u << 20);
  JS_SetCanBlock(rt, false); // Atomics.wait must not bypass cancellation.
  JS_SetMaxStackSize(rt, 1024u << 10);
  JS_SetInterruptHandler(rt, interrupt, &e);
  auto *c = JS_NewContext(rt);
  if (!c) {
    JS_FreeRuntime(rt);
    error = "Could not allocate provider context";
    return {};
  }
  JS_SetContextOpaque(c, &e);
  auto global = JS_GetGlobalObject(c);
  JS_SetPropertyStr(c, global, "__fetch",
                    JS_NewCFunction(c, native_fetch, "__fetch", 1));
  JS_SetPropertyStr(c, global, "__random",
                    JS_NewCFunction(c, random_bytes, "__random", 1));
  JS_SetPropertyStr(c, global, "__timer",
                    JS_NewCFunction(c, timer, "__timer", 3));
  JS_SetPropertyStr(c, global, "__clearTimer",
                    JS_NewCFunction(c, clear_timer, "__clearTimer", 1));
  JS_FreeValue(c, global);
  auto eval = [&](const std::string &code) {
    auto v = JS_Eval(c, code.data(), code.size(), "provider.js",
                     JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(v)) {
      error = exception(c);
      return false;
    }
    JS_FreeValue(c, v);
    return true;
  };
  json result;
  JSValue promise = JS_UNDEFINED;
  if (eval(runtime_source) &&
      eval("globalThis.SCRAPER_SETTINGS=" +
           (settings.is_object() ? settings : json::object()).dump() +
           ";globalThis.getSetting=(k,d)=>SCRAPER_SETTINGS[k]??d;globalThis."
           "TMDB_API_KEY=SCRAPER_SETTINGS.tmdbApiKey||"+json(configured_tmdb_key()).dump()+";globalThis."
           "getTmdbApiKey=()=>TMDB_API_KEY;") &&
      eval("(function(module,exports){\n" + source +
           "\n})(module,module.exports);")) {
    auto invocation = std::string("(async()=>{const p=module.exports;const ")+
                      (describe_settings ? "fn=p.onSettings||(p.default&&p.default.onSettings)||globalThis.onSettings;if(typeof fn!=='function')return '[]';" :
                      "fn=p.getStreams||(p.default&&p.default.getStreams)||globalThis.getStreams;")+"if("
                      "typeof fn!=='function')throw new Error('Missing "
                      "getStreams');const r=await fn(..." +
                      arguments.dump() +
                      ");if(!Array.isArray(r))throw new Error('Expected stream "
                      "array');return JSON.stringify(r);})()";
    promise = JS_Eval(c, invocation.data(), invocation.size(),
                      "provider-call.js", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(promise))
      error = exception(c);
    else
      while (!e.stopped()) {
        if (JS_PromiseState(c, promise) != JS_PROMISE_PENDING)
          break;
        JSContext *job_context = nullptr;
        int job = JS_ExecutePendingJob(rt, &job_context);
        if (job < 0) {
          error = exception(job_context ? job_context : c);
          break;
        }
        if (job)
          continue;
        bool fired = false;
        for (size_t i = 0; i < e.timers.size(); ++i)
          if (e.timers[i].when <= Clock::now()) {
            auto t = e.timers[i];
            e.timers.erase(e.timers.begin() + i);
            std::vector<JSValue> args;
            auto len = JS_GetPropertyStr(c, t.args, "length");
            uint32_t count = 0;
            JS_ToUint32(c, &count, len);
            JS_FreeValue(c, len);
            for (uint32_t a = 0; a < std::min(count, 32u); ++a)
              args.push_back(JS_GetPropertyUint32(c, t.args, a));
            auto ret = JS_Call(c, t.fn, JS_UNDEFINED, args.size(), args.data());
            for (auto v : args)
              JS_FreeValue(c, v);
            JS_FreeValue(c, t.fn);
            JS_FreeValue(c, t.args);
            if (JS_IsException(ret))
              error = exception(c);
            JS_FreeValue(c, ret);
            fired = true;
            break;
          }
        if (!error.empty())
          break;
        if (!fired)
          std::this_thread::sleep_for(std::chrono::milliseconds(2));
      }
    if (e.stopped())
      error = cancel && *cancel ? "Provider search cancelled"
                                : "Provider search timed out";
    else if (error.empty()) {
      auto v = JS_PromiseResult(c, promise);
      if (JS_PromiseState(c, promise) == JS_PROMISE_REJECTED) {
        auto message = js_string(c, v);
        error =
            message.find("Unsupported provider module:") != std::string::npos &&
                    message.size() < 160
                ? message
                : "Provider script failed (unsupported API or invalid "
                  "response)";
      } else {
        auto data = js_string(c, v);
        if (data.size() <= 4 * 1024 * 1024)
          result = json::parse(data, nullptr, false);
        if (!result.is_array())
          error = "Invalid provider stream response";
      }
      JS_FreeValue(c, v);
    }
  }
  if (e.stopped())
    error = cancel && *cancel ? "Provider search cancelled"
                              : "Provider search timed out";
  if (error.empty() && result.empty() && !describe_settings && !e.diagnostic.empty())
    error=e.diagnostic;
  JS_FreeValue(c, promise);
  for (auto &t : e.timers) {
    JS_FreeValue(c, t.fn);
    JS_FreeValue(c, t.args);
  }
  JS_FreeContext(c);
  JS_FreeRuntime(rt);
  return result;
}

Identity identify(const std::string &video_id, const Meta &meta) {
  Identity identity;
  identity.tmdb = meta.tmdb_id;
  identity.imdb = meta.imdb_id;
  auto first = video_id.find(':');
  const auto base = video_id.substr(0, first);
  if (starts_with(base, "tt"))
    identity.imdb = base;
  else if (identity.imdb.empty() && starts_with(meta.id, "tt"))
    identity.imdb = meta.id.substr(0, meta.id.find(':'));
  if (starts_with(video_id, "tmdb:")) {
    first = video_id.find(':', 5);
    identity.tmdb = video_id.substr(
        5, first == std::string::npos ? std::string::npos : first - 5);
  }
  auto number = [](const std::string &value) {
    int n = 0;
    if (value.empty() || value.size() > 6)
      return 0;
    for (char c : value) {
      if (c < '0' || c > '9')
        return 0;
      n = n * 10 + (c - '0');
    }
    return n;
  };
  if (first != std::string::npos) {
    auto rest = video_id.substr(first + 1);
    auto split = rest.find(':');
    if (split != std::string::npos) {
      identity.season = number(rest.substr(0, split));
      identity.episode = number(rest.substr(split + 1));
    }
  }
  for (const auto &v : meta.videos)
    if (v.id == video_id && v.episode > 0) {
      identity.season = v.season;
      identity.episode = v.episode;
      break;
    }
  return identity;
}

namespace {
std::string load_script(const Addon& repository, const ProviderScript& p,
                        std::string& error, const std::atomic<bool>* cancel) {
  if (cancel && *cancel) { error="Provider request cancelled";return {}; }
  const auto url=resolve_url(repository.transport_url,p.filename);
  const auto key=url+"\n"+p.version;
  {
    std::lock_guard<std::mutex> lock(scripts_mutex);
    auto it=scripts.find(key);
    if(it!=scripts.end() && Clock::now()<it->second.expires) return it->second.code;
  }
  Execution e;e.cancel=cancel;e.deadline=Clock::now()+std::chrono::seconds(15);
  auto response=request(e,{{"url",url}});
  if(!jstr(response,"error").empty() || jnum(response,"status")!=200) {
    const auto reason=jstr(response,"error");
    error="Could not download provider script ("+(reason.empty()?"HTTP "+std::to_string(static_cast<int>(jnum(response,"status"))):reason)+")";
    dlog("provider: script download failed id=%s status=%d",p.id.c_str(),static_cast<int>(jnum(response,"status")));
    return {};
  }
  auto encoded=jstr(response,"body");std::string code(encoded.size(),'\0');
  auto size=EVP_DecodeBlock(reinterpret_cast<unsigned char*>(code.data()),
                          reinterpret_cast<const unsigned char*>(encoded.data()),encoded.size());
  if(size<0) {error="Invalid provider script download";return {};}
  while(!encoded.empty() && encoded.back()=='=') {--size;encoded.pop_back();}
  if(size<=0 || size>1024*1024) {error="Provider script is empty or exceeds 1 MB";return {};}
  code.resize(size);
  if(cancel && *cancel) {error="Provider request cancelled";return {};}
  {
    std::lock_guard<std::mutex> lock(scripts_mutex);
    // At most eight cached scripts, each limited to 1 MiB; never store failures.
    if(scripts.size()>=8 && !scripts.count(key)) scripts.erase(scripts.begin());
    scripts[key]={code,Clock::now()+std::chrono::minutes(5)};
  }
  return code;
}
}
json settings_layout(const Addon& a,const ProviderScript& p,std::string& error,
                     const std::atomic<bool>* cancel) {
  error.clear();auto code=load_script(a,p,error,cancel);
  if(code.empty())return json::array();
  return execute(code,json::array(),p.settings,error,cancel,10000,true);
}
Result streams(const Addon &repository, const std::string &type,
               const std::string &video_id, const Meta &meta,
               const std::atomic<bool> *cancel) {
  Result out;
  const auto identity = identify(video_id, meta);
  auto tmdb = identity.tmdb;
  const auto imdb = identity.imdb;
  const int season = identity.season, episode = identity.episode;
  if (type == "series" && episode <= 0) {
    out.error = "Could not identify this episode for provider search";
    return out;
  }
  if (tmdb.empty() && starts_with(imdb, "tt")) {
    static std::mutex mutex;
    static std::map<std::string, std::string> cache;
    const auto key = type + ":" + imdb;
    {
      std::lock_guard<std::mutex> lock(mutex);
      auto it = cache.find(key);
      if (it != cache.end())
        tmdb = it->second;
    }
    if (tmdb.empty()) {
      json j;
      std::string error;
      if (fetch_json("https://v3-cinemeta.strem.io/meta/" + url_encode(type) +
                         "/" + url_encode(imdb) + ".json",
                     j, error, 10, cancel))
        tmdb = parse_meta(jobj(j, "meta")).tmdb_id;
      if (!tmdb.empty()) {
        std::lock_guard<std::mutex> lock(mutex);
        if (cache.size() > 512)
          cache.clear();
        cache[key] = tmdb;
      }
    }
  }
  if (tmdb.empty() ||
      tmdb.find_first_not_of("0123456789") != std::string::npos) {
    out.error =
        "Providers need a TMDB title ID; this title could not be mapped";
    return out;
  }
  for (const auto &p : repository.providers) {
    if (!p.enabled || (cancel && *cancel))
      continue;
    const auto media_type = type == "series" ? "tv" : type;
    if (!p.types.empty() &&
        std::find(p.types.begin(), p.types.end(), media_type) ==
            p.types.end() &&
        std::find(p.types.begin(), p.types.end(), type) == p.types.end())
      continue;
    std::string download_error;
    auto code=load_script(repository,p,download_error,cancel);
    if(code.empty()) {out.error=p.name+": "+download_error;continue;}
    std::string error;
    auto values =
        execute(code, json::array({tmdb, media_type, season, episode}),
                p.settings, error, cancel);
    if (!error.empty()) {
      out.error = p.name + ": " + error;
      continue;
    }
    // Nuvio scripts return top-level headers; Stremio addons use
    // behaviorHints.proxyHeaders.request. Preserve both when adapting results.
    for(auto& value:values) {
      if(!value.is_object() || !jobj(value,"headers").is_object())continue;
      json merged=json::object();
      for(auto it=value["headers"].begin();it!=value["headers"].end();++it) {
        const auto key=it.key();
        if(!it.value().is_string() || key.empty() || key.size()>128 ||
           key.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!#$%&'*+-.^_`|~")!=std::string::npos)continue;
        const auto text=it.value().get<std::string>();
        if(text.size()>8192 || text.find_first_of("\r\n")!=std::string::npos)continue;
        merged[key]=text;
      }
      const auto& standard=jobj(jobj(jobj(value,"behaviorHints"),"proxyHeaders"),"request");
      if(standard.is_object())for(auto it=standard.begin();it!=standard.end();++it)merged[it.key()]=it.value();
      if(!jobj(value,"behaviorHints").is_object())value["behaviorHints"]=json::object();
      if(!jobj(value["behaviorHints"],"proxyHeaders").is_object())value["behaviorHints"]["proxyHeaders"]=json::object();
      value["behaviorHints"]["proxyHeaders"]["request"]=std::move(merged);
    }
    auto parsed = parse_streams({{"streams", values}}, repository.name);
    for (auto &s : parsed)
      if (s.direct_http())
        out.streams.push_back(std::move(s));
  }
  return out;
}
} // namespace providers
#endif
