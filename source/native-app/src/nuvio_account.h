// Native Nuvio device pairing and persistent sessions.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <string>

bool nuvio_account_init(const std::string& config_path, const std::string& data_dir);
bool nuvio_account_saved();
std::string nuvio_account_user_id(); // cached identity only; no network request
void nuvio_pairing_cancel();

// Profile calls block; run on workers. Every data request carries the opaque
// context key returned here, so late work cannot target another profile.
#include "stremio.h"
ApiResult api_profiles_get(const std::string& key);
ApiResult api_profile_select(const std::string& key, int profile, const std::string& pin = "");
ApiResult api_profile_create(const std::string& key, const std::string& name);
std::string nuvio_profile_path();
std::string nuvio_profile_key();
ApiResult api_profile_source_update(const std::string& key, const std::string& url,
                                   bool plugin, bool remove);

ApiResult api_watched_get(const std::string& key);
ApiResult api_watched_update(const std::string& key, const std::string& video_id,
                            const std::string& title, bool watched, const std::string& content = "",
                            int season = -1, int episode = -1);

// Tracker connections/preferences belong to the selected verified account profile.
// Blocking: use workers. Returns only redacted status, except a one-time pairing code.
ApiResult api_tracking(const std::string& key, const std::string& action, const json& args = json::object(), const std::atomic<bool>* cancel = nullptr);
