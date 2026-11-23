#include "http.h"

#include <curl/curl.h>
#include <algorithm>
#include <chrono>
#include <limits>
#include <mutex>

#include "util.h"

const char* kUserAgent = "NuvioMaxd/0.5.28 (PlayStation 5; native)";

static std::string g_ca_bundle;

namespace {
// Only DNS addresses are shared. Connections, cookies, headers and credentials
// remain private to each easy handle. Warm playback/CDN lookups can be reused
// by range workers and subtitle requests instead of starting resolver threads
// for every parallel connection. Curl's normal 60-second expiry is retained.
struct DnsCache {
    std::mutex locks[CURL_LOCK_DATA_LAST];
    CURLSH* handle=nullptr;
    DnsCache() {
        handle=curl_share_init();
        if(!handle)return;
        if(curl_share_setopt(handle,CURLSHOPT_USERDATA,this)!=CURLSHE_OK ||
           curl_share_setopt(handle,CURLSHOPT_LOCKFUNC,lock)!=CURLSHE_OK ||
           curl_share_setopt(handle,CURLSHOPT_UNLOCKFUNC,unlock)!=CURLSHE_OK ||
           curl_share_setopt(handle,CURLSHOPT_SHARE,CURL_LOCK_DATA_DNS)!=CURLSHE_OK) {
            curl_share_cleanup(handle);handle=nullptr;
        }
    }
    ~DnsCache(){if(handle)curl_share_cleanup(handle);}
    static void lock(CURL*,curl_lock_data data,curl_lock_access,void* opaque) {
        if(data>CURL_LOCK_DATA_NONE && data<CURL_LOCK_DATA_LAST)
            static_cast<DnsCache*>(opaque)->locks[data].lock();
    }
    static void unlock(CURL*,curl_lock_data data,void* opaque) {
        if(data>CURL_LOCK_DATA_NONE && data<CURL_LOCK_DATA_LAST)
            static_cast<DnsCache*>(opaque)->locks[data].unlock();
    }
};
}

std::string HttpResponse::describe() const {
	if (!error.empty()) return error;
	if (status == 0) return "no response";
	return "HTTP " + std::to_string(status);
}

void http_init(const std::string& ca_bundle_path) {
	curl_global_init(CURL_GLOBAL_ALL);
	g_ca_bundle = ca_bundle_path;
	dlog("curl %s, CA bundle %s (%s)", curl_version(), ca_bundle_path.c_str(),
	     file_exists(ca_bundle_path) ? "found" : "MISSING");
}

const std::string& http_ca_bundle() { return g_ca_bundle; }

#ifdef PLATFORM_PS5_NATIVE
extern "C" int console_curl_nonblocking(int socket);  // native/console_curl.c

static int on_socket(void*, curl_socket_t s, curlsocktype) {
	console_curl_nonblocking(s);  // without it a finished request can hang
	return CURL_SOCKOPT_OK;
}
#endif

void http_setup_handle(void* curl) {
	// All clients, including JavaScript providers, must use the console DNS route.
	curl_easy_setopt(static_cast<CURL*>(curl), CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
#ifdef PLATFORM_PS5_NATIVE
	curl_easy_setopt(static_cast<CURL*>(curl), CURLOPT_SOCKOPTFUNCTION, on_socket);
#else
	(void)curl;
#endif
}

void http_share_playback_dns(void* curl) {
    static DnsCache dns;
    if(dns.handle)curl_easy_setopt(static_cast<CURL*>(curl),CURLOPT_SHARE,dns.handle);
}

struct BodySink {
	std::string* body;
	size_t limit;
	bool oversized=false;
};
static size_t write_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
	auto& sink = *static_cast<BodySink*>(userdata);
	if (size && nmemb > std::numeric_limits<size_t>::max() / size) return 0;
	const size_t bytes = size * nmemb;
	if (bytes > sink.limit - sink.body->size()) { sink.oversized=true; return 0; }
	sink.body->append(ptr, bytes);
	return bytes;
}

static size_t header_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
	auto* response = static_cast<HttpResponse*>(userdata);
	const size_t length = size * nmemb;
	const std::string line(ptr, length);
	// A redirect's Date must not substitute for a missing final response Date.
	if (starts_with(line, "HTTP/")) response->server_epoch = 0;
	if (starts_with(lower(line), "date:")) {
		const std::string value = trim(line.substr(5));
		const time_t epoch = curl_getdate(value.c_str(), nullptr);
		response->server_epoch = epoch > 0 ? double(epoch) : 0;
	}
	return length;
}

static int progress_cb(void* clientp, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
	auto* cancel = static_cast<const std::atomic<bool>*>(clientp);
	return (cancel && cancel->load()) ? 1 : 0;
}

