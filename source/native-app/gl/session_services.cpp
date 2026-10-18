// Native Connected Services UI and read-only Nuvio credential import.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "session.hpp"
#include "nuvio_account.h"
#include "qrcodegen.h"

namespace sx {
bool Session::services_save(const debrid::Settings& next) {
    if (!debrid::save(profile_dir_ + "/connected-services.json", next)) {
        say("Could not save Connected Services. Your previous settings are unchanged.", 7.0, true);
        return false;
    }
    services_ = next; ++services_generation_;
    detail_preparation_cancel(); next_preparation_cancel();
    prepared_->configure(services_);
    refresh_settings();
    if (d_open_) detail_refresh_streams();
    return true;
}

void Session::services_menu() {
    open_dropdown("Connected Services", {
        "TorBox: " + std::string(services_.torbox.key.empty() ? "Not connected" : "Connected"),
        "Premiumize: " + std::string(services_.premiumize.key.empty() ? "Not connected" : "Connected"),
        "Resolve playable links: " + std::string(services_.enabled ? "On (cached files)" : "Off"),
        "Resolve with: " + (services_.preferred.empty() ? std::string("Automatic") : debrid::provider_name(services_.preferred)),
        "Instant playback: " + (services_.instant_limit == 0 ? std::string("Off") : std::to_string(services_.instant_limit) + " streams"),
        services_importing_ ? "Importing from Nuvio Sync..." : "Import connections from Nuvio Sync"
    }, 0, [this](int choice) {
        if (choice == 0) service_menu("torbox");
        else if (choice == 1) service_menu("premiumize");
        else if (choice == 2) {
            if (services_.active_provider().empty() && !services_.enabled) {
                say("Connect a service, then choose it under Resolve with.", 6.0); return;
            }
            auto next = services_; next.enabled = !next.enabled;
            if (services_save(next)) cue(hui::audio::Cue::toggle);
            services_menu();
        } else if (choice == 3) {
            open_dropdown("Resolve with", {"Automatic", "TorBox", "Premiumize"},
                services_.preferred == "torbox" ? 1 : services_.preferred == "premiumize" ? 2 : 0, [this](int i) {
                    auto next = services_;
                    next.preferred = i == 1 ? "torbox" : i == 2 ? "premiumize" : "";
                    if (!next.preferred.empty() && next.key(next.preferred).empty()) {
                        say("Connect " + debrid::provider_name(next.preferred) + " first.", 6.0); return;
                    }
                    services_save(next); services_menu();
                });
        } else if (choice == 4) {
            open_dropdown("Prepare links while browsing", {"Off", "1 stream", "2 streams", "3 streams", "4 streams", "5 streams"},
                services_.instant_limit, [this](int i) {
                    auto next = services_; next.instant_limit = i;
                    services_save(next); services_menu();
                });
        } else if (choice == 5) services_import();
    });
}

void Session::service_menu(const std::string& provider) {
    const bool torbox = provider == "torbox";
    std::vector<std::string> options;
    if (torbox) options.push_back("Connect with a QR code");
    options.push_back("Enter an API key");
    options.push_back("Disconnect on this PS5");
    open_dropdown(debrid::provider_name(provider), options, 0, [this, provider, torbox](int choice) {
        if (torbox && choice == 0) { service_login_start(); return; }
        if (choice == (torbox ? 1 : 0)) {
            // Do not prefill an existing credential or echo it in a message.
            say("Enter your " + debrid::provider_name(provider) + " API key from the service's account page.", 8.0);
            const int generation = services_generation_;
            ask_text([this, provider, generation](const std::string& text) {
                const std::string key = trim(text);
                if (key.empty()) return;
                if (!debrid::valid_key(key)) { say("Enter a valid API key.", 6.0, true); return; }
                say("Checking " + debrid::provider_name(provider) + " connection...");
                bg<std::string>([provider, key] { return debrid::Client().validate(provider, key); },
                    [this, provider, key, generation](auto& error) {
                        if (generation != services_generation_) return;
                        if (!error.empty()) { say(error, 8.0, true); return; }
                        auto next = services_;
                        (provider == "torbox" ? next.torbox : next.premiumize) = {key, ""};
                        next.enabled = true;
                        if (services_save(next)) say(debrid::provider_name(provider) + " connected");
                    });
            });
        } else {
            open_dropdown("Disconnect " + debrid::provider_name(provider) + "?", {"Keep connection", "Disconnect"}, 0,
                [this, provider](int i) {
                    if (i != 1) return;
                    auto next = services_;
                    (provider == "torbox" ? next.torbox : next.premiumize) = {};
                    if (services_save(next)) say("Disconnected on this PS5. Your Nuvio connection is unchanged.", 6.0);
                });
        }
    });
}

void Session::services_import() {
    if (!signed_in()) { say("Sign in to Nuvio first to import Connected Services.", 7.0); return; }
    if (services_importing_) return;
    services_importing_ = true;
    const int generation = services_generation_, login = login_gen_;
    say("Importing Connected Services from your selected Nuvio profile...");
    bg<ApiResult>([key = prefs_.auth_key] { return api_connected_services_pull(key); }, [this, generation, login](auto& result) {
        services_importing_ = false;
        if (generation != services_generation_ || login != login_gen_ || !signed_in()) return;
        if (!result.ok) { say(result.error, 8.0, true); return; }
        auto next = services_;
        if (!debrid::import_snapshot(next, result.result)) { say("Could not read the synced connections.", 7.0, true); return; }
        const auto& rows = jobj(result.result, "credentials");
        if (rows.empty()) {
            say("No synced TorBox or Premiumize connection found. Connect it in Nuvio and sync, or connect it here.", 10.0);
            return;
        }
        if (!result.result.contains("enabled")) next.enabled = !next.torbox.key.empty() || !next.premiumize.key.empty();
        if (services_save(next)) say(next.enabled ? "Connections imported. Cached links can now resolve here." : "Connections imported. Resolve playable links is off.", 7.0);
    });
}

void Session::service_login_start() {
    if (device_cancel_) *device_cancel_ = true;
    const int gen = ++login_gen_;
    device_provider_ = "torbox"; device_code_ = {};
    device_cancel_ = std::make_shared<std::atomic<bool>>(false);
    const auto cancel = device_cancel_;
    login_polling_ = false;
    sign_in = SignInState(); sign_in.title = "Connect TorBox"; sign_in.cancel_label = "Cancel";
    sign_in.link = "torbox.app"; sign_in.status = "Loading sign-in code..."; sign_in_open = true;
    bg<debrid::DeviceCode>([cancel] { return debrid::Client().start_torbox(cancel.get()); }, [this, gen](auto& code) {
        if (gen != login_gen_ || !sign_in_open || device_provider_.empty()) return;
        if (!code.ok) { sign_in.status = code.error; return; }
        device_code_ = code; sign_in.code = code.user_code;
        sign_in.link = code.url.substr(8);
        std::uint8_t temp[qrcodegen_BUFFER_LEN_MAX], qr[qrcodegen_BUFFER_LEN_MAX];
        if (qrcodegen_encodeText(code.link.c_str(), temp, qr, qrcodegen_Ecc_MEDIUM, qrcodegen_VERSION_MIN, 20,
                                qrcodegen_Mask_AUTO, true)) {
            sign_in.qr_size = qrcodegen_getSize(qr);
            sign_in.qr_modules.resize(sign_in.qr_size * sign_in.qr_size);
            for (int y = 0; y < sign_in.qr_size; ++y) for (int x = 0; x < sign_in.qr_size; ++x)
                sign_in.qr_modules[y * sign_in.qr_size + x] = qrcodegen_getModule(qr, x, y);
        }
        login_interval_ = code.interval; login_next_poll_ = now_seconds() + code.interval;
        login_expires_ = now_seconds() + code.lifetime;
        sign_in.status = "Waiting for approval on your phone...";
    });
}

void Session::service_login_poll() {
    if (device_code_.device_code.empty() || login_polling_ || now_seconds() < login_next_poll_) return;
    if (now_seconds() >= login_expires_) {
        sign_in.status = "Code expired. Close this and connect again."; device_code_.device_code.clear(); return;
    }
    login_polling_ = true;
    const int gen = login_gen_;
    const auto code = device_code_.device_code;
    const auto cancel = device_cancel_;
    bg<debrid::DeviceToken>([code, cancel] { return debrid::Client().poll_torbox(code, cancel.get()); }, [this, gen](auto& token) {
        if (gen != login_gen_ || !sign_in_open || device_provider_.empty()) return;
        login_polling_ = false; login_next_poll_ = now_seconds() + login_interval_;
        if (token.state == debrid::DeviceToken::authorized) {
            auto next = services_; next.torbox = {token.key, ""}; next.enabled = true;
            if (services_save(next)) { sign_in_cancel(); say("TorBox connected"); }
            else { sign_in.status = "Could not save connection. Close this and try again."; device_code_.device_code.clear(); }
        } else if (token.state == debrid::DeviceToken::expired) {
            sign_in.status = "Code expired. Close this and connect again."; device_code_.device_code.clear();
        } else if (token.state == debrid::DeviceToken::failed) {
            sign_in.status = token.error; device_code_.device_code.clear();
        }
    });
}
} // namespace sx
