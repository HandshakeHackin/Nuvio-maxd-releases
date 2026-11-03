#include "media_session_boundary.hpp"
#include "plex_fixture.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>
#include <sys/stat.h>
using namespace sx;
int main(int argc, char **argv) {
  assert(argc == 2);
  http_init("");
  Session s;
  s.profile_dir_ = "/tmp/maxd-media-session-" + std::to_string(now_epoch_ms());
  make_dirs(s.profile_dir_);
  s.media_load();
  assert(!s.media_visible);
  // Exercise the production three-step connect callback and private
  // persistence.
  s.media_connect("jellyfin");
  auto typed = s.typed;
  typed(std::string(argv[1]) + "/jellyfin");
  typed = s.typed;
  typed("alice");
  typed = s.typed;
  typed("test-password");
  g_tasks.drain();
  assert(s.media_visible && s.media_accounts_.size() == 1 &&
         s.media_items_.size() == 1);
  struct stat st{};
  assert(stat((s.profile_dir_ + "/media-servers.json").c_str(), &st) == 0 &&
         (st.st_mode & 0777) == 0600);
  auto saved =
      iptv::load_private(s.profile_dir_ + "/media-servers.json").dump();
  assert(saved.find("test-password") == std::string::npos);
  auto old_id = s.media_accounts_[0].id;
  s.media_connect("jellyfin", old_id);
  typed = s.typed;
  typed(std::string(argv[1]) + "/jellyfin");
  typed = s.typed;
  typed("alice");
  typed = s.typed;
  typed("test-password");
  g_tasks.drain();
  assert(s.media_accounts_.size() == 1 && s.media_accounts_[0].id != old_id);
  // Browse series and return to the exact parent context.
  s.media_items_ = json::array(
      {{{"Id", "series-1"}, {"Name", "Series"}, {"Type", "Series"}}});
  s.media_activate(0);
  assert(s.media_location_.type == "episodes");
  g_tasks.drain();
  assert(s.media_items_[0]["IndexNumber"] == 2);
  assert(s.media_back());
  g_tasks.drain();
  // Cancelled and previous-profile requests cannot replace the active library.
  s.media_reload();
  g_tasks.one_work();
  auto previous = s.profile_generation_;
  ++s.profile_generation_;
  s.media_items_ = json::array({{{"Id", "new-profile"}}});
  g_tasks.one_ready();
  assert(s.media_items_[0]["Id"] == "new-profile");
  s.profile_generation_ = previous;
  s.media_reload();
  g_tasks.drain();
  // Direct play stays separate from addon detail and history identities.
  s.media_items_ =
      json::array({{{"Id", "movie-1"},
                    {"Name", "Movie"},
                    {"Type", "Movie"},
                    {"UserData", {{"PlaybackPositionTicks", 250000000}}}}});
  s.media_activate(0);
  auto pick = s.pick;
  pick(0);
  assert(s.media_play_pending_);
  g_tasks.drain();
  assert(s.plays == 1 && s.w_item_.type == "media-video" &&
         s.media_resume_ == 25);
  assert(s.last_stream.url.find("Static=true") != std::string::npos);
  s.watching = false;
  // Cancelling during negotiation and switching profile suppress stale
  // launches.
  s.media_play(s.media_items_[0]);
  assert(s.media_back());
  g_tasks.drain();
  assert(s.plays == 1);
  s.media_play(s.media_items_[0]);
  g_tasks.one_work();
  s.media_reset();
  ++s.profile_generation_;
  g_tasks.one_ready();
  assert(s.plays == 1 && !s.media_visible && s.media_items_.empty());
  // Reopen only this profile's persisted accounts; remove requires
  // confirmation.
  s.media_load();
  assert(s.media_visible);
  s.media_menu();
  pick = s.pick;
  pick(3);
  pick = s.pick;
  pick(6);
  pick = s.pick;
  pick(1);
  g_tasks.drain();
  assert(!s.media_visible && s.media_accounts_.empty());
  s.media_load();
  assert(!s.media_visible);
  // Drive production Plex PIN, pending poll, approval, server selection and
  // persistence.
  PlexFixture plex_fixture;
  s.media_plex_client_ = plex_fixture.client();
  s.media_connect("plex");
  g_tasks.drain();
  assert(s.sign_in_open && s.sign_in.code == "ABCD" && s.sign_in.qr_size > 0);
  assert(s.media_accounts_.empty());
  s.login_next_poll_ = 0;
  s.media_plex_poll();
  g_tasks.drain();
  assert(s.sign_in_open && s.media_accounts_.empty());
  plex_fixture.approved = true;
  s.login_next_poll_ = 0;
  s.media_plex_poll();
  g_tasks.drain();
  assert(!s.sign_in_open && s.choices[0] == "Test Plex" &&
         s.media_accounts_.empty());
  // Connecting a new media server must not interrupt ordinary addon playback.
  s.watching = true;
  s.w_item_.type = "movie";
  auto stops = s.stops;
  auto choose = s.pick;
  choose(0);
  g_tasks.drain();
  assert(s.stops == stops && s.media_accounts_.size() == 1 && s.media_visible);
  assert(s.media_accounts_[0].kind == "plex" &&
         s.media_items_[0]["Id"] == "lib-1");
  s.watching = false;
  s.media_activate(0);
  g_tasks.drain();
  assert(s.media_items_[0]["Id"] == "42");
  auto before_plex_play = s.plays;
  s.media_activate(0);
  choose = s.pick;
  choose(0);
  g_tasks.drain();
  assert(s.plays == before_plex_play + 1 && s.last_stream.addon == "Plex" &&
         s.w_item_.type == "media-video");
  s.watching = false;
  auto plex_account = s.media_accounts_[0];
  assert(s.media_plex_client_.browse(plex_account, "", "views", "", 0).ok());
  plex_fixture.reject = true;
  assert(s.media_plex_client_.browse(plex_account, "", "views", "", 0).ok());
  s.media_auth_save_at_ = 0;
  s.media_flush_auth();
  auto private_saved =
      iptv::load_private(s.profile_dir_ + "/media-servers.json");
  assert(private_saved["connections"][0]["token"] == "new-server-token");
  s.media_load();
  assert(s.media_accounts_[0].auth["accountToken"] == "new-account-token");
  // A canceled PIN and a previous profile cannot reopen a pairing modal or add
  // an account.
  s.media_connect("plex");
  s.sign_in_cancel();
  g_tasks.drain();
  assert(!s.sign_in_open && s.media_accounts_.size() == 1);
  s.media_connect("plex");
  g_tasks.one_work();
  s.media_reset();
  ++s.profile_generation_;
  g_tasks.one_ready();
  assert(!s.sign_in_open && s.media_accounts_.empty() &&
         s.media_plex_pair_.key.empty());
  std::filesystem::remove_all(s.profile_dir_);
  std::cout << "Production media Session connection, persistence, browse, "
               "cancellation, profile isolation and playback passed\n";
}
