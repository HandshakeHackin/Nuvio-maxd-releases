#include "media_session_boundary.hpp"
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
  auto old_id=s.media_accounts_[0].id;
  s.media_connect("jellyfin",old_id);typed=s.typed;typed(std::string(argv[1])+"/jellyfin");typed=s.typed;typed("alice");typed=s.typed;typed("test-password");g_tasks.drain();assert(s.media_accounts_.size()==1&&s.media_accounts_[0].id!=old_id);
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
  pick(2);
  pick = s.pick;
  pick(6);
  pick = s.pick;
  pick(1);
  g_tasks.drain();
  assert(!s.media_visible && s.media_accounts_.empty());
  s.media_load();
  assert(!s.media_visible);
  std::filesystem::remove_all(s.profile_dir_);
  std::cout << "Production media Session connection, persistence, browse, "
               "cancellation, profile isolation and playback passed\n";
}
