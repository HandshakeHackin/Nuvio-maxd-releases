// Encrypted backup/restore and explicit conflict-aware Drive synchronization.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "config_backup.h"
#include "config_drive.h"
#include "i18n.hpp"
#include "nuvio_account.h"
#include "qrcodegen.h"
#include "session.hpp"
namespace sx {
namespace {
json drive_settings(const std::string &dir) {
  std::string s;
  if (!config_backup::read(dir + "/config-drive.json", s, 1u << 20))
    return json::object();
  auto j = json::parse(s, nullptr, false);
  return j.is_object() ? j : json::object();
}
config_drive::Application application(const json &j) {
  return {jstr(j, "client_id"), jstr(j, "client_secret"),
          jstr(j, "refresh_token")};
}
bool save_drive(const std::string &dir, const json &j) {
  return config_backup::save(dir + "/config-drive.json", j.dump());
}
struct SyncResult {
  json local, remote;
  std::string local_hash, remote_hash, error, refresh, access;
  config_drive::Remote cloud;
  config_drive::Plan plan = config_drive::Plan::conflict;
};
} // namespace
void Session::features_reset() {
  if (features_cancel_)
    *features_cancel_ = true;
  features_cancel_ = std::make_shared<std::atomic<bool>>(false);
  ++tvdb_generation_;
  tvdb_busy_ = false;
  backup_busy_ = false;
}
void Session::backup_menu() {
  if (!profile_ready_ || watching || resuming_) {
    say(i18n::tr("Choose a profile and stop playback before managing backups."),
        7);
    return;
  }
  if (backup_busy_) {
    say(i18n::tr("Backup operation in progress..."));
    return;
  }
  const auto profile = profile_generation_;
  open_dropdown(
      i18n::tr("Configuration backup and sync"),
      {i18n::tr("Create local encrypted backup"),
       i18n::tr("Restore local encrypted backup"),
       i18n::tr("Sync configuration with Google Drive"),
       i18n::tr("Connect Google Drive"),
       i18n::tr("Google OAuth application setup"),
       i18n::tr("Disconnect Google Drive")},
      0, [this, profile](int n) {
        if (profile != profile_generation_ || !profile_ready_ || backup_busy_)
          return;
        if (n >= 0 && n <= 2) {
          backup_passphrase(n);
          return;
        }
        if (n == 3) {
          backup_drive_login();
          return;
        }
        if (n == 4) {
          open_dropdown(
              i18n::tr("Google OAuth application setup"),
              {i18n::tr("Set Google OAuth client ID"),
               i18n::tr("Set Google OAuth client secret")},
              0, [this, profile](int field) {
                if (profile != profile_generation_ || field < 0 || field > 1)
                  return;
                ask_text(
                    [this, profile, field](const auto &value) {
                      if (profile != profile_generation_ || !profile_ready_ ||
                          value.size() > 4096)
                        return;
                      auto j = drive_settings(profile_dir_);
                      j[field == 0 ? "client_id" : "client_secret"] =
                          trim(value);
                      j.erase("refresh_token");
                      j.erase("base_hash");
                      const bool ok = save_drive(profile_dir_, j);
                      say(i18n::tr(ok ? "Google application settings saved. "
                                        "Connect Drive next."
                                      : "Could not save Google settings."),
                          8, !ok);
                    },
                    i18n::tr(field == 0 ? "Google OAuth client ID"
                                        : "Google OAuth client secret"),
                    true);
              });
          dropdown.message =
              i18n::tr("Use a Google OAuth application registered for TVs and "
                       "limited-input devices with Drive file access. No "
                       "Google application credentials are bundled with this "
                       "build. Personal Google passwords are never requested.");
          return;
        }
        if (n == 5) {
          open_dropdown(i18n::tr("Disconnect Google Drive"),
                        {i18n::tr("Cancel"), i18n::tr("Disconnect")}, 0,
                        [this, profile](int yes) {
                          if (yes != 1 || profile != profile_generation_)
                            return;
                          auto j = drive_settings(profile_dir_);
                          j.erase("refresh_token");
                          j.erase("base_hash");
                          const bool ok = save_drive(profile_dir_, j);
                          say(i18n::tr(ok ? "Drive disconnected on this PS5. "
                                            "Cloud backups are retained."
                                          : "Could not save Google settings."),
                              8, !ok);
                        });
        }
      });
  dropdown.message = i18n::tr(
      "Includes this profile's addons, source preferences, Live TV "
      "connections, imported playlists and TVDB settings. Excludes account "
      "sessions and playback history. Backups use a separate passphrase; keep "
      "it safe. Local file: configuration-backup.nmb in this profile's app "
      "data folder. Sync runs only when you choose it.");
}
void Session::backup_passphrase(int mode) {
  const auto profile = profile_generation_;
  ask_text(
      [this, profile, mode](const std::string &pass) {
        if (profile != profile_generation_ || !profile_ready_)
          return;
        if (!config_backup::valid_passphrase(pass)) {
          say(i18n::tr("Use at least 12 characters and at most 1024 bytes for "
                       "the backup passphrase."),
              8, true);
          return;
        }
        if (mode == 0) {
          ask_text(
              [this, profile, mode, pass](const auto &again) {
                if (profile != profile_generation_)
                  return;
                if (pass != again) {
                  say(i18n::tr("Passphrases did not match. Backup cancelled."),
                      8, true);
                  return;
                }
                backup_run(mode, pass);
              },
              i18n::tr("Confirm backup passphrase"), true);
        } else
          backup_run(mode, pass);
      },
      i18n::tr(mode == 0 ? "Create backup passphrase (12+ characters)"
                         : "Backup passphrase"),
      true);
}
void Session::backup_run(int mode, const std::string &pass) {
  if (backup_busy_ || !profile_ready_)
    return;
  backup_busy_ = true;
  const auto profile = profile_generation_;
  const auto key = prefs_.auth_key, dir = profile_dir_;
  const auto cancel = features_cancel_;
  if (mode == 0 || mode == 1) {
    bg<config_backup::Result>(
        [mode, pass, key, dir, cancel] {
          config_backup::Result r;
          const auto captured =
              api_configuration_capture(key, dir, cancel.get());
          if (!captured.ok) {
            r.error = captured.error;
            return r;
          }
          const auto snapshot = jobj(captured.result, "snapshot");
          r.hash = jstr(captured.result, "hash");
          if (mode == 0) {
            r = config_backup::encrypt(snapshot, pass, cancel.get());
            if (r.error.empty() && !*cancel &&
                !config_backup::save(dir + "/configuration-backup.nmb",
                                     r.bytes))
              r.error =
                  "Could not save encrypted backup. Previous copy was kept.";
          } else {
            std::string bytes;
            if (!config_backup::read(dir + "/configuration-backup.nmb", bytes,
                                     config_backup::max_encrypted)) {
              r.error = "Could not read the encrypted local backup.";
              return r;
            }
            auto decoded = config_backup::decrypt(
                bytes, pass, jstr(snapshot, "scope"), cancel.get());
            r.snapshot = std::move(decoded.snapshot);
            r.error = decoded.error;
          }
          return r;
        },
        [this, profile, mode](auto &r) {
          if (profile != profile_generation_)
            return;
          backup_busy_ = false;
          if (!r.error.empty()) {
            say(r.error, 10, true);
            return;
          }
          if (mode == 0) {
            say(i18n::tr("Encrypted local backup saved. Keep its passphrase "
                         "separately."),
                8);
            return;
          }
          backup_confirm_restore(r.snapshot, r.hash, {});
        });
    return;
  }
  auto settings = drive_settings(dir);
  const auto app = application(settings);
  bg<SyncResult>(
      [key, dir, pass, cancel, app, settings] {
        SyncResult r;
        const auto captured = api_configuration_capture(key, dir, cancel.get());
        if (!captured.ok) {
          r.error = captured.error;
          return r;
        }
        r.local = jobj(captured.result, "snapshot");
        r.local_hash = jstr(captured.result, "hash");
        auto token = config_drive::refresh(app, cancel.get());
        if (!token.error.empty()) {
          r.error = token.error;
          return r;
        }
        r.access = token.access;
        r.refresh = token.refresh;
        r.cloud = config_drive::download(r.access, jstr(r.local, "scope"),
                                         cancel.get());
        if (!r.cloud.error.empty()) {
          r.error = r.cloud.error;
          return r;
        }
        if (!r.cloud.missing) {
          auto decoded = config_backup::decrypt(
              r.cloud.bytes, pass, jstr(r.local, "scope"), cancel.get());
          if (!decoded.error.empty()) {
            r.error = decoded.error;
            return r;
          }
          r.remote = std::move(decoded.snapshot);
          r.remote_hash = decoded.hash;
        }
        r.plan = config_drive::plan(jstr(settings, "base_hash"), r.local_hash,
                                    r.remote_hash, r.cloud.missing);
        return r;
      },
      [this, profile, key, dir, pass, cancel, settings](auto &r) mutable {
        if (profile != profile_generation_)
          return;
        backup_busy_ = false;
        if (!r.error.empty()) {
          say(r.error, 10, true);
          return;
        }
        if (!r.refresh.empty()) {
          settings["refresh_token"] = r.refresh;
          if (!save_drive(dir, settings)) {
            say(i18n::tr("Could not save Google settings."), 8, true);
            return;
          }
        }
        if (r.plan == config_drive::Plan::equal) {
          settings["base_hash"] = r.local_hash;
          const bool ok = save_drive(dir, settings);
          say(i18n::tr(ok ? "Configuration already matches the cloud backup."
                          : "Cloud matches; sync state could not be saved."),
              8, !ok);
          return;
        }
        std::vector<std::string> rows{
            i18n::tr("Cancel"),
            i18n::tr("Upload this profile's configuration")};
        if (!r.cloud.missing)
          rows.push_back(i18n::tr("Restore cloud configuration"));
        open_dropdown(
            i18n::tr(r.plan == config_drive::Plan::conflict
                         ? "Configuration sync conflict"
                         : "Review configuration sync"),
            rows, 0,
            [this, profile, key, dir, pass, cancel, settings,
             r](int n) mutable {
              if (profile != profile_generation_ || !profile_ready_ ||
                  backup_busy_ || n < 1 || n > 2)
                return;
              if (n == 2) {
                if (!r.cloud.missing)
                  backup_confirm_restore(r.remote, r.local_hash, r.remote_hash);
                return;
              }
              backup_busy_ = true;
              bg<std::string>(
                  [key, dir, pass, cancel, r] {
                    const auto current =
                        api_configuration_capture(key, dir, cancel.get());
                    if (!current.ok)
                      return current.error;
                    if (jstr(current.result, "hash") != r.local_hash)
                      return std::string("Configuration changed. Sync again "
                                         "before uploading.");
                    auto encrypted =
                        config_backup::encrypt(r.local, pass, cancel.get());
                    if (!encrypted.error.empty())
                      return encrypted.error;
                    auto uploaded = config_drive::upload(
                        r.access, jstr(r.local, "scope"), r.cloud,
                        encrypted.bytes, cancel.get());
                    return uploaded.error;
                  },
                  [this, profile, dir, settings,
                   base = r.local_hash](auto &error) mutable {
                    if (profile != profile_generation_)
                      return;
                    backup_busy_ = false;
                    if (!error.empty()) {
                      say(error, 10, true);
                      return;
                    }
                    settings["base_hash"] = base;
                    const bool ok = save_drive(dir, settings);
                    say(i18n::tr(
                            ok ? "Encrypted cloud backup uploaded and verified."
                               : "Cloud backup verified; sync state could not "
                                 "be saved."),
                        8, !ok);
                  });
            });
        dropdown.message =
            i18n::tr(r.plan == config_drive::Plan::conflict
                         ? "Local and cloud configurations differ without a "
                           "common unchanged version. Choose which to keep. No "
                           "configuration has been overwritten."
                     : r.plan == config_drive::Plan::restore
                         ? "The cloud backup has changed. Review before "
                           "replacing this profile's configuration."
                         : "Review before uploading this profile's encrypted "
                           "configuration. Restoring replaces configuration "
                           "only; history and account sessions are preserved.");
      });
}
void Session::backup_confirm_restore(const json &snapshot,
                                     const std::string &expected_hash,
                                     const std::string &cloud_hash) {
  const auto profile = profile_generation_;
  open_dropdown(
      i18n::tr("Restore this profile's configuration?"),
      {i18n::tr("Cancel"), i18n::tr("Restore configuration")}, 0,
      [this, profile, snapshot, expected_hash, cloud_hash](int yes) {
        if (yes != 1 || profile != profile_generation_ || !profile_ready_ ||
            watching || resuming_ || backup_busy_)
          return;
        const auto key = prefs_.auth_key, dir = profile_dir_;
        const json previous{{"path", dir},
                            {"authKey", key},
                            {"profile", active_profile_},
                            {"profiles", profiles_},
                            {"userEmail", prefs_.user_email}};
        clear_profile_views();
        profile_ready_ = false;
        backup_busy_ = true;
        const auto generation = profile_generation_;
        bg<ApiResult>(
            [key, dir, snapshot, expected_hash] {
              return api_configuration_restore(key, dir, snapshot,
                                               expected_hash);
            },
            [this, generation, previous, cloud_hash](auto &r) {
              if (generation != profile_generation_)
                return;
              if (r.ok) {
                profile_apply(r.result);
                if (!cloud_hash.empty()) {
                  auto j = drive_settings(profile_dir_);
                  j["base_hash"] = cloud_hash;
                  if (!save_drive(profile_dir_, j))
                    say(i18n::tr("Restored; sync state could not be saved."), 8,
                        true);
                }
                say(i18n::tr("Configuration restored. Account sessions and "
                             "playback history were preserved."),
                    10);
              } else {
                profile_apply(previous);
                say(r.error, 12, true);
              }
            });
      });
  dropdown.message = i18n::tr(
      "Replaces this verified profile's addon preferences, Live TV sources, "
      "imported playlists and TVDB settings. Account sessions, profile PINs, "
      "watched history and resume points are preserved.");
}
void Session::backup_drive_login() {
  if (!profile_ready_ || watching || resuming_)
    return;
  const auto profile = profile_generation_;
  const int gen = ++login_gen_;
  if (device_cancel_)
    *device_cancel_ = true;
  device_cancel_ = std::make_shared<std::atomic<bool>>(false);
  const auto cancel = device_cancel_;
  device_provider_ = "drive";
  device_code_ = {};
  login_polling_ = false;
  sign_in = SignInState();
  sign_in.title = i18n::tr("Connect Google Drive");
  sign_in.cancel_label = i18n::tr("Cancel");
  sign_in.status = i18n::tr("Getting a connection code...");
  sign_in_open = true;
  const auto app = application(drive_settings(profile_dir_));
  bg<config_drive::Device>(
      [app, cancel] { return config_drive::begin(app, cancel.get()); },
      [this, profile, gen](auto &code) {
        if (profile != profile_generation_ || gen != login_gen_ ||
            !sign_in_open)
          return;
        if (!code.error.empty()) {
          sign_in.status = code.error;
          return;
        }
        device_code_.device_code = code.code;
        sign_in.code = code.user_code;
        sign_in.link = code.url.substr(8);
        std::uint8_t tmp[qrcodegen_BUFFER_LEN_MAX],
            qr[qrcodegen_BUFFER_LEN_MAX];
        if (qrcodegen_encodeText(code.url.c_str(), tmp, qr,
                                 qrcodegen_Ecc_MEDIUM, qrcodegen_VERSION_MIN,
                                 20, qrcodegen_Mask_AUTO, true)) {
          sign_in.qr_size = qrcodegen_getSize(qr);
          sign_in.qr_modules.resize(sign_in.qr_size * sign_in.qr_size);
          for (int y = 0; y < sign_in.qr_size; ++y)
            for (int x = 0; x < sign_in.qr_size; ++x)
              sign_in.qr_modules[y * sign_in.qr_size + x] =
                  qrcodegen_getModule(qr, x, y);
        }
        login_interval_ = code.interval;
        login_next_poll_ = now_seconds() + code.interval;
        login_expires_ = now_seconds() + code.lifetime;
        sign_in.status = i18n::tr("Approve Drive file access on your phone. "
                                  "Your backup stays encrypted.");
      });
}
void Session::backup_drive_poll() {
  if (device_code_.device_code.empty() || login_polling_ ||
      now_seconds() < login_next_poll_)
    return;
  if (now_seconds() >= login_expires_) {
    sign_in.status = i18n::tr("Code expired. Close and connect again.");
    device_code_ = {};
    return;
  }
  const auto profile = profile_generation_;
  const auto gen = login_gen_;
  const auto cancel = device_cancel_;
  const auto code = device_code_.device_code;
  auto settings = drive_settings(profile_dir_);
  const auto app = application(settings);
  login_polling_ = true;
  bg<config_drive::Token>(
      [app, code, cancel] {
        return config_drive::poll(app, code, cancel.get());
      },
      [this, profile, gen, settings](auto &token) mutable {
        if (profile != profile_generation_ || gen != login_gen_ ||
            !sign_in_open || device_provider_ != "drive")
          return;
        login_polling_ = false;
        if (token.slow)
          login_interval_ = std::min(60, login_interval_ + 5);
        login_next_poll_ = now_seconds() + login_interval_;
        if (token.pending)
          return;
        if (!token.error.empty() || token.refresh.empty()) {
          sign_in.status =
              token.error.empty()
                  ? i18n::tr("Google did not grant an offline connection. "
                             "Reconnect and approve access.")
                  : token.error;
          device_code_ = {};
          return;
        }
        settings["refresh_token"] = token.refresh;
        settings.erase("base_hash");
        if (!save_drive(profile_dir_, settings)) {
          sign_in.status = i18n::tr("Could not save Google settings.");
          device_code_ = {};
          return;
        }
        sign_in_cancel();
        say(i18n::tr("Google Drive connected. Choose Sync configuration to "
                     "review your first backup."),
            10);
      });
}
} // namespace sx
