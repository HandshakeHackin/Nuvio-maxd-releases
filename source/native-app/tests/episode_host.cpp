#include "episode_playback.hpp"
#include <cmath>
#include <cstdio>
#include <functional>
#include <limits>
#include <stdexcept>
using namespace sx::episode_playback;
namespace {
void expect(bool value) { if (!value) throw std::runtime_error("unexpected episode behavior"); }
::Stream stream(const char* text = "4K HEVC HDR10 Atmos 5.1", const char* group = "release") {
    ::Stream s; s.name = text; s.url = "https://example.invalid/episode"; s.addon = "AIOStreams"; s.binge_group = group; return s;
}
Flow credits() { Flow f; f.segments = {{Kind::credits, 900, 1000}}; return f; }
void tick(Flow& f, double pos=950, bool advancing=true, bool autoplay=true, bool ready=true) {
    f.update(pos,1000,0.25,advancing,true,autoplay,ready);
}
void finish(Flow& f) { for(int i=0;i<42;++i) tick(f); }
// Shapes taken from live ARM (Attack on Titan) and AniSkip (MAL 16498, episode 1) responses.
json arm_titan() {
    auto e=[](long mal,long anilist,int season,const char* media){return json{{"myanimelist",mal},{"anilist",anilist},{"thetvdb-season",season},{"media",media}};};
    return json::array({e(16498,16498,1,"TV"),e(18397,18397,0,"OVA"),e(25777,20958,2,"TV"),e(35760,99147,3,"TV"),e(38524,104578,3,"TV"),
                        e(40028,110277,4,"TV"),e(48583,131681,4,"TV"),e(51535,146984,4,"SPECIAL")});
}
json aniskip_titan() {
    auto r=[](const char* type,double a,double b,double length){return json{{"interval",{{"startTime",a},{"endTime",b}}},{"skipType",type},{"episodeLength",length}};};
    return json{{"found",true},{"results",json::array({r("op",47.365,137.365,1540.061),r("ed",1342.795,1430.616,1446.9973),
                                                        r("mixed-op",75,135,1420),r("mixed-ed",24.41,114.41,1464.4)})}};
}
json animeskip_episodes(json episodes) { return json{{"data",{{"findEpisodesByShowId",episodes}}}}; }
json animeskip_episode(const char* number,double base,std::vector<std::pair<double,const char*>> marks) {
    json t=json::array(); for(auto& [at,name]:marks) t.push_back(json{{"at",at},{"type",{{"name",name}}}});
    return json{{"season","1"},{"number",number},{"baseDuration",base},{"timestamps",t}};
}
}
int main() {
    const std::pair<const char*, std::function<void()>> tests[] = {
        {"seconds and milliseconds", [] { const auto s=parse_segments(json{{"intro",{{"start_sec",10},{"end_sec",60}}},{"recap",{{"start_ms",0},{"end_ms",10000}}}}); expect(s.size()==2 && s[0].kind==Kind::recap && s[1].end==60); }},
        {"seconds take precedence", [] { const auto s=parse_segments(json{{"intro",{{"start_sec",5},{"end_sec",15},{"start_ms",9999},{"end_ms",88888}}}}); expect(s.size()==1 && s[0].start==5 && s[0].end==15); }},
        {"bad segment fields", [] { expect(parse_segments(json{{"intro",{{"start_sec",-1},{"end_sec",5}}},{"recap",{{"start_sec",10},{"end_sec",1}}},{"outro",{{"start_sec",nullptr},{"end_sec",100}}}}).empty()); }},
        {"missing and error data", [] { expect(parse_segments(json::array()).empty() && parse_segments(json{{"error","unavailable"}}).empty()); }},
        {"unreasonable bounds", [] { expect(parse_segments(json{{"intro",{{"start_sec",1},{"end_sec",999999}}}}).empty()); }},
        {"IMDb query isolates identity", [] { expect(timing_url({"tt123456:2:4"},2,4)=="https://api.introdb.app/segments?imdb_id=tt123456&season=2&episode=4"); }},
        {"invalid identities and specials", [] { expect(timing_url({"https://secret/token","tt123&token=x","tmdb:123"},1,1).empty() && timing_url({"tt123"},0,1).empty()); }},
        {"metadata IMDb mapping", [] { const auto m=parse_meta(json{{"id","tmdb:42"},{"imdb_id","tt123"}}); expect(timing_url({m.imdb_id,m.id},1,1).find("tt123")!=std::string::npos); }},
        {"movie timing query", [] { expect(timing_url({"tt123"},-1,-1,true)=="https://api.introdb.app/segments?imdb_id=tt123&is_movie=true"); }},
        {"completed episode stays watched after rewind until next playback", [] {
            Flow f; expect(!f.record_completion(400,1000)); expect(f.record_completion(950,1000));
            expect(f.record_completion(100,1000)); f=Flow(); expect(!f.record_completion(100,1000));
            expect(f.record_completion(100,1000,true));
        }},
        {"skip clamps metadata rounding beyond video end", [] {
            Flow f; f.segments={{Kind::intro,10,1000.5}}; tick(f,20); expect(f.skip_target(1000)==1000);
        }},
        {"intro button boundaries", [] { Flow f; f.segments={{Kind::intro,10,60}}; tick(f,9);expect(f.active<0);tick(f,10);expect(f.active==0);tick(f,59.6);expect(f.active<0); }},
        {"recap seeks to correct end", [] { Flow f;f.segments={{Kind::recap,0,40}};tick(f,10);expect(f.skip_target(1000)==40); }},
        {"skip suppression and backward seek", [] { Flow f;f.segments={{Kind::intro,10,60}};tick(f,20);f.skipped();tick(f,20);expect(f.active<0);tick(f,65);tick(f,20);expect(f.active==0); }},
        {"duration mismatch ignored", [] { Flow f;f.segments={{Kind::intro,10,1100}};tick(f,20);expect(!f.skip_target(1000) && !f.next_visible); }},
        {"nonfinite playback ignored", [] { auto f=credits();tick(f,std::numeric_limits<double>::quiet_NaN());expect(!f.next_visible && f.active<0); }},
        {"missing credits do not guess", [] { Flow f;tick(f,999);finish(f);expect(!f.next_visible && !f.launch()); }},
        {"credits countdown launches once ready", [] { auto f=credits();tick(f,899);expect(!f.next_visible);finish(f);expect(f.launch()); }},
        {"paused countdown", [] { auto f=credits();tick(f);for(int i=0;i<80;++i)tick(f,950,false);expect(f.remaining==10 && !f.launch()); }},
        {"autoplay disabled", [] { auto f=credits();for(int i=0;i<80;++i)tick(f,950,true,false);expect(f.next_visible && !f.launch()); }},
        {"waiting for matching stream", [] { auto f=credits();for(int i=0;i<80;++i)tick(f,950,true,true,false);expect(f.remaining==10 && !f.launch()); }},
        {"expired match prevents launch", [] { auto f=credits();finish(f);expect(f.launch());tick(f,950,true,true,false);expect(!f.launch()); }},
        {"cancel survives seek and EOF", [] { auto f=credits();tick(f);f.cancel();tick(f,500);finish(f);expect(f.cancelled && !f.launch()); }},
        {"seek back resets countdown", [] { auto f=credits();for(int i=0;i<20;++i)tick(f);tick(f,500);expect(f.remaining==10 && !f.next_visible); }},
        {"post-credit scene retained", [] { Flow f;f.segments=parse_segments(json{{"outro",{{"start_sec",800},{"end_sec",1000}}},{"post_credits",{{"start_sec",940},{"end_sec",970}}}});tick(f,820);expect(f.skip_target(1000)==940 && !f.next_visible);tick(f,955);expect(f.active<0 && !f.next_visible); }},
        {"autoplay follows post-credit scene", [] { Flow f;f.segments={{Kind::credits,800,900},{Kind::post_credits,940,970}};tick(f,969);expect(!f.next_visible);tick(f,971);expect(f.next_visible); }},
        {"unlabelled tail retained", [] { Flow f;f.segments={{Kind::credits,800,900}};tick(f,850);expect(!f.next_visible && f.skip_target(1000)==900);tick(f,999);expect(f.next_visible); }},
        {"final episode and specials", [] { std::vector<Video> v={{"special","","","","",0,1},{"a","","","","",1,1}};expect(!next_video(v,"a",now_epoch_ms()) && !next_video(v,"missing",now_epoch_ms())); }},
        {"season rollover sorted logically", [] { std::vector<Video> v={{"b","","","","",2,1},{"a","","","","",1,8},{"special","","","","",0,1}};expect(next_video(v,"a",now_epoch_ms())->id=="b"); }},
        {"unaired episode blocks rollover", [] { std::vector<Video> v={{"a","","","","",1,1},{"b","","","","2099-01-01T00:00:00Z",1,2},{"c","","","","2020-01-01T00:00:00Z",1,3}};expect(!next_video(v,"a",now_epoch_ms())); }},
        {"same group preferred", [] { auto other=stream();other.binge_group="other";expect(select_stream({other,stream()},stream())==1); }},
        {"lower quality group rejected", [] { expect(select_stream({stream("1080p HEVC HDR10 Atmos 5.1")},stream())<0); }},
        {"HDR and audio traits preserved", [] { expect(select_stream({stream("4K HEVC SDR 5.1"),stream("4K HEVC HDR10 5.1")},stream())<0); }},
        {"raw torrent not auto selected", [] { auto raw=stream();raw.url.clear();raw.info_hash="abc";expect(select_stream({raw},stream())<0); }},
        {"connected service cached stream can keep next episode quality", [] { auto resolved=stream();resolved.url.clear();resolved.debrid_resolvable=true;expect(select_stream({resolved},stream())==0); }},
        {"same addon quality fallback", [] { expect(select_stream({stream("4K HEVC HDR10 Atmos 5.1","other")},stream())==0); }},
        {"unknown quality requires group", [] { expect(select_stream({stream("Unlabelled","other")},stream("Unlabelled"))<0 && select_stream({stream("Unlabelled")},stream("Unlabelled"))==0); }},
        {"anime identity from Kitsu, MAL and AniList IDs", [] { expect(anime_source({"tt1","kitsu:7442:5"})->source=="kitsu" && anime_source({"kitsu:7442:5"})->id==7442 &&
            anime_source({"mal:16498"})->source=="myanimelist" && anime_source({"anilist:21:3"})->source=="anilist"); }},
        {"invalid anime identities", [] { expect(!anime_source({"kitsu:abc","kitsu:","tt123:1:1","tmdb:5","kitsu:-4"})); }},
        {"ARM lookups carry only catalogue IDs", [] { expect(arm_ids_url({"kitsu",7442})=="https://arm.haglund.dev/api/v2/ids?source=kitsu&id=7442&include=myanimelist,anilist" &&
            arm_imdb_url({"tt2560140:3:1"})=="https://arm.haglund.dev/api/v2/imdb?id=tt2560140&include=myanimelist,anilist,thetvdb-season,media" &&
            arm_imdb_url({"tt12&x=1"}).empty() && arm_ids_url({"imdb",5}).empty()); }},
        {"ARM ID object", [] { const auto ids=parse_arm_ids(json{{"myanimelist",16498},{"anilist",16498}},1); expect(ids.any() && ids.mal==16498 && ids.anilist==16498 && ids.episode==1 &&
            !parse_arm_ids(nullptr,1).any() && !parse_arm_ids(json{{"myanimelist",16498}},0).any()); }},
        {"single-entry TVDB season maps", [] { const auto s2=pick_arm_season(arm_titan(),2,7); expect(pick_arm_season(arm_titan(),1,1).mal==16498 && s2.mal==25777 && s2.anilist==20958 && s2.episode==7); }},
        {"split and special seasons are not guessed", [] { expect(!pick_arm_season(arm_titan(),3,14).any() && !pick_arm_season(arm_titan(),4,1).any() &&
            !pick_arm_season(arm_titan(),0,1).any() && !pick_arm_season(json::array(),1,1).any()); }},
        {"AniSkip query sends the file length", [] { expect(aniskip_url(16498,1,1440.4)=="https://api.aniskip.com/v2/skip-times/16498/1?types=op&types=ed&types=recap&types=mixed-op&types=mixed-ed&episodeLength=1440" &&
            aniskip_url(16498,1,std::numeric_limits<double>::quiet_NaN()).ends_with("episodeLength=0") && aniskip_url(0,1,1440).empty() && aniskip_url(1,0,1440).empty()); }},
        {"AniSkip types and order", [] { auto data=aniskip_titan(); for(auto& row:data["results"])row["episodeLength"]=1440; const auto s=parse_aniskip(data,1440); expect(s.size()==2 && s[0].kind==Kind::intro && s[0].start==47.365 && s[1].kind==Kind::credits && s[1].end==1430.616); }},
        {"AniSkip rejects another release's lengths", [] { const auto s=parse_aniskip(aniskip_titan(),1447); expect(s.size()==1 && s[0].kind==Kind::credits); }},
        {"AniSkip recap and unknown types", [] { const auto s=parse_aniskip(json{{"found",true},{"results",json::array({json{{"interval",{{"startTime",0},{"endTime",30}}},{"skipType","recap"},{"episodeLength",1400}},
            json{{"interval",{{"startTime",40},{"endTime",50}}},{"skipType","preview"}}})}},1400); expect(s.size()==1 && s[0].kind==Kind::recap); }},
        {"AniSkip not found and malformed", [] { expect(parse_aniskip(json{{"found",false},{"results",json::array()}},0).empty() && parse_aniskip(json::array(),0).empty() &&
            parse_aniskip(json{{"found",true},{"results",json::array({json{{"interval",{{"startTime",90},{"endTime",10}}},{"skipType","op"}}})}},0).empty()); }},
        {"Anime-Skip queries cannot be injected", [] { expect(json::parse(animeskip_shows_body(21))["query"]=="{ findShowsByExternalId(service: ANILIST, serviceId: \"21\") { id } }" &&
            animeskip_shows_body(0).empty() && animeskip_episodes_body("x\") { evil }").empty() &&
            json::parse(animeskip_episodes_body("a1b2-c3"))["query"].get<std::string>().find("showId: \"a1b2-c3\"")!=std::string::npos); }},
        {"Anime-Skip show IDs", [] { const auto shows=parse_animeskip_shows(json{{"data",{{"findShowsByExternalId",json::array({json{{"id","a-1"}},json{{"id","bad id"}},json{{"id","b-2"}},json{{"id","c-3"}},json{{"id","d-4"}}})}}}});
            expect(shows.size()==3 && shows[0]=="a-1" && shows[1]=="b-2" && parse_animeskip_shows(json{{"errors",json::array()}}).empty()); }},
        {"Anime-Skip sections run to the next timestamp", [] { const auto s=parse_animeskip(animeskip_episodes(json::array({animeskip_episode("2",1420,{{0,"Canon"}}),
            animeskip_episode("1",1420,{{1300,"Credits"},{0,"Canon"},{90,"Intro"},{180,"Canon"},{1390,"Preview"}})})),1,1425);
            expect(s.size()==2 && s[0].kind==Kind::intro && s[0].start==90 && s[0].end==180 && s[1].kind==Kind::credits && s[1].end==1390); }},
        {"Anime-Skip drops branding a shorter copy lacks", [] { const auto ep=animeskip_episodes(json::array({animeskip_episode("412",1435.1,{{0,"Branding"},{15,"Intro"},{213.1,"Recap"},{264.3,"Title Card"},{1395.1,"Preview"}})}));
            const auto s=parse_animeskip(ep,412,1410); expect(s.size()==2 && s[0].kind==Kind::intro && s[0].start==0 && std::abs(s[0].end-198.1)<1e-9 && s[1].kind==Kind::recap && std::abs(s[1].end-249.3)<1e-9);
            const auto same=parse_animeskip(ep,412,1435.1); expect(same.size()==2 && same[0].start==15 && same[1].end==264.3);
            expect(parse_animeskip(ep,412,1380).empty()); }},
        {"Anime-Skip without branding needs a close length", [] { expect(parse_animeskip(animeskip_episodes(json::array({animeskip_episode("1",1435.1,{{15,"Intro"},{213.1,"Recap"}})})),1,1410).empty()); }},
        {"Anime-Skip final section ends with the episode", [] { const auto s=parse_animeskip(animeskip_episodes(json::array({animeskip_episode("1",1420,{{1300,"Credits"}})})),1,1420);
            expect(s.size()==1 && s[0].end==1420); }},
        {"Anime-Skip ambiguous or mismatched episodes", [] { const auto twice=json::array({animeskip_episode("1",1420,{{90,"Intro"},{180,"Canon"}}),animeskip_episode("1",1420,{{60,"Intro"},{150,"Canon"}})});
            expect(parse_animeskip(animeskip_episodes(twice),1,1420).empty() && parse_animeskip(animeskip_episodes(json::array({animeskip_episode("1",1420,{{90,"Intro"},{180,"Canon"}})})),1,1500).empty() &&
                   parse_animeskip(animeskip_episodes(json::array({animeskip_episode("1",1420,{{90,"Mixed Intro"},{180,"Canon"}})})),1,1420).empty()); }},
        {"earlier sources keep priority when merging", [] { const auto m=merge_segments({{{Kind::intro,10,60},{Kind::credits,800,900},{Kind::post_credits,940,970}},
            {{Kind::intro,20,70},{Kind::recap,0,10},{Kind::credits,810,905}}});
            expect(m.size()==4 && m[0].kind==Kind::recap && m[1].start==10 && m[2].start==800 && m[3].kind==Kind::post_credits); }},
        {"merged credits stop at a post-credit scene", [] { const auto m=merge_segments({{{Kind::post_credits,940,970}},{{Kind::credits,900,1000}}}); expect(m.size()==2 && m[0].end==940); }},
        {"anime episodes keep their IMDb numbering", [] { const auto m=parse_meta(json{{"id","kitsu:1"},{"videos",json::array({json{{"id","kitsu:1:1"},{"season",1},{"episode",1},{"imdbSeason",2},{"imdbEpisode",14}}})}});
            expect(m.videos.size()==1 && m.videos[0].episode==1 && m.videos[0].imdb_season==2 && m.videos[0].imdb_episode==14); }},
        {"out-of-range preferred credits preserve a valid fallback", [] {
            auto ep=animeskip_episodes(json::array({animeskip_episode("1",1420,{{1300,"Credits"}})}));
            auto preferred=parse_animeskip(ep,1,1405);expect(preferred.empty());
            auto merged=merge_segments({{{Kind::credits,1300,1420}},{{Kind::credits,1300,1400}}},1405);
            Flow f;f.segments=merged;f.update(1320,1405,.1,true,false,false,false);
            expect(f.skip_target(1405).has_value() && *f.skip_target(1405)==1400);
        }},
        {"unverified source durations are rejected", [] {
            auto data=aniskip_titan();for(auto& row:data["results"])row.erase("episodeLength");
            expect(parse_aniskip(data,1440).empty() && parse_aniskip(aniskip_titan(),0).empty());
            expect(parse_animeskip(animeskip_episodes(json::array({animeskip_episode("1",0,{{0,"Intro"},{90,"Canon"}})})),1,1440).empty());
        }},
        {"late provider completion keeps priority and fills gaps", [] {
            const std::vector<Segment> introdb{{Kind::intro,10,70},{Kind::recap,0,10}};
            const std::vector<Segment> ani{{Kind::intro,20,80}};
            auto before=merge_segments({{},ani,introdb},1440);expect(before.size()==2 && before[1].start==20);
            auto after=merge_segments({{{Kind::intro,30,90}},ani,introdb},1440);expect(after.size()==2 && after[1].start==30);
            auto failure=merge_segments({{},ani,introdb},1440);expect(failure[1].start==20);
        }},
    };
    int failed=0;
    for(const auto& [name,run]:tests) try {run();} catch(const std::exception& e) {++failed;std::fprintf(stderr,"FAIL (%s): %s\n",name,e.what());}
    std::printf("Episode skip and next playback: %zu tests, %d failures\n",std::size(tests),failed);
    return failed ? 1 : 0;
}
