#include "http.h"

#include <curl/curl.h>

#include "util.h"

const char* kUserAgent = "NuvioMaxd/0.5.20 (PlayStation 5; native)";

static std::string g_ca_bundle;

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
#ifdef PLATFORM_PS5_NATIVE
	curl_easy_setopt(static_cast<CURL*>(curl), CURLOPT_SOCKOPTFUNCTION, on_socket);
#else
	(void)curl;
#endif
}

static size_t write_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
	auto* out = static_cast<std::string*>(userdata);
	out->append(ptr, size * nmemb);
	// Refuse absurd responses (a catalog or a poster is never this large).
	if (out->size() > 64u * 1024 * 1024) return 0;
	return size * nmemb;
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
                            const std::string& content_type = "application/json", bool redirects = true) {
	HttpResponse r;
	CURL* c = curl_easy_init();
	if (!c) {
		r.error = "curl_easy_init failed";
		return r;
	}
	char errbuf[CURL_ERROR_SIZE] = {0};
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
	curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 10L);
	curl_easy_setopt(c, CURLOPT_TIMEOUT, timeout_s);
	curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
	// The PS5 has no IPv6 route; don't let an AAAA record fail the connect.
	curl_easy_setopt(c, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
	curl_easy_setopt(c, CURLOPT_ERRORBUFFER, errbuf);
	curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, write_cb);
	curl_easy_setopt(c, CURLOPT_WRITEDATA, &r.body);
	curl_easy_setopt(c, CURLOPT_HEADERFUNCTION, header_cb);
	curl_easy_setopt(c, CURLOPT_HEADERDATA, &r);
	if (!g_ca_bundle.empty() && file_exists(g_ca_bundle)) curl_easy_setopt(c, CURLOPT_CAINFO, g_ca_bundle.c_str());
	http_setup_handle(c);
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

	CURLcode rc = curl_easy_perform(c);
	if (rc != CURLE_OK) {
		r.error = errbuf[0] ? errbuf : curl_easy_strerror(rc);
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