static HttpResponse perform(const std::string& url, const std::string* post_body, long timeout_s,
                            const std::atomic<bool>* cancel, const std::vector<std::string>& headers, bool https_only = false,
                            const std::string& content_type = "application/json", bool redirects = true,
                            bool subtitle = false) {
	HttpResponse r;
	if (cancel && cancel->load()) { r.error="cancelled"; return r; }
	CURL* c = curl_easy_init();
	if (!c) {
		r.error = "curl_easy_init failed";
		return r;
	}
	char errbuf[CURL_ERROR_SIZE] = {0};
	BodySink sink{&r.body, subtitle ? 8u * 1024 * 1024 : 64u * 1024 * 1024};
	struct curl_slist* hdrs = nullptr;
	for (auto& h : headers) hdrs = curl_slist_append(hdrs, h.c_str());
	if (post_body) hdrs = curl_slist_append(hdrs, ("Content-Type: " + content_type).c_str());

	curl_easy_setopt(c, CURLOPT_URL, url.c_str());
	curl_easy_setopt(c, CURLOPT_USERAGENT, kUserAgent);
	curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, redirects ? 1L : 0L);
	curl_easy_setopt(c, CURLOPT_MAXREDIRS, 8L);
	curl_easy_setopt(c, CURLOPT_PROTOCOLS_STR, https_only ? "https" : "http,https");
	curl_easy_setopt(c, CURLOPT_REDIR_PROTOCOLS_STR, https_only ? "https" : "http,https");
	curl_easy_setopt(c, CURLOPT_ACCEPT_ENCODING, "");  // whatever curl was built with (gzip, br)
	curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, subtitle ? 30L : 10L);
	curl_easy_setopt(c, CURLOPT_TIMEOUT, timeout_s);
	curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(c, CURLOPT_ERRORBUFFER, errbuf);
	curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, write_cb);
	curl_easy_setopt(c, CURLOPT_WRITEDATA, &sink);
	curl_easy_setopt(c, CURLOPT_HEADERFUNCTION, header_cb);
	curl_easy_setopt(c, CURLOPT_HEADERDATA, &r);
	if (!g_ca_bundle.empty() && file_exists(g_ca_bundle)) curl_easy_setopt(c, CURLOPT_CAINFO, g_ca_bundle.c_str());
	http_setup_handle(c);
    if(subtitle)http_share_playback_dns(c);
	if (cancel) {
		curl_easy_setopt(c, CURLOPT_NOPROGRESS, 0L);
		curl_easy_setopt(c, CURLOPT_XFERINFOFUNCTION, progress_cb);
		curl_easy_setopt(c, CURLOPT_XFERINFODATA, (void*)cancel);
	}
	if (hdrs) curl_easy_setopt(c, CURLOPT_HTTPHEADER, hdrs);
	if (post_body) {
		curl_easy_setopt(c, CURLOPT_POST, 1L);
		curl_easy_setopt(c, CURLOPT_POSTFIELDS, post_body->c_str());
		curl_easy_setopt(c, CURLOPT_POSTFIELDSIZE, long(post_body->size()));
	}

	CURLcode rc = CURLE_OK;
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeout_s);
	for (int attempt=0; attempt<(subtitle ? 2 : 1); ++attempt) {
		if (cancel && cancel->load()) { rc=CURLE_ABORTED_BY_CALLBACK; break; }
		if (subtitle) {
			const auto remaining=std::chrono::duration_cast<std::chrono::milliseconds>(deadline-std::chrono::steady_clock::now()).count();
			if (remaining<=0) { rc=CURLE_OPERATION_TIMEDOUT; break; }
			curl_easy_setopt(c, CURLOPT_TIMEOUT_MS, long(remaining));
			curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT_MS, long(std::min<int64_t>(30000,remaining)));
            // Reuse positive playback/CDN results on the first attempt. A DNS
            // retry bypasses cached failures and performs a fresh lookup.
            curl_easy_setopt(c, CURLOPT_DNS_CACHE_TIMEOUT, attempt ? 0L : 60L);
		}
		r = HttpResponse{}; sink.oversized=false; errbuf[0]='\0';
		rc = curl_easy_perform(c);
		curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &r.status);
		const bool transient = rc==CURLE_COULDNT_RESOLVE_HOST || rc==CURLE_COULDNT_CONNECT || rc==CURLE_OPERATION_TIMEDOUT;
		// Download services often redirect to a different CDN hostname. Curl
		// retains the redirect status if resolving/connecting that host fails.
		const bool no_final_response = r.status==0 || (r.status>=300 && r.status<400);
		if (!subtitle || rc==CURLE_OK || !transient || !no_final_response || !r.body.empty() || (cancel && cancel->load())) break;
	}
	if (rc != CURLE_OK) {
		// Curl's detailed buffer may include a private subtitle URL/hostname.
		r.error = subtitle ? (sink.oversized ? "Subtitle file is too large" : curl_easy_strerror(rc)) : (errbuf[0] ? errbuf : curl_easy_strerror(rc));
		if (cancel && cancel->load()) r.error = "cancelled";
		char* ip = nullptr;
		curl_easy_getinfo(c, CURLINFO_PRIMARY_IP, &ip);
		dlog("http: %s failed (curl %d)", post_body ? "POST" : "GET", int(rc));
	}
	curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &r.status);
	char* ct = nullptr;
	if (curl_easy_getinfo(c, CURLINFO_CONTENT_TYPE, &ct) == CURLE_OK && ct) r.content_type = ct;
	if (hdrs) curl_slist_free_all(hdrs);
	curl_easy_cleanup(c);
	return r;
}

HttpResponse http_get(const std::string& url, long timeout_s, const std::atomic<bool>* cancel,
                      const std::vector<std::string>& headers) {
	return perform(url, nullptr, timeout_s, cancel, headers);
}

HttpResponse http_get_subtitle(const std::string& url, const std::atomic<bool>* cancel) {
	return perform(url, nullptr, 60, cancel, {}, false, "application/json", true, true);
}

HttpResponse http_post_json(const std::string& url, const std::string& body, long timeout_s,
                            const std::atomic<bool>* cancel, const std::vector<std::string>& headers) {
	return perform(url, &body, timeout_s, cancel, headers);
}

HttpResponse http_get_https(const std::string& url, long timeout_s, const std::atomic<bool>* cancel,
                            const std::vector<std::string>& headers) {
	return perform(url, nullptr, timeout_s, cancel, headers, true);
}

HttpResponse http_provider_request(const std::string& url, const std::string* body,
                                  const std::string& content_type, const std::vector<std::string>& headers,
                                  const std::atomic<bool>* cancel) {
	return perform(url, body, 25, cancel, headers, true, content_type, false);
}
