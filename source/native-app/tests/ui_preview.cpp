// Render the native QR overlay with Mesa using the console's drawing code.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "overlays.hpp"
#include "../update/core.hpp"
#include "../gl/install_source.hpp"
#include "detail.hpp"
#include "player.hpp"
#include "settings.hpp"
#include "board.hpp"
#include <cmath>
#include "dynamic_font.hpp"
#include "gfx/renderer.hpp"
#include "qrcodegen.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/glcorearb.h>
#include <cstdio>
#include <string>
#include <vector>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

int main(int argc, char** argv) {
    if (argc != 3 && argc != 4) return 2;
    auto get_display = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
    EGLDisplay display = get_display(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr);
    if (!eglInitialize(display, nullptr, nullptr) || !eglBindAPI(EGL_OPENGL_API)) return 3;
    const EGLint attrib[]{EGL_CONTEXT_MAJOR_VERSION, 4, EGL_CONTEXT_MINOR_VERSION, 5,
        EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT, EGL_NONE};
    EGLContext context = eglCreateContext(display, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, attrib);
    if (context == EGL_NO_CONTEXT || !eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context)) return 4;
    hui::gfx::Renderer renderer;
    if (!renderer.init()) return 5;
    sx::UiFonts font_data; hui::ui::Fonts fonts; sx::Icons icons;
    const std::string root = argv[1];
    if (!sx::load_ui_fonts(renderer, root + "/app/fonts", root + "/third_party/hui/assets/fonts", &font_data, &fonts)) return 6;
    if(!font_data.regular.has_glyph(0x4e2d) || !font_data.regular.has_glyph(0x65e5) || !font_data.regular.has_glyph(0xac00) || !font_data.regular.has_glyph(0x0e01)) return 8;
    sx::SignInState state;
    state.link = "nuvio.tv/link"; state.code = "ABCD-EF";
    state.status = "Waiting for approval on your phone...";
    std::uint8_t temp[qrcodegen_BUFFER_LEN_MAX], qr[qrcodegen_BUFFER_LEN_MAX];
    if (!qrcodegen_encodeText("https://nuvio.tv/link?code=ABCD-EF", temp, qr, qrcodegen_Ecc_MEDIUM,
        1, 20, qrcodegen_Mask_AUTO, true)) return 7;
    state.qr_size = qrcodegen_getSize(qr); state.qr_modules.resize(state.qr_size * state.qr_size);
    for (int y = 0; y < state.qr_size; ++y) for (int x = 0; x < state.qr_size; ++x)
        state.qr_modules[y * state.qr_size + x] = qrcodegen_getModule(qr, x, y);
    hui::gfx::DrawList list;
    const sx::Env env{fonts,icons};
    const std::string mode = argc == 4 ? argv[3] : "login";
    const auto artwork = [&](int w, int h, float tint) {
        std::vector<std::uint8_t> px(static_cast<std::size_t>(w)*h*4);
        for (int y=0;y<h;++y) for (int x=0;x<w;++x) {
            const float fx=static_cast<float>(x)/w, fy=static_cast<float>(y)/h;
            const float ridge=0.5f+0.1f*std::sin(fx*13+tint)+0.07f*std::sin(fx*27);
            const bool hill=fy>ridge;
            const auto i=(static_cast<std::size_t>(y)*w+x)*4;
            px[i]=static_cast<std::uint8_t>(hill ? 18+fy*20 : 34+fy*80+tint*10);
            px[i+1]=static_cast<std::uint8_t>(hill ? 28+fy*24 : 72+fy*90);
            px[i+2]=static_cast<std::uint8_t>(hill ? 38+fy*32 : 105+fy*65);
            px[i+3]=255;
        }
        return renderer.batch().create_texture(w,h,px.data());
    };
    const auto backdrop_texture=artwork(1920,1080,0);
    const auto poster_texture=artwork(480,720,3);
    if(mode=="donation") {
        state.link_only=true;state.title="Support on Ko-fi";state.link=sx::donation_link;state.cancel_label="Close";
        std::uint8_t temp[qrcodegen_BUFFER_LEN_MAX],donation_qr[qrcodegen_BUFFER_LEN_MAX];
        assert(qrcodegen_encodeText(sx::donation_url,temp,donation_qr,qrcodegen_Ecc_MEDIUM,qrcodegen_VERSION_MIN,20,qrcodegen_Mask_AUTO,true));
        state.qr_size=qrcodegen_getSize(donation_qr);state.qr_modules.resize(state.qr_size*state.qr_size);
        for(int y=0;y<state.qr_size;++y)for(int x=0;x<state.qr_size;++x)
            state.qr_modules[y*state.qr_size+x]=qrcodegen_getModule(donation_qr,x,y);
        sx::draw_sign_in(list,env,state,0,1);
    }
    else if(mode=="release-notes") sx::draw_release_notes(list,env,nuvio_update::current_version);
    else if(mode=="audio-output") {
        sx::DropdownState d;d.title="Audio output";d.options={"PCM","Auto (experimental)","Bitstream (experimental)"};
        d.message="Auto uses formats advertised by your HDMI receiver. Refused ports fall back to PCM. TrueHD and DTS-HD keep decoded lossless PCM.";
        sx::draw_dropdown(list,env,d,1);
    }
    else if(mode=="install-source" || mode=="store-update" || mode=="github-update") {
        sx::DropdownState d;
        d.title=mode=="install-source" ? "Install source" : "New release: 0.5.21";
        d.options=mode=="install-source" ? std::vector<std::string>{"Homebrew Store","GitHub / manual download","Not sure"} :
            mode=="store-update" ? std::vector<std::string>{"Close","Change install source"} :
            std::vector<std::string>{"Download update in app","GitHub update instructions","Later"};
        d.selected=mode=="install-source"?2:0;
        d.active=mode=="install-source"?2:-1;
        d.message=mode=="install-source" ? "We'll point update reminders to the place you chose. This choice stays on your PS5 and can be changed in Settings → Install source." :
            mode=="store-update" ? "Open Homebrew Store, find Nuvio Max'd, and check for an update. The store may publish it later than GitHub." :
            "Update from github.com/HandshakeHackin/Nuvio-maxd-releases/releases, or download the verified image here.";
        sx::draw_dropdown(list,env,d,1);
    }
    else if(mode=="subtitle-preview" || mode=="subtitle-preview-high-contrast") {
        sx::SettingsContent content;
        content.subtitle_style=mode=="subtitle-preview" ? sx::SubtitleStyle{} : sx::SubtitleStyle{2,1,2,75};
        content.subtitle_size=1;
        sx::theme::set_high_contrast(mode=="subtitle-preview-high-contrast");
        hui::tween::reduced_motion=mode=="subtitle-preview-high-contrast";
        const char* labels[]={"Subtitle font","Subtitle colour","Subtitle effect","Subtitle background","D-pad seek interval","L1 / R1 seek interval","Next-episode countdown"};
        const char* values[]={"Bold","White","Outline","Off","10 s","60 s","10 s"};
        for(int i=0;i<7;++i) { sx::SettingRow row; row.label=labels[i]; row.value=values[i]; row.help="Choose your preferred style. The preview uses the same renderer as video playback."; row.subtitle_preview=i<4;content.rows.push_back(row); }
        if(mode=="subtitle-preview-high-contrast") { content.rows[0].value="Display";content.rows[1].value="Yellow";content.rows[2].value="Shadow";content.rows[3].value="75%"; }
        sx::Settings settings(env);settings.set_content(&content);settings.enter();hui::InputFrame input;
        for(int i=0;i<120;++i) settings.update(input,1.0f/60);
        hui::gfx::BackdropSpec backdrop;settings.draw(list,backdrop);
    }
    else if (mode == "player" || mode == "tracks" || mode == "skip" || mode == "skip-focused" ||
        mode == "recap-focused" || mode == "credits-focused" || mode == "next-focused")
    {
        list.image(backdrop_texture,{0,0,1920,1080},hui::gfx::kFullUv,hui::gfx::Color::rgb(0xffffff));
        sx::PlayerUi player(env);
        player.state.title="The End of Oak Street";
        player.state.subtitle="Season 2  ·  Episode 5  ·  The Quiet Hours";
        player.state.audio="English · EAC3 5.1"; player.state.subs="English";
        player.state.position=2136; player.state.duration=7200; player.state.buffered=60;
        player.state.stats="HEVC 3840×2160 (hardware) · 24 fps · buffered 60 s · dropped 0";
        player.state.quality="Picture 3840×2160 10-bit HDR10 → SDR · Render 3840×2160 SDR 8-bit · 60 Hz · Audio stereo";
        player.state.line="Every story deserves its own voice.";
        player.state.controls=true;
        if (mode == "skip" || mode == "skip-focused" || mode == "recap-focused" ||
            mode == "credits-focused" || mode == "next-focused") {
            player.state.controls=false;
            player.state.skip_label = mode == "recap-focused" ? "Skip Recap" :
                                      mode == "credits-focused" || mode == "next-focused" ? "Skip Credits" : "Skip Intro";
            player.state.episode_focused = mode != "skip";
            if (mode == "next-focused") {
                player.state.next_label = "Next: S2 E6  A Familiar Face";
                player.state.next_status = "Playing in 15 s";
                player.state.episode_action = 1;
            }
        }
        if (mode == "tracks") {
            player.state.menu=true;
            player.state.audio_tracks={"English · EAC3 5.1", "Spanish · AC3 5.1", "Commentary · AAC stereo"};
            player.state.subtitle_tracks={"Off", "English", "Spanish", "French"};
            player.state.menu_row=0; player.state.subtitle_active=1;
        }
        player.snap(); player.draw(list);
    }
    else if (mode == "home")
    {
        sx::BoardContent content;
        for (int r=0;r<2;++r) {
            sx::Row row; row.title=r ? "Popular TV" : "Continue Watching";
            for (int i=0;i<9;++i) {
                sx::Title t; t.name="The Quiet Hours"; t.id=std::to_string(i); t.poster=poster_texture; t.backdrop=backdrop_texture;
                t.year="2026"; t.synopsis="A quiet town, a familiar face, and a story waiting to be told.";
                if (!r) { t.progress=0.36f; t.badge="S2 E5"; }
                row.items.push_back(t);
            }
            content.rows.push_back(row);
        }
        sx::Board board(env); board.set_content(&content); board.enter();
        hui::InputFrame input;
        for (int i=0;i<120;++i) board.update(input,1.0f/60);
        hui::gfx::BackdropSpec backdrop; board.draw(list,backdrop);
    }
    else if (mode != "login")
    {
        sx::DetailContent content;
        content.title.name = "The End of Oak Street"; content.title.year = "2026";
        content.title.genres = "Drama  ·  Mystery";
        if (mode != "streams") {
            content.title.backdrop=backdrop_texture; content.title.poster=poster_texture;
            content.title.runtime="2h 08m"; content.title.imdb="8.2";
            content.directors="Avery Quinn"; content.cast="Alex Morgan, Sam Rivera, Taylor Reed";
        }
        content.title.synopsis = "Choose a source from your addons. Video, audio and language badges describe the release supplied by each addon.";
        content.stream_heading = "Available streams"; content.stream_addons = {"AIOStreams","Pengu"};
        content.resume_label = "Resume from 35:36";
        using B = sx::StreamBadge;
        for (int i = 0; i < 5; ++i)
        {
            sx::Stream s;
            s.addon = "AIOStreams"; s.name = "UHD⁴ᴷ  WEB-DL  [TB]";
            s.size = i == 0 ? "14 GB" : i == 1 ? "11 GB" : "10 GB";
            s.language = i == 0 ? "LA" : "MULTI"; s.seeds = "Direct / debrid";
            s.detail = "The.End.of.Oak.Street.2160p.WEB-DL.DV.HDR.HEVC.DDP5.1.mkv";
            s.badges = {B::web_dl,B::uhd,B::hdr,B::hevc,B::dolby_plus,B::channels_51};
            if (i == 0) s.badges.insert(s.badges.begin()+2,B::dolby_vision);
            content.streams.push_back(s);
        }
        if (mode == "series" || mode == "seasons") {
            content.series=true; content.seasons={"Season 1","Season 2","Season 3","Season 4","Specials"}; content.season=1;
            for (int i=0;i<12;++i) {
                sx::Episode e; e.number=i+1; e.title=i ? "A Familiar Face" : "The Quiet Hours";
                e.synopsis="A quiet town, a familiar face, and a story waiting to be told.";
                e.info="Oct 8, 2026"; e.thumb=backdrop_texture; e.watched=i==0; e.progress=i==1 ? 0.36f : -1;
                content.episodes.push_back(e);
            }
            content.episode_hint=1;
        }
        sx::Detail detail(env); detail.set_content(&content); detail.enter();
        hui::InputFrame input;
        for (int i = 0; i < 120; ++i) detail.update(input,1.0f/60);
        if (mode == "seasons") { input.nav=hui::Direction::up; detail.update(input,1.0f/60); }
        hui::gfx::BackdropSpec backdrop; detail.draw(list,backdrop);
    }
    else sx::draw_sign_in(list, env, state, 1.0f, 1.0f);
    sx::sync_fonts(renderer, font_data, fonts);
    GLuint framebuffer, color;
    glGenFramebuffers(1, &framebuffer); glGenRenderbuffers(1, &color);
    glBindRenderbuffer(GL_RENDERBUFFER, color); glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 1920, 1080);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) return 8;
    renderer.begin(); renderer.draw(list); renderer.present(framebuffer, 1920, 1080);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    std::vector<unsigned char> pixels(1920 * 1080 * 4);
    glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    const auto error = glGetError();
    stbi_flip_vertically_on_write(1);
    const bool written = stbi_write_png(argv[2], 1920, 1080, 4, pixels.data(), 1920 * 4);
    std::fprintf(stderr, "%s: GL error 0x%x; %zu shapes\n", mode.c_str(), error, renderer.last_instances());
    return written && error == GL_NO_ERROR ? 0 : 9;
}
