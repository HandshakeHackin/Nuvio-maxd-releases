// Native tracker account and source controls. No tokens are displayed or logged.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "session.hpp"
#include "nuvio_account.h"
#include "tracking.h"
#include "qrcodegen.h"

namespace sx {
void Session::tracking_refresh() {
    if (!signed_in() || !profile_ready_) return;
    const auto key = prefs_.auth_key; const int gen = tracking_generation_;
    bg<ApiResult>([key] { return api_tracking(key, "status"); }, [this, gen](auto& result) {
        if (gen != tracking_generation_ || !result.ok) return;
        tracking_status_ = result.result; refresh_settings();
    });
}
void Session::tracking_action(const std::string& action, const json& args, bool reload) {
    if (!signed_in() || !profile_ready_) { say("Sign in and choose a Nuvio profile first", 7.0); return; }
    const auto key = prefs_.auth_key;
    const int gen = ++tracking_generation_;
    say("Updating tracking settings...");
    bg<ApiResult>([key, action, args] { return api_tracking(key, action, args); }, [this, gen, reload](auto& result) {
        if (gen != tracking_generation_) return;
        if (!result.ok) { say(result.error, 10.0, true); tracking_refresh(); return; }
        tracking_status_ = result.result; refresh_settings();
        if (reload) {
            // A source switch must not keep resume times or next episodes from
            // the previous service in the shared presentation model.
            progress_.clear(); library_.clear(); library_loaded_ = false; library_loading_ = false; ++library_generation_;
            ++watched_revision_; watched_.clear(); load_progress();
            if (d_open_) close_detail();
            refresh_board(); refresh_library(); load_library();
        }
        say("Tracking settings saved");
    });
}
void Session::tracking_menu() {
    if (!signed_in() || !profile_ready_) { say("Sign in and choose a Nuvio profile first", 7.0); return; }
    const auto key = prefs_.auth_key;
    bg<ApiResult>([key] { return api_tracking(key, "status"); }, [this](auto& result) {
        if (!result.ok) { say(result.error, 8.0, true); return; }
        tracking_status_ = result.result; refresh_settings();
        std::vector<std::string> rows;
        for (auto p : {"trakt", "simkl", "mdblist"}) {
            const auto& account = jobj(jobj(tracking_status_, "accounts"), p);
            rows.push_back(tracking::name(p)+": "+(jbool(account,"connected") ? "Connected" : jbool(account,"reconnect") ? "Reconnect required" : "Not connected"));
        }
        rows.push_back("Library Source: "+(jstr(tracking_status_,"library","nuvio") == "nuvio" ? "Nuvio Library" : tracking::name(jstr(tracking_status_,"library"))));
        rows.push_back("Watch Progress: "+(jstr(tracking_status_,"progress","nuvio") == "nuvio" ? "Nuvio Sync" : tracking::name(jstr(tracking_status_,"progress"))));
        open_dropdown("Tracking", rows, 0, [this](int i) {
            if (i >= 0 && i < 3) tracker_menu(i == 0 ? "trakt" : i == 1 ? "simkl" : "mdblist");
            else if (i == 3 || i == 4) tracking_source_menu(i == 4);
        });
        dropdown.message = "Connections and source choices are separate for each profile. Nuvio remains the default.";
    });
}
void Session::tracking_source_menu(bool progress) {
    if (!signed_in() || !profile_ready_) { say("Sign in and choose a Nuvio profile first", 7.0); return; }
    if (watching || resuming_) { say("Stop playback before changing tracking sources", 7.0); return; }
    const auto key = prefs_.auth_key;
    bg<ApiResult>([key] { return api_tracking(key,"status"); }, [this, progress](auto& result) {
        if (!result.ok) { say(result.error, 8.0, true); return; }
        tracking_status_ = result.result;
        std::vector<std::string> ids{"nuvio"}, labels{progress ? "Nuvio Sync (default)" : "Nuvio Library (default)"};
        for (auto p : {"trakt", "simkl", "mdblist"}) if (jbool(jobj(jobj(tracking_status_,"accounts"),p),"connected")) {
            ids.push_back(p); labels.push_back(tracking::name(p));
        }
        int active = 0;
        for (size_t i = 0; i < ids.size(); ++i) if (ids[i] == jstr(tracking_status_, progress ? "progress" : "library")) active = int(i);
        open_dropdown(progress ? "Watch Progress" : "Library Source", labels, active, [this, ids, progress](int i) {
            if (i < 0 || size_t(i) >= ids.size() || watching || resuming_) return;
            tracking_action("source", {{"field", progress ? "progress" : "library"}, {"value", ids[i]}}, true);
        });
        dropdown.message = progress ? "Choose which progress source powers resume and Continue Watching. New progress is sent only to this source."
            : "Choose which source Nuvio reads for your Library. Trakt and MDBList read your watchlist; SIMKL reads your tracked library.";
    });
}
void Session::tracker_menu(const std::string& p) {
    if (watching || resuming_) { say("Stop playback before changing tracker connections",7.0); return; }
    const bool configured=jbool(jobj(jobj(tracking_status_,"accounts"),p.c_str()),"configured");
    std::vector<std::string> rows{p == "mdblist" ? "Connect with API key" : configured ? "Connect with phone / QR code" : "Phone pairing: awaiting app registration", "Import connection from Nuvio Sync"};
    if (p != "mdblist") rows.push_back("Set up application credentials");
    rows.push_back("Disconnect on this PS5");
    open_dropdown(tracking::name(p), rows, 0, [this,p](int i) {
        if (i == 0) {
            if (p != "mdblist") { tracker_login_start(p); return; }
            say("Enter your MDBList API key from mdblist.com/preferences. Do not enter the page URL.", 10.0);
            ask_text([this,p](const auto& text) { if (!trim(text).empty()) tracking_action("key",{{"provider",p},{"api_key",trim(text)}},true); });
        } else if (i == 1) tracking_action("import_sync",{{"provider",p}},true);
        else if (i == 2 && p != "mdblist") {
            say("Enter the client ID from your "+tracking::name(p)+" developer application", 10.0);
            ask_text([this,p](const auto& id) {
                if (trim(id).empty()) return;
                if (p == "simkl") { tracking_action("configure",{{"provider",p},{"client_id",trim(id)}}); return; }
                say("Enter the Trakt application's client secret. Do not share it in logs or screenshots.", 10.0);
                ask_text([this,p,id](const auto& secret) {
                    if (!trim(secret).empty()) tracking_action("configure",{{"provider",p},{"client_id",trim(id)},{"client_secret",trim(secret)}});
                });
            });
        } else if (i == (p == "mdblist" ? 2 : 3)) {
            if (watching || resuming_) { say("Stop playback before disconnecting a tracker",7.0); return; }
            open_dropdown("Disconnect "+tracking::name(p)+"?",{"Keep connection","Disconnect"},0,[this,p](int choice) {
                if (choice == 1) tracking_action("disconnect",{{"provider",p}},true);
            });
            dropdown.message = "Disconnect on this PS5 and clear its pending tracker updates. Sources return to Nuvio; other devices are unchanged.";
        }
    });
    dropdown.message = p == "mdblist" ? "Connect using your personal MDBList API key. Import is available when your Nuvio profile syncs that key."
        : configured ? "Use your regular tracker account on your phone. You do not need your own developer application. Import requires a compatible Nuvio Sync connection."
        : "This build is awaiting the developer's application registration. Regular users do not need API credentials. Nuvio Library and Nuvio Sync remain available; compatible existing connections can be imported.";
}
void Session::tracker_login_start(const std::string& p) {
    if (watching || resuming_) { say("Stop playback before connecting a tracker",7.0); return; }
    dlog("tracking: %s phone pairing requested; application configured=%d",p.c_str(),int(jbool(jobj(jobj(tracking_status_,"accounts"),p.c_str()),"configured")));
    const auto key = prefs_.auth_key; const int gen = ++login_gen_;
    if (device_cancel_) *device_cancel_=true;
    device_cancel_=std::make_shared<std::atomic<bool>>(false);
    const auto cancel=device_cancel_;
    device_provider_ = "tracking:"+p; device_code_ = {}; login_polling_ = false;
    sign_in = SignInState(); sign_in.title = "Connect "+tracking::name(p); sign_in.cancel_label = "Cancel";
    sign_in.status = "Getting a connection code..."; sign_in_open = true;
    bg<ApiResult>([key,p,cancel] { return api_tracking(key,"device_start",{{"provider",p}},cancel.get()); },[this,gen](auto& result) {
        if (gen != login_gen_ || !sign_in_open) return;
        if (!result.ok) { sign_in.status = result.error; return; }
        sign_in.code = jstr(result.result,"code"); const auto url = jstr(result.result,"url"); sign_in.link = url.substr(8);
        device_code_.device_code = "active";
        std::uint8_t temp[qrcodegen_BUFFER_LEN_MAX], qr[qrcodegen_BUFFER_LEN_MAX];
        if (qrcodegen_encodeText(url.c_str(),temp,qr,qrcodegen_Ecc_MEDIUM,qrcodegen_VERSION_MIN,20,qrcodegen_Mask_AUTO,true)) {
            sign_in.qr_size = qrcodegen_getSize(qr); sign_in.qr_modules.resize(sign_in.qr_size*sign_in.qr_size);
            for (int y=0;y<sign_in.qr_size;++y) for (int x=0;x<sign_in.qr_size;++x)
                sign_in.qr_modules[y*sign_in.qr_size+x] = qrcodegen_getModule(qr,x,y);
        }
        login_interval_ = int(jnum(result.result,"interval",5)); login_next_poll_ = now_seconds()+login_interval_;
        login_expires_ = now_seconds()+jnum(result.result,"expires_in"); sign_in.status = "Enter this code on your phone to approve.";
    });
}
void Session::tracker_login_poll() {
    if (device_code_.device_code.empty() || login_polling_ || now_seconds() < login_next_poll_) return;
    if (now_seconds() >= login_expires_) { sign_in.status = "Code expired. Close and connect again."; device_code_ = {}; return; }
    const auto p = device_provider_.substr(9), key = prefs_.auth_key; const int gen = login_gen_;
    login_polling_ = true;
    const auto cancel=device_cancel_;
    bg<ApiResult>([key,p,cancel] { return api_tracking(key,"device_poll",{{"provider",p}},cancel.get()); },[this,gen,p](auto& result) {
        if (gen != login_gen_ || !sign_in_open) return;
        login_polling_ = false;
        if (!result.ok) { sign_in.status = result.error; device_code_ = {}; return; }
        if (jbool(result.result,"pending")) {
            if (jbool(result.result,"slow")) login_interval_ = std::min(60,login_interval_+5);
            login_next_poll_ = now_seconds()+login_interval_; return;
        }
        tracking_status_ = result.result; sign_in_cancel(); refresh_settings();
        say(tracking::name(p)+" connected. Choose it under Library Source or Watch Progress.",10.0);
    });
}
}
