#include "../ui/i18n.hpp"
#include "qrcodegen.h"
#include "session.hpp"
namespace sx {
void Session::media_plex_cancel() {
  if (device_cancel_)
    *device_cancel_ = true;
  media_plex_pair_ = {};
  media_plex_replace_.clear();
  login_polling_ = false;
}
void Session::media_plex_start(const std::string &replace) {
  if (!profile_ready_) {
    say(sx::i18n::tr("Select a Nuvio profile first"), 6);
    return;
  }
  if (replace.empty() && media_accounts_.size() >= 8) {
    say(sx::i18n::tr("Remove a server connection first (maximum 8)"), 6);
    return;
  }
  if (sign_in_open)
    sign_in_cancel();
  if (device_cancel_)
    *device_cancel_ = true;
  device_cancel_ = std::make_shared<std::atomic<bool>>(false);
  auto cancel = device_cancel_;
  const auto gen = ++login_gen_;
  const auto profile = profile_generation_;
  media_plex_pair_ = {};
  media_plex_replace_ = replace;
  device_provider_ = "plex";
  login_polling_ = false;
  sign_in = {};
  sign_in.title = sx::i18n::tr("Connect Plex");
  sign_in.cancel_label = sx::i18n::tr("Cancel");
  sign_in.link = "plex.tv/link";
  sign_in.status = sx::i18n::tr("Creating a secure device connection…");
  sign_in_open = true;
  auto client = media_plex_client_;
  g_tasks.run<plex::Pair>(
      [client, cancel] { return client.start(cancel.get()); },
      [this, gen, profile, cancel](plex::Pair &p) {
        if (gen != login_gen_ || profile != profile_generation_ ||
            !sign_in_open || device_provider_ != "plex" || cancel->load())
          return;
        if (!p.error.empty()) {
          sign_in.status = p.error;
          return;
        }
        media_plex_pair_ = p;
        sign_in.code = p.code;
        sign_in.status = sx::i18n::tr("Scan the QR code or enter the code at plex.tv/link.");
        std::uint8_t temp[qrcodegen_BUFFER_LEN_MAX],
            qr[qrcodegen_BUFFER_LEN_MAX];
        if (qrcodegen_encodeText(p.url.c_str(), temp, qr, qrcodegen_Ecc_MEDIUM,
                                 qrcodegen_VERSION_MIN, 20, qrcodegen_Mask_AUTO,
                                 true)) {
          sign_in.qr_size = qrcodegen_getSize(qr);
          sign_in.qr_modules.resize(sign_in.qr_size * sign_in.qr_size);
          for (int y = 0; y < sign_in.qr_size; ++y)
            for (int x = 0; x < sign_in.qr_size; ++x)
              sign_in.qr_modules[y * sign_in.qr_size + x] =
                  qrcodegen_getModule(qr, x, y);
        }
        login_expires_ = now_seconds() + p.expires;
        login_next_poll_ = now_seconds() + 2;
      });
}
void Session::media_plex_poll() {
  if (device_provider_ != "plex" || !sign_in_open ||
      media_plex_pair_.id.empty() || login_polling_ ||
      now_seconds() < login_next_poll_)
    return;
  if (now_seconds() >= login_expires_) {
    media_plex_pair_ = {};
    sign_in.code.clear();
    sign_in.qr_modules.clear();
    sign_in.qr_size = 0;
    sign_in.status = sx::i18n::tr("Plex code expired. Close and connect again.");
    return;
  }
  login_polling_ = true;
  const auto gen = login_gen_;
  const auto profile = profile_generation_;
  auto cancel = device_cancel_;
  auto client = media_plex_client_;
  auto pair = media_plex_pair_;
  g_tasks.run<plex::Pair>(
      [client, pair, cancel] { return client.poll(pair, cancel.get()); },
      [this, gen, profile, cancel](plex::Pair &p) {
        if (gen != login_gen_ || profile != profile_generation_ ||
            !sign_in_open || device_provider_ != "plex" || cancel->load())
          return;
        login_polling_ = false;
        if (!p.error.empty()) {
          sign_in.status = p.error;
          media_plex_pair_ = {};
          return;
        }
        if (p.pending) {
          login_next_poll_ = now_seconds() + 2;
          return;
        }
        media_plex_pair_ = p;
        media_plex_servers(p);
      });
}
void Session::media_plex_servers(const plex::Pair &pair) {
  login_polling_ = true;
  const auto gen = login_gen_;
  const auto profile = profile_generation_;
  auto cancel = device_cancel_;
  auto client = media_plex_client_;
  const auto replace = media_plex_replace_;
  sign_in.status = sx::i18n::tr("Finding accessible Plex servers…");
  g_tasks.run<plex::Servers>(
      [client, pair, cancel] { return client.servers(pair, cancel.get()); },
      [this, client, pair, gen, profile, cancel, replace](plex::Servers &r) {
        if (gen != login_gen_ || profile != profile_generation_ ||
            device_provider_ != "plex" || cancel->load())
          return;
        login_polling_ = false;
        if (!r.error.empty()) {
          sign_in.status = r.error;
          media_plex_pair_ = {};
          return;
        }
        sign_in_open = false;
        device_provider_.clear();
        media_plex_pair_ = {};
        media_plex_replace_.clear();
        std::vector<std::string> labels;
        for (const auto &s : r.items)
          labels.push_back(s.name);
        labels.push_back(sx::i18n::tr("Cancel"));
        auto servers = r.items;
        open_dropdown(
            sx::i18n::tr("Choose Plex server"), labels, 0,
            [this, client, pair, servers, profile, gen, cancel,
             replace](int n) {
              if (n < 0 || size_t(n) >= servers.size() ||
                  profile != profile_generation_ || gen != login_gen_ ||
                  cancel->load())
                return;
              if (media_cancel_)
                *media_cancel_ = true;
              media_cancel_ = std::make_shared<std::atomic<bool>>(false);
              auto stop = media_cancel_;
              auto generation = ++media_generation_;
              auto server = servers[size_t(n)];
              media_server::Connection c;
              c.id =
                  "plex-" + pair.device + "-" + std::to_string(now_epoch_ms());
              say(sx::i18n::tr("Connecting to Plex server…"), 8);
              struct Connected {
                media_server::Connection c;
                media_server::Result result;
              };
              g_tasks.run<Connected>(
                  [client, pair, server, c, stop]() mutable {
                    Connected r;
                    r.c = c;
                    r.result = client.connect(r.c, pair, server, stop.get());
                    return r;
                  },
                  [this, profile, generation, gen, stop,
                   replace](Connected &result) {
                    if (profile != profile_generation_ ||
                        generation != media_generation_ || gen != login_gen_ ||
                        stop->load())
                      return;
                    if (!result.result.ok()) {
                      say(result.result.error, 10, true);
                      return;
                    }
                    if (watching && w_item_.type == "media-video" &&
                        !replace.empty() && media_play_account_.id == replace)
                      watch_stop(false);
                    auto existing = std::find_if(
                        media_accounts_.begin(), media_accounts_.end(),
                        [&](const auto &c) { return c.id == replace; });
                    if (existing == media_accounts_.end()) {
                      if (media_accounts_.size() >= 8) {
                        say(sx::i18n::tr("Maximum server connections reached"), 6, true);
                        return;
                      }
                      media_accounts_.push_back(std::move(result.c));
                      media_account_ = media_accounts_.size() - 1;
                    } else {
                      media_account_ =
                          size_t(existing - media_accounts_.begin());
                      *existing = std::move(result.c);
                    }
                    media_location_ = {};
                    media_stack_.clear();
                    media_save();
                    media_reload();
                    goto_ = Page::servers;
                    say(sx::i18n::tr("Plex connected"), 5);
                  });
            });
      });
}
void Session::media_flush_auth() {
  if (!profile_ready_ || now_seconds() < media_auth_save_at_)
    return;
  media_auth_save_at_ = now_seconds() + 10;
  json connections = json::array();
  for (const auto &c : media_accounts_)
    connections.push_back(c.saved());
  if (connections != media_saved_connections_)
    media_save();
}
} // namespace sx
