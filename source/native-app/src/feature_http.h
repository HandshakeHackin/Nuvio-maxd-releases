// Bounded, cancellable requests. Credentials never follow redirects.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "http.h"
#include <functional>
namespace feature_http {
struct Query {
  std::string url, method = "GET", body;
  std::vector<std::string> headers;
  size_t limit = 8u << 20;
  bool https_only = true;
};
struct Response : HttpResponse {
  std::string etag;
};
using Request =
    std::function<Response(const Query &, const std::atomic<bool> *)>;
Response perform(const Query &, const std::atomic<bool> * = nullptr);
std::string encode(const std::string &);
bool safe_url(const std::string &, bool https_only = true);
} // namespace feature_http
