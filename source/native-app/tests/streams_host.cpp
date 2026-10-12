// Exercise real addon presentation and controller input; no network or PS5 APIs.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "detail.hpp"
#include "stream_display.hpp"
#include "stream_json.hpp"
#include <cstdio>
#include <stdexcept>

namespace {
void expect(bool ok, const char *reason) { if (!ok) throw std::runtime_error(reason); }
bool badge(const sx::Stream &s, sx::StreamBadge b) { return std::find(s.badges.begin(),s.badges.end(),b) != s.badges.end(); }
struct Fixture {
    hui::ui::Fonts fonts; sx::Icons icons; sx::Env env{fonts,icons};
    sx::DetailContent content; sx::Detail page{env};
    Fixture() {
        content.stream_addons = {"AIOStreams","Pengu","Empty"};
        for (int i = 0; i < 4; ++i) {
            sx::Stream s; s.identity=std::to_string(i); s.addon=i%2 ? "Pengu" : "AIOStreams";
            content.streams.push_back(s);
        }
        page.set_content(&content); page.enter();
    }
    void move(hui::Direction nav) { hui::InputFrame in; in.nav=nav; page.update(in,1.0f/60); }
    void press(hui::Action action) { hui::InputFrame in; in.pressed=hui::action_bit(action); page.update(in,1.0f/60); }
};
void aio_card_preserves_badges() {
    ::Stream s; s.addon="AIOStreams"; s.name="𝐔𝐇𝐃⁴ᴷ WEB-DL [TB]";
    s.description="💾 14 GB 🛰️ LA\nDolby Vision HDR HEVC Dolby Digital Plus 5.1";
    s.filename="Title.2160p.WEB-DL.DV.HDR.HEVC.DDP5.1.mkv"; s.url="https://fixture.invalid/debrid";
    const auto v=sx::stream_display::view_of(s); using B=sx::StreamBadge;
    expect(v.name=="UHD⁴ᴷ WEB-DL [TB]", "formatter title lost or unreadable");
    expect(v.size=="14 GB" && v.language=="LA" && v.addon==s.addon,"size, language or provider lost");
    expect(badge(v,B::web_dl) && badge(v,B::uhd) && badge(v,B::dolby_vision) && badge(v,B::hdr) &&
           badge(v,B::hevc) && badge(v,B::dolby_plus) && badge(v,B::channels_51),"expected video/audio badge missing");
    expect(!badge(v,B::sdr) && v.seeds=="Direct / debrid","incorrect SDR/torrent label");
}
void filename_and_hint_metadata() {
    const auto parsed=parse_streams(json::parse(R"({"streams":[{"url":"https://fixture.invalid/video","name":"Source","behaviorHints":{"filename":"Movie.1080p.H.264.SDR.DDP5.1.MULTI.mkv","videoSize":14000000000}}]})"),"Pengu");
    expect(parsed.size()==1,"standard addon stream rejected");
    const auto restored=stream_from_json(stream_to_json(parsed[0]));
    const auto v=sx::stream_display::view_of(restored); using B=sx::StreamBadge;
    expect(v.size=="14.0 GB" && v.language=="MULTI","standard size hint or saved metadata lost");
    expect(badge(v,B::full_hd) && badge(v,B::avc) && badge(v,B::sdr) && badge(v,B::dolby_plus),"filename badges lost");
    ::Stream a; a.addon="AIOStreams";
    a.client_resolve={{"service","torbox"},{"infoHash",std::string(40,'a')},{"fileIdx",0}};
    auto b=a; b.client_resolve["fileIdx"]=1;
    expect(sx::stream_display::view_of(a).identity != sx::stream_display::view_of(b).identity,"instant candidates share a UI identity");
    b=a; b.client_resolve["infoHash"]=std::string(40,'b');
    expect(sx::stream_display::view_of(a).identity != sx::stream_display::view_of(b).identity,"different hashes share a UI identity");
}
void no_invented_metadata() {
    ::Stream s; s.name="DVD documentary"; s.description="Unlabeled source"; s.url="https://fixture.invalid/direct";
    const auto v=sx::stream_display::view_of(s);
    expect(v.badges.empty() && v.size.empty() && v.language.empty(),"unknown metadata fabricated");
    expect(!badge(v,sx::StreamBadge::dolby_vision),"DVD misidentified as Dolby Vision");
}
void flags_and_hdr_plus() {
    ::Stream s; s.name="2160p WEBRip HDR10+ AV1 DTS-HD 7.1"; s.description="💾 7.1GB 🇺🇸 🇪🇸";
    const auto v=sx::stream_display::view_of(s); using B=sx::StreamBadge;
    expect(v.language=="US / ES" && v.size=="7.1 GB","flag language or compact size lost");
    expect(badge(v,B::hdr10_plus) && !badge(v,B::hdr) && badge(v,B::av1) && badge(v,B::dts_hd),"HDR/audio precedence incorrect");
}
void filtered_index_maps_to_source() {
    Fixture f;
    f.press(hui::Action::page_next);
    expect(f.page.stream_addon()=="AIOStreams","preparation uses a different addon filter");
    expect(f.page.stream_index()==0,"first AIO source index changed");
    f.move(hui::Direction::down); expect(f.page.stream_index()==2,"second AIO card plays different addon");
    f.press(hui::Action::page_next); expect(f.page.stream_index()==1,"Pengu filter plays AIO stream");
    f.move(hui::Direction::down); expect(f.page.stream_index()==3,"second Pengu stream mapped incorrectly");
}
void pills_never_start_playback() {
    Fixture f; f.move(hui::Direction::up);
    expect(f.page.filters_focused() && f.page.stream_index()==-1,"filter focus can start a video");
    f.move(hui::Direction::right); f.press(hui::Action::confirm);
    expect(f.page.addon_filter()==1 && f.page.stream_index()==-1,"Cross on addon pill starts playback");
    f.move(hui::Direction::down); expect(f.page.stream_index()==0,"cannot enter filtered cards");
    f.move(hui::Direction::up); f.move(hui::Direction::left); f.move(hui::Direction::left);
    expect(f.page.reload_focused() && f.page.stream_index()==-1,"reload button can play a stream");
}
void empty_filter_and_late_arrival() {
    Fixture f; for (int i=0;i<3;++i) f.press(hui::Action::page_next);
    expect(f.page.filters_focused() && f.page.stream_index()==-1,"empty addon produces playable index");
    sx::Stream s; s.addon="Empty"; s.identity="new"; f.content.streams.push_back(s); f.page.refresh();
    expect(f.page.addon_filter()==3 && f.page.stream_index()==-1,"late addon steals filter focus");
    f.move(hui::Direction::down); expect(f.page.stream_index()==4,"late addon stream not selectable");
}
void arrival_keeps_selected_source() {
    Fixture f; f.move(hui::Direction::down); expect(f.page.stream_index()==1,"initial selection wrong");
    sx::Stream s; s.identity="inserted"; s.addon="AIOStreams";
    f.content.streams.insert(f.content.streams.begin(),s); f.page.refresh();
    expect(f.page.stream_index()==2,"new addon response changes selected release");
    f.content.streams[2].prepared = true; f.page.refresh();
    expect(f.page.stream_index()==2,"Ready update changes selected release");
    f.content.streams.clear(); f.page.refresh(); f.move(hui::Direction::none);
    expect(f.page.stream_index()==-1,"reload keeps stale playable index");
}
void series_returns_to_episodes() {
    Fixture f; f.content.series=true; f.content.episodes.resize(2); f.page.set_content(&f.content);
    f.press(hui::Action::confirm); expect(f.page.mode()==sx::Detail::Mode::streams,"episode cannot open streams");
    f.press(hui::Action::page_next); f.press(hui::Action::back);
    expect(f.page.mode()==sx::Detail::Mode::episodes,"Back from filtered streams lost episodes");
}
void season_selection_does_not_open_streams() {
    Fixture f; f.content.series=true; f.content.seasons={"Season 1","Season 2","Specials"};
    f.content.episodes.resize(16); f.content.season=1; f.content.episode_hint=6;
    f.page.set_content(&f.content);
    expect(f.page.episode_index()==6,"opening a series loses resume episode");
    f.move(hui::Direction::up);
    expect(f.page.seasons_focused() && f.page.season_index()==1 && f.page.episode_index()==-1,"season chips expose a playable episode");
    f.move(hui::Direction::right); f.press(hui::Action::confirm);
    expect(f.page.season_index()==2 && f.page.mode()==sx::Detail::Mode::episodes,"Cross on season opens streams");
    f.content.season=2; f.content.episode_hint=12; ++f.content.hint_serial; f.page.refresh();
    expect(f.page.seasons_focused() && f.page.season_index()==2,"season response steals focus");
    f.move(hui::Direction::down);
    expect(f.page.episode_index()==12,"season's watch-progress hint was lost");
    f.move(hui::Direction::right); f.press(hui::Action::confirm);
    expect(f.page.episode_index()==13 && f.page.mode()==sx::Detail::Mode::streams,"episode carousel opens the wrong episode");
}
void empty_season_keeps_other_seasons_reachable() {
    Fixture f; f.content.series=true; f.content.seasons={"Season 1","Season 2"}; f.content.episodes.clear();
    f.page.set_content(&f.content); f.move(hui::Direction::none);
    expect(f.page.seasons_focused() && f.page.episode_index()==-1,"empty season traps focus on missing episode");
    f.move(hui::Direction::left); expect(f.page.season_index()==0,"season selection underflows");
    for (int i=0;i<8;++i) f.move(hui::Direction::right);
    expect(f.page.season_index()==1,"season selection overflows");
    f.press(hui::Action::confirm); expect(f.page.mode()==sx::Detail::Mode::episodes,"empty season can request streams");
    f.content.season=1; f.content.episodes.resize(1); ++f.content.hint_serial; f.page.refresh();
    f.move(hui::Direction::down); expect(f.page.episode_index()==0,"new season's episode cannot receive focus");
}
void late_episode_art_keeps_focus_and_selection() {
    Fixture f; f.content.series=true; f.content.seasons={"Season 1","Season 2"}; f.content.episodes.resize(30);
    f.page.set_content(&f.content);
    for (int i=0;i<19;++i) f.move(hui::Direction::right);
    f.content.episodes[19].thumb=123; f.page.refresh();
    expect(f.page.episode_index()==19,"late thumbnail resets carousel selection");
    f.move(hui::Direction::up); f.move(hui::Direction::right); f.page.refresh();
    expect(f.page.season_index()==1,"art refresh resets unconfirmed season selection");
    f.content.season=1; f.content.episode_hint=4; ++f.content.hint_serial; f.page.refresh();
    f.move(hui::Direction::down); expect(f.page.episode_index()==4,"season switch reuses previous season's cursor");
    f.content.episodes.clear(); f.content.seasons.clear(); f.page.refresh(); f.move(hui::Direction::none);
    expect(!f.page.seasons_focused(),"removed seasons leave stale chip focus");
}

}
int main() {
    const struct { const char *name; void (*run)(); } tests[] = {
        {"aio_card_preserves_badges",aio_card_preserves_badges}, {"filename_and_hint_metadata",filename_and_hint_metadata},
        {"no_invented_metadata",no_invented_metadata}, {"flags_and_hdr_plus",flags_and_hdr_plus},
        {"filtered_index_maps_to_source",filtered_index_maps_to_source}, {"pills_never_start_playback",pills_never_start_playback},
        {"empty_filter_and_late_arrival",empty_filter_and_late_arrival}, {"arrival_keeps_selected_source",arrival_keeps_selected_source},
        {"series_returns_to_episodes",series_returns_to_episodes},
        {"season_selection_does_not_open_streams",season_selection_does_not_open_streams},
        {"empty_season_keeps_other_seasons_reachable",empty_season_keeps_other_seasons_reachable},
        {"late_episode_art_keeps_focus_and_selection",late_episode_art_keeps_focus_and_selection},
    };
    int failed=0;
    for (auto &t:tests) {
        try { t.run(); std::printf("%s: PASS\n",t.name); }
        catch(const std::exception &e) { ++failed; std::printf("%s: FAIL (%s)\n",t.name,e.what()); }
    }
    std::printf("Stream presentation and navigation: 12 tests, %d failures\n",failed);
    return failed ? 1 : 0;
}
