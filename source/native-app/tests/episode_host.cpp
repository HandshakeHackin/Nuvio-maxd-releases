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
    };
    int failed=0;
    for(const auto& [name,run]:tests) try {run();} catch(const std::exception& e) {++failed;std::fprintf(stderr,"FAIL (%s): %s\n",name,e.what());}
    std::printf("Episode skip and next playback: %zu tests, %d failures\n",std::size(tests),failed);
    return failed ? 1 : 0;
}
