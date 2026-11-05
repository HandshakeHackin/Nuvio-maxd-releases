#include "source_preferences_boundary.hpp"
#include <cassert>
#include <limits>
#include <iostream>
#include <fstream>
using namespace sx::source_policy;
static Stream source(const std::string& filename,double size=1e9) {
    Stream s;s.filename=filename;s.video_size=size;s.url="https://fixture.invalid/play?token=not-metadata";return s;
}
int main(int argc,char** argv){
    Preferences p;
    assert(assess(source("Show.S01E03.1080p.mkv"),p,{1,2}).hidden);
    assert(!assess(source("Show_S01E02_1080p.mkv"),p,{1,2}).hidden);
    assert(!assess(source("Show.S01E01-E03.mkv"),p,{1,2}).hidden);
    assert(!assess(source("Show.S01E01E02.mkv"),p,{1,2}).hidden);
    assert(!assess(source("Show.S01E01.S01E02.mkv"),p,{1,2}).hidden);
    assert(!assess(source("Show.Season.Pack.S01E03.mkv"),p,{1,2}).hidden);
    assert(!assess(source("Show.S01E03.mkv"),p,{}).hidden);
    assert(assess(source("film.sample.mkv",10e6),p).hidden);
    assert(!assess(source("film.sample.mkv",0),p).warning.empty());
    assert(assess(source("film.zip"),p).hidden);
    assert(assess(source("film.exe"),p).hidden);
    assert(!assess(source("film.ts"),p).hidden);
    assert(assess(source("film.ts"),p).warning.empty());
    assert(assess(source("film.camera.mkv"),p).warning.empty());
    assert(assess(source("film.DVD.mkv"),p).warning.empty());
    p.hide_cam=true;assert(assess(source("film.HDCAM.mkv"),p).hidden);
    p.checks=false;assert(!assess(source("film.zip"),p).hidden);
    p=Preferences{};p.min_height=1080;p.max_gb=5;p.hdr="avoid";
    assert(assess(source("film.720p.mkv"),p).hidden);
    assert(assess(source("film.1080p.mkv",6e9),p).hidden);
    assert(assess(source("film.1080p.HDR10.mkv"),p).hidden);
    assert(!assess(source("film.1080p.DVD.mkv"),p).hidden);
    assert(!assess(source("unknown",std::numeric_limits<double>::quiet_NaN()),p).hidden);
    assert(!assess(source("unknown",std::numeric_limits<double>::infinity()),p).hidden);
    auto english=source("film.1080p.English.mkv"),other=source("film.2160p.mkv");
    p=Preferences{};p.language="eng";assert(assess(english,p).score>assess(other,p).score);
    assert(assess(english,p).reason.find("Preferred language advertised")!=std::string::npos);
    auto archive=source("film.rar");auto torrent=source("film.mkv");torrent.url.clear();torrent.info_hash=std::string(40,'a');
    auto filtered=select({archive,english,torrent},p,{},false);
    assert(filtered.hidden==1&&filtered.order==std::vector<size_t>{1});
    auto all=select({archive,english,torrent},p,{},false,true);
    assert(all.order==std::vector<size_t>({1,0})); // Show all never enables raw torrents.
    all=select({archive,english,torrent},p,{},true,true);assert(all.order.size()==3);
    assert(select({archive},p,{},false).order.empty()); // Never silently relax filters.
    assert(select({english,english},p,{},false).order==std::vector<size_t>({0,1}));
    auto malicious=english;malicious.filename="unknown";malicious.url="https://fixture.invalid/English.HDR.S01E03.exe";
    assert(!assess(malicious,Preferences{},{1,2}).hidden);assert(assess(malicious,p).reason.find("language")==std::string::npos);
    json stored={{"other","preserved"}};save(stored,p);auto loaded=load(stored);
    assert(loaded.language=="eng"&&stored["other"]=="preserved");
    auto invalid=load({{"source_preferences",{{"min_height",1e100},{"max_gb",-1},{"hdr","injected"},{"language","injected"}}}});
    assert(invalid.min_height==0&&invalid.max_gb==0&&invalid.hdr=="any"&&invalid.language.empty());
    Meta meta;Video v;v.id="kitsu:1:2";v.season=1;v.episode=2;meta.videos={v};
    assert(context(meta,v.id).season==-1);meta.videos[0].imdb_season=2;meta.videos[0].imdb_episode=4;
    assert(context(meta,v.id).season==2&&context(meta,v.id).episode==4);
    sx::Session s;s.source_options();assert(s.choices.size()==9&&s.cancellations==1);
    s.pick(2);s.pick(2);assert(s.prefs_.source_preferences.min_height==1080&&s.saves==1);
    s.pick(6);assert(s.d_show_all_sources_&&s.saves==1); // Search-only override, not persisted.
    s.source_options();auto stale=s.choice;++s.profile_generation_;stale(0);assert(s.prefs_.source_preferences.checks);
    s.source_options();s.pick(4);stale=s.choice;++s.d_gen_;stale(2);assert(s.prefs_.source_preferences.hdr=="any");
    s.source_options();s.pick(7);assert(s.plays==0&&!s.notice.empty());
    s.d_found_={english};s.source_options();s.pick(7);assert(s.plays==1);
    s.d_found_={archive};s.source_options();s.pick(7);assert(s.plays==1);
    s.d_open_=false;s.source_options();assert(s.choices.size()==7);s.pick(6);assert(s.prefs_.source_preferences.min_height==0);
    sx::Session detail;
    Video ep;ep.id=detail.d_video_id_;ep.season=1;ep.episode=2;detail.d_meta_.videos={ep};
    auto right=source("Show.S01E02.1080p.English.mkv"),wrong=source("Show.S01E03.2160p.mkv");
    detail.d_found_={wrong,right};detail.detail_refresh_streams();
    assert(detail.d_streams_.size()==1&&detail.d_streams_[0].filename==right.filename);
    assert(detail.detail.streams.size()==1&&!detail.detail.streams[0].assessment.empty());
    assert(detail.detail.streams_status.find("filtered")!=std::string::npos);
    detail.d_show_all_sources_=true;detail.detail_refresh_streams();
    assert(detail.d_streams_.size()==2&&detail.d_streams_[1].filename==wrong.filename);
    assert(detail.detail.streams[1].source_warning);
    detail.d_show_all_sources_=false;detail.prefs_.source_preferences.min_height=2160;
    detail.detail_refresh_streams();assert(detail.d_streams_.empty());
    detail.prefs_.source_preferences.min_height=0;
    auto previous=right;previous.addon="fixture";right.addon=previous.addon;wrong.addon=previous.addon;
    detail.d_found_={wrong,right};detail.autoplay_previous_=previous;detail.autoplay_pending_=true;
    detail.detail_refresh_streams();assert(detail.plays==1&&detail.played.filename==right.filename);
    if(argc>1) {
        sx::Session menu;menu.source_options();
        json scenes;scenes["source-preferences"]={{"title",menu.dropdown_title},{"message",menu.dropdown.message},{"options",menu.choices}};
        Preferences pref;pref.language="eng";
        const std::vector<Stream> examples={source("Example.Show.S01E02.1080p.English.WEB-DL.mkv",2.4e9),source("Example.Show.S01E02.2160p.HDR.HEVC.mkv",12e9),source("Example.Show.S01E03.sample.mkv",20e6),source("Example.Show.S01E02.HDCAM.mkv",8e8)};
        const auto chosen=select(examples,pref,{1,2},false);
        scenes["source-ranking"]["hidden"]=chosen.hidden;
        for(auto i:chosen.order) {
            const auto view=sx::stream_display::view_of(examples[i]);const auto& a=chosen.assessments[i];
            scenes["source-ranking"]["streams"].push_back({{"name",view.detail},{"size",view.size},{"language",view.language},{"badges",view.badges},{"assessment",a.warning.empty()?a.reason:a.warning},{"warning",!a.warning.empty()}});
        }
        std::ofstream(argv[1])<<scenes.dump(2);
    }
    std::cout<<"Source checks, preference validation/ranking, safe overrides and production menu callbacks passed\n";
}
