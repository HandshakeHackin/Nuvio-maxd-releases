#include "watched_session_boundary.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>
#include <unistd.h>
ApiResult response{true,{},""};
ApiResult api_watched_get(const std::string& key){assert(key=="profile-key");return response;}
int main() {
 const auto root="/tmp/nuvio-watched-session-"+std::to_string(getpid());std::filesystem::create_directory(root);
 sx::Session s;s.profile_dir_=root;s.prefs_.auth_key="profile-key";s.d_item_.id="tt123";s.d_item_.type="movie";
 sx::Title movie;movie.id="tt123";movie.type="movie";s.discover.items={movie};sx::Row row;row.items={movie};s.search.rows={row};
 response.result={{"synced",true},{"entries",json::array({{{"content_id","tt123"},{"season",nullptr},{"episode",nullptr}}})}};
 s.load_watched();s.complete();assert(s.watched_.count("tt123")&&s.detail.title.watched&&s.discover.items[0].watched&&s.search.rows[0].items[0].watched);
 assert(s.board.rows[0].items[0].watched&&s.library.items[0].watched);
 s.detail.title.watched=false;s.detail_fill_title();assert(s.detail.title.watched);
 // A saved snapshot survives restart and a detail revisit; resume is independent.
 sx::Progress progress;progress.time=900;progress.duration=3600;progress.video_id="tt123";s.progress_["tt123"]=progress;s.save_progress();
 sx::Session restored;restored.profile_dir_=root;restored.load_progress();assert(restored.watched_.count("tt123")&&restored.progress_["tt123"].time==900);
 response.result["entries"]=json::array();s.load_watched();s.complete();assert(!s.detail.title.watched&&!s.discover.items[0].watched&&!s.search.rows[0].items[0].watched);
 assert(s.progress_["tt123"].time==900); // Unwatched never erases resume.
 s.watched_.insert("keep");response.ok=false;response.error="offline";s.load_watched();s.complete();assert(s.watched_.count("keep"));
 response.ok=true;response.result["entries"]=json::array({{{"season",1}}});s.load_watched();s.complete();assert(s.watched_.count("keep"));
 response.result["entries"]=json::array({{{"content_id","stale"}}});s.load_watched();++s.watched_revision_;s.complete();assert(!s.watched_.count("stale"));
 s.load_watched();++s.profile_generation_;s.complete();assert(!s.watched_.count("stale"));
 // Season/show scopes use canonical aired coordinates, including alternate addon video IDs.
 const auto cutoff=iso8601_to_ms("2025-10-15T20:00:00Z");
 response.result["entries"]=json::array({{{"content_id","tt456"},{"scope","season"},{"season",1},{"episode",nullptr},{"watched_at",cutoff}}});
 s.d_item_.id="tt456";s.d_item_.type="series";
 s.d_meta_.videos={Video{"custom","One","","","2025-01-01",1,1},Video{"later","Two","","","2026-01-01",1,2},Video{"unknown","Three","","","",1,3}};
 s.load_watched();s.complete();assert(s.watched("custom")&&s.watched("tt456:1:1")&&!s.watched("later")&&!s.watched("unknown"));
 s.watched_exclusions_["tt456:1:1"]=cutoff+1000;s.save_progress();assert(!s.watched("custom"));
 sx::Session again;again.profile_dir_=root;again.d_item_=s.d_item_;again.d_meta_=s.d_meta_;again.load_progress();assert(!again.watched("custom")&&!again.watched_scopes_.empty());
 s.load_watched();s.complete();assert(!s.watched("custom")); // Same broad receipt cannot resurrect an explicit unmark.
 tracking::selected="nuvio";sx::Session other_source;other_source.profile_dir_=root;other_source.load_progress();assert(other_source.watched_.empty()&&other_source.watched_scopes_.empty());
 const auto other=root+"/other";std::filesystem::create_directory(other);tracking::selected="mdblist";sx::Session other_profile;other_profile.profile_dir_=other;other_profile.load_progress();assert(other_profile.watched_.empty()&&other_profile.watched_scopes_.empty());
 std::filesystem::remove_all(root);
 std::cout<<"Watched Session: production snapshot/revisit, all movie grids/detail, restart, resume preservation, partial failures, canonical scopes and stale profile/source guards passed\n";
}
