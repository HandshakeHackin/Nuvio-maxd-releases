#include "experience_preferences.hpp"
#include "release_notes.hpp"
#include "episode_playback.hpp"
#include "player.hpp"
#include "theme.hpp"
#include "core/tween.hpp"
#include "diagnostics.h"
#include "util.h"
#include <cassert>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
struct Glyphs : hui::gfx::GlyphSource {
 bool glyph(uint32_t, hui::gfx::GlyphBitmap& b) override { b.advance=8; b.w=6; b.h=9; b.offset_y=-8; b.pixels.assign(54,255); return true; }
};
int main() {
 assert(sx::should_show_release_notes("","0.5.17") && sx::should_show_release_notes("0.5.16","0.5.17"));
 assert(!sx::should_show_release_notes("0.5.17","0.5.17"));
 sx::ExperiencePreferences defaults;
 assert(defaults.install_source.empty() && defaults.audio_output_mode=="pcm");
 sx::load_experience({{"audio_output_mode","invalid"}},defaults);assert(defaults.audio_output_mode=="pcm");
 for(const std::string mode : {"pcm","auto","bitstream"}) {
     defaults.audio_output_mode=mode;nlohmann::json saved;sx::save_experience(saved,defaults);
     sx::ExperiencePreferences loaded;sx::load_experience(saved,loaded);assert(loaded.audio_output_mode==mode);
 }
 sx::load_experience({{"install_source","unsupported"}},defaults); assert(defaults.install_source.empty());
 sx::load_experience({{"install_source",42}},defaults); assert(defaults.install_source.empty());
 for(const std::string source : {"store","github","unknown"}) {
     defaults.install_source=source;
     nlohmann::json saved; sx::save_experience(saved,defaults);
     sx::ExperiencePreferences loaded; sx::load_experience(saved,loaded);
     assert(loaded.install_source==source);
 }
 assert(sx::update_destination("store").find("Homebrew Store")!=std::string::npos);
 assert(sx::update_destination("github").find("GitHub")!=std::string::npos);
 assert(sx::update_destination("unknown").find("Choose")!=std::string::npos);
 std::cout<<"Install source: unset migration, validated local persistence and source-specific update routing passed\n";
 assert(defaults.seek_seconds==10 && defaults.shoulder_seek_seconds==60 && defaults.next_episode_seconds==10);
 sx::load_experience({{"subtitle_font","bad"},{"seek_seconds",999},{"shoulder_seek_seconds",-10},{"next_episode_seconds",0},{"high_contrast","true"}},defaults);
 assert(defaults.subtitle_style.font==1 && defaults.seek_seconds==60 && defaults.shoulder_seek_seconds==5 && defaults.next_episode_seconds==5 && !defaults.high_contrast);
 defaults.seen_release="0.5.17";
 defaults.subtitle_style={2,1,2,75}; defaults.reduced_motion=true; defaults.high_contrast=true;
 nlohmann::json settings; sx::save_experience(settings,defaults); sx::ExperiencePreferences roundtrip;
 sx::load_experience(settings,roundtrip);
 assert(!sx::should_show_release_notes(roundtrip.seen_release,"0.5.17"));
 assert(roundtrip.subtitle_style.background==75 && roundtrip.subtitle_style.effect==2 && roundtrip.subtitle_style.font==2 && roundtrip.high_contrast && roundtrip.reduced_motion);
 hui::tween::reduced_motion=true;
 hui::tween::Spring spring; spring.target=100; spring.update(.001f); assert(spring.value==100 && spring.velocity==0);
 hui::tween::Bounce bounce; bounce.target=2; bounce.kick(50); bounce.update(.001f); assert(bounce.value==2 && bounce.velocity==0);
 hui::tween::Timer timer; timer.start(2); timer.update(.001f); assert(timer.progress()==1 && !timer.running);
 assert(hui::tween::stagger(0,100)==1);
 hui::tween::reduced_motion=false;
 spring.snap(0);spring.target=100;spring.update(.001f); assert(spring.value<100);
 sx::theme::set_high_contrast(true); assert(sx::theme::ink_2.a==1 && sx::theme::surface.a==1 && sx::theme::hairline.a>.5);
 sx::theme::set_high_contrast(false); assert(sx::theme::ink_2.a<1 && sx::theme::surface.a<.1);
 hui::tween::reduced_motion=true; // UI accessibility must not speed up playback countdowns
 sx::episode_playback::Flow flow; flow.segments={{sx::episode_playback::Kind::credits,90,100}}; flow.set_countdown(30);
 flow.update(95,100,.25,true,true,true,true);
 for(int i=0;i<80;++i) flow.update(95,100,.25,true,true,true,true);
 assert(flow.remaining==10 && !flow.launch());
 for(int i=0;i<80;++i) flow.update(95,100,.25,false,true,true,true);
 assert(flow.remaining==10);
 for(int i=0;i<40;++i) flow.update(95,100,.25,true,true,true,true);
 assert(flow.launch()); flow.cancel(); flow.update(95,100,.25,true,true,true,true);assert(!flow.launch());
 flow.set_countdown(5); assert(flow.remaining==5 && flow.cancelled);
 sx::episode_playback::Flow eof; eof.set_countdown(5);
 eof.update(100,100,.25,true,true,true,true,true);
 for(int i=0;i<19;++i) eof.update(100,100,.25,true,true,true,true,true);
 assert(eof.next_visible && !eof.launch());
 eof.update(100,100,.25,true,true,true,true,true); assert(eof.launch());
 hui::tween::reduced_motion=false;
 Glyphs glyphs; hui::gfx::Font regular,bold,display;
 for(auto* f : {&regular,&bold,&display}) f->init_dynamic(&glyphs,10,2,8,-2,0,256);
 hui::ui::Fonts fonts; fonts.regular={&regular,1};fonts.semibold={&bold,2};fonts.display={&display,3};
 hui::gfx::DrawList plain,outline,shadow,box;
 sx::draw_subtitle_block(plain,fonts,"Hello world",{0,0,0,0},20,100,100,180);
 sx::draw_subtitle_block(outline,fonts,"Hello world",{1,1,1,0},20,100,100,180);
 sx::draw_subtitle_block(shadow,fonts,"Hello world",{2,2,2,0},20,100,100,180);
 sx::draw_subtitle_block(box,fonts,"Hello world",{0,0,0,75},20,100,100,180);
 assert(!plain.empty() && outline.instances().size()>shadow.instances().size() && shadow.instances().size()>plain.instances().size());
 assert(box.instances().size()>plain.instances().size());
 assert(outline.instances().back().color_top[0]>.99f && outline.instances().back().color_top[2]<.5f);
 assert(shadow.instances().back().color_top[0]<.7f && shadow.instances().back().color_top[2]>.99f);
 assert(plain.runs().front().texture==1 && outline.runs().front().texture==2 && shadow.runs().front().texture==3);
 const auto filtered=nuvio_diagnostics::filter_context({{"streamUrl","secret"},{"token","secret"},{"title","private"},{"droppedFrames",24},{"paused",true},{"uiFps","bad"}});
 assert(filtered.size()==2 && filtered["droppedFrames"]==24 && !filtered.contains("streamUrl"));
 const auto dir=std::filesystem::temp_directory_path()/("nuvio-diagnostics-"+std::to_string(getpid()));
 std::filesystem::create_directories(dir);
 save_json((dir/"runtime.json").string(),{{"cleanExit",false},{"droppedFrames",66}});
 nuvio_diagnostics::init(dir.string(),"test");nuvio_diagnostics::record({{"droppedFrames",24},{"paused",true},{"token","secret"}});nuvio_diagnostics::shutdown();
 nlohmann::json recorded,previous; assert(load_json((dir/"runtime.json").string(),recorded) && recorded["cleanExit"]==true && recorded["droppedFrames"]==24 && !recorded.contains("token"));
 assert(load_json((dir/"runtime-previous.json").string(),previous) && previous["droppedFrames"]==66);
 const auto child=fork(); assert(child>=0);
 if(child==0) { rlimit limit{0,0}; setrlimit(RLIMIT_CORE,&limit); nuvio_diagnostics::init((dir/"fatal").string(),"test");raise(SIGABRT);_exit(1); }
 int status=0;waitpid(child,&status,0);assert(WIFSIGNALED(status) && WTERMSIG(status)==SIGABRT);
 std::string crash;assert(read_file((dir/"fatal/crash-current.txt").string(),crash) && crash=="fatal signal: SIGABRT\n");
 std::filesystem::remove_all(dir);
 std::cout<<"Release notes: first launch, update and persistent acknowledgement passed\n";
 std::cout<<"User experience: preference recovery, subtitle styles, motion/contrast, custom countdown and private crash/runtime diagnostics passed\n";
}
