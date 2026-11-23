// Google Drive drive.file backup transport, with conditional updates.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "feature_http.h"
#include "json.hpp"
namespace config_drive {
using Json = nlohmann::json;
struct Application {
  std::string client_id, client_secret, refresh_token;
};
struct Device {
  std::string code, user_code, url, error;
  int interval = 5, lifetime = 600;
};
struct Token {
  std::string access, refresh, error;
  bool pending = false, slow = false;
};
struct Remote {
  std::string id, etag, bytes, error;
  bool missing = false;
};
Device begin(const Application &, const std::atomic<bool> *,
             const feature_http::Request & = {});
Token poll(const Application &, const std::string &code,
           const std::atomic<bool> *, const feature_http::Request & = {});
Token refresh(const Application &, const std::atomic<bool> *,
              const feature_http::Request & = {});
Remote download(const std::string &token, const std::string &scope,
                const std::atomic<bool> *, const feature_http::Request & = {});
Remote upload(const std::string &token, const std::string &scope,
              const Remote &previous, const std::string &bytes,
              const std::atomic<bool> *, const feature_http::Request & = {});
enum class Plan { equal, first_upload, upload, restore, conflict };
Plan plan(const std::string &base, const std::string &local,
          const std::string &remote, bool missing);
} // namespace config_drive
