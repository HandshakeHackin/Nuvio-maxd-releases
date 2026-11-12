#include "feature_http.h"
#include <algorithm>
#include <curl/curl.h>
#include <limits>
namespace feature_http {
std::string encode(const std::string &s) {
  if (s.size() > 32u << 20)
    return {};
  char *p = curl_easy_escape(nullptr, s.data(), int(s.size()));
  if (!p)
    return {};
  std::string out(p);
  curl_free(p);
  return out;
}
bool safe_url(const std::string &s, bool https) {
  if (s.empty() || s.size() > 16384 ||
      std::any_of(s.begin(), s.end(),
                  [](unsigned char c) { return c <= 32 || c == 127; }))
    return false;
  CURLU *u = curl_url();
  if (!u)
    return false;
  auto part = [&](CURLUPart key) {
    char *p = nullptr;
    std::string v;
    if (curl_url_get(u, key, &p, 0) == CURLUE_OK) {
      v = p;
      curl_free(p);
    }
    return v;
  };
  bool ok = curl_url_set(u, CURLUPART_URL, s.c_str(), 0) == CURLUE_OK;
  const auto scheme = part(CURLUPART_SCHEME);
  ok = ok && (scheme == "https" || (!https && scheme == "http")) &&
       !part(CURLUPART_HOST).empty() && part(CURLUPART_USER).empty() &&
       part(CURLUPART_PASSWORD).empty() && part(CURLUPART_FRAGMENT).empty();
  curl_url_cleanup(u);
  return ok;
}
namespace {
struct Transfer {
  Response result;
  size_t limit;
  const std::atomic<bool> *cancel;
  bool large = false;
};
size_t write(char *p, size_t a, size_t b, void *v) {
  auto &t = *static_cast<Transfer *>(v);
  if ((a && b > t.limit / a) || a * b > t.limit - t.result.body.size()) {
    t.large = true;
    return 0;
  }
  if (t.cancel && *t.cancel)
    return 0;
  t.result.body.append(p, a * b);
  return a * b;
}
size_t header(char *p, size_t a, size_t b, void *v) {
  if (a && b > 16384 / a)
    return 0;
  size_t n = a * b;
  if (n > 16384)
    return 0;
  std::string s(p, n);
  if (s.rfind("HTTP/", 0) == 0)
    static_cast<Transfer *>(v)->result.etag.clear();
  if (s.size() > 5) {
    auto key = s.substr(0, 5);
    std::transform(key.begin(), key.end(), key.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (key == "etag:") {
      auto begin = s.find_first_not_of(" \t", 5),
           end = s.find_last_not_of(" \t\r\n");
      if (begin != std::string::npos && end >= begin && end - begin < 1024)
        static_cast<Transfer *>(v)->result.etag =
            s.substr(begin, end - begin + 1);
    }
  }
  return n;
}
int progress(void *v, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
  auto c = static_cast<Transfer *>(v)->cancel;
  return c && *c;
}
} // namespace
Response perform(const Query &q, const std::atomic<bool> *cancel) {
  Response bad;
  bad.error = "Invalid request";
  if (!safe_url(q.url, q.https_only) || q.limit > (128u << 20) ||
      q.body.size() > (32u << 20) ||
      (q.method != "GET" && q.method != "POST" && q.method != "PUT" &&
       q.method != "PATCH" && q.method != "DELETE"))
    return bad;
  for (const auto &h : q.headers)
    if (h.size() > 16384 || h.find_first_of("\r\n") != std::string::npos ||
        h.find('\0') != std::string::npos)
      return bad;
  if (cancel && *cancel) {
    bad.error = "Cancelled";
    return bad;
  }
  auto *c = curl_easy_init();
  if (!c) {
    bad.error = "Request unavailable";
    return bad;
  }
  Transfer t{{}, q.limit, cancel};
  http_setup_handle(c);
  curl_easy_setopt(c, CURLOPT_URL, q.url.c_str());
  curl_easy_setopt(c, CURLOPT_PROTOCOLS_STR,
                   q.https_only ? "https" : "http,https");
  curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 0L);
  curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 15L);
  curl_easy_setopt(c, CURLOPT_TIMEOUT, 60L);
  curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 1L);
  curl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST, 2L);
  if (!http_ca_bundle().empty())
    curl_easy_setopt(c, CURLOPT_CAINFO, http_ca_bundle().c_str());
  curl_easy_setopt(c, CURLOPT_USERAGENT, kUserAgent);
  curl_easy_setopt(c, CURLOPT_ACCEPT_ENCODING, "");
  curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, write);
  curl_easy_setopt(c, CURLOPT_WRITEDATA, &t);
  curl_easy_setopt(c, CURLOPT_HEADERFUNCTION, header);
  curl_easy_setopt(c, CURLOPT_HEADERDATA, &t);
  curl_easy_setopt(c, CURLOPT_XFERINFOFUNCTION, progress);
  curl_easy_setopt(c, CURLOPT_XFERINFODATA, &t);
  curl_easy_setopt(c, CURLOPT_NOPROGRESS, 0L);
  if (q.method != "GET") {
    curl_easy_setopt(c, CURLOPT_CUSTOMREQUEST, q.method.c_str());
    curl_easy_setopt(c, CURLOPT_POSTFIELDS, q.body.data());
    curl_easy_setopt(c, CURLOPT_POSTFIELDSIZE_LARGE, curl_off_t(q.body.size()));
  }
  curl_slist *headers = nullptr;
  for (const auto &h : q.headers)
    headers = curl_slist_append(headers, h.c_str());
  curl_easy_setopt(c, CURLOPT_HTTPHEADER, headers);
  auto code = curl_easy_perform(c);
  curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &t.result.status);
  if (code != CURLE_OK) {
    t.result.body.clear();
    t.result.error = t.large             ? "Response exceeds size limit"
                     : cancel && *cancel ? "Cancelled"
                                         : "Network request failed";
  }
  curl_slist_free_all(headers);
  curl_easy_cleanup(c);
  return std::move(t.result);
}
} // namespace feature_http
