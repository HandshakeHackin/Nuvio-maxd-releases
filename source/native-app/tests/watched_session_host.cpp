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
 // Only explicitly returned episodes are watched; a mark refresh does not
 // issue a new focus hint, even when the first unwatched episode changes.
 s.d_item_.id="tt456";s.d_item_.type="series";s.d_seasons_={1,2};s.d_meta_.videos.clear();
 for(int season=1;season<=2;++season)for(int n=1;n<=30;++n)s.d_meta_.videos.push_back(Video{"native-"+std::to_string(season)+"-"+std::to_string(n),"Episode","","","2020-01-01",season,n});
 s.watched_.clear();s.detail_set_season(0);const auto serial=s.detail.hint_serial;const int hint=s.detail.episode_hint;
 response.result["entries"]=json::array();for(int n=1;n<=3;++n)response.result["entries"].push_back({{"content_id","tt456"},{"season",1},{"episode",n}});
 s.load_watched();s.complete();assert(s.watched("native-1-3")&&!s.watched("native-1-4")&&!s.watched("native-2-1"));
 assert(s.detail.episodes[2].watched&&!s.detail.episodes[3].watched&&s.detail.hint_serial==serial&&s.detail.episode_hint==hint);
 response.result["entries"].push_back({{"content_id","tt456"},{"season",1},{"episode",4}});s.load_watched();s.complete();
 assert(s.detail.episodes[3].watched&&s.detail.hint_serial==serial&&s.detail.episode_hint==hint);
 s.detail_set_season(1);assert(s.detail.hint_serial==serial+1&&s.detail.season==1&&!s.detail.episodes[0].watched);
 // Remove 1.0.0.22's read-side scope cache without network requests or changes
 // to explicit marks, resume data, unrelated cache values or tracker write queues.
 json disk;assert(load_json(root+"/progress-mdblist.json",disk));
 disk["watchedScopes"]={{"tt456:*",iso8601_to_ms("2026-11-01T00:00:00Z")}};
 disk["watchedExclusions"]={{"tt456:1:1",1}};disk["unrelated"]="keep";assert(save_json(root+"/progress-mdblist.json",disk));
 assert(write_file(root+"/tracking.json","private-queue-sentinel"));
 sx::Session again;again.profile_dir_=root;again.d_item_=s.d_item_;again.d_meta_=s.d_meta_;again.load_progress();
 assert(again.watched("native-1-1")&&!again.watched("native-1-5")&&!again.watched("native-2-1")&&again.progress_["tt123"].time==900);
 assert(load_json(root+"/progress-mdblist.json",disk)&&!disk.contains("watchedScopes")&&!disk.contains("watchedExclusions")&&disk["unrelated"]=="keep");
 std::string untouched;assert(read_file(root+"/tracking.json",untouched)&&untouched=="private-queue-sentinel");
 tracking::selected="nuvio";sx::Session other_source;other_source.profile_dir_=root;other_source.load_progress();assert(other_source.watched_.empty());
 const auto other=root+"/other";std::filesystem::create_directory(other);tracking::selected="mdblist";sx::Session other_profile;other_profile.profile_dir_=other;other_profile.load_progress();assert(other_profile.watched_.empty());
 std::filesystem::remove_all(root);
 std::cout<<"Watched Session: production snapshot/revisit, all movie grids/detail, restart, resume preservation, partial failures, explicit episodes, legacy cache retirement, unchanged focus hints and stale profile/source guards passed\n";
}
