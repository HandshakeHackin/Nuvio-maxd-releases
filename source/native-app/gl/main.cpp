// Nuvio for the PS5 on the OpenGL toolkit: opens the display and the
// controller, then runs the frame loop. What the buttons mean is ui/shell.cpp,
// what the screens show is gl/session.cpp.

#include "chrome.hpp"
#include "console_lights.hpp"
#include "system_status.hpp"
#include "core/input.hpp"
#include "dynamic_font.hpp"
#include "display_output.h"
#include "frame_pacer.hpp"
#include "gfx/renderer.hpp"
#include "hwdec_ps5.h"
#include "hdr_compositor.hpp"
#include "hdr_output.hpp"
#include "platform/ps5/display_egl.hpp"
#include "platform/ps5/pad.hpp"
#include "platform/ps5/system.hpp"
#include "overlays.hpp"
#include "page.hpp"
#include "session.hpp"
#include "diagnostics.h"
#include "../update/core.hpp"
#include "shell.hpp"
#include "theme.hpp"

#include <algorithm>
#include <csignal>
#include <cstdio>
#include <span>
#include <string>

extern "C" void ps5_load_modules(void); // the keyboard dialog's module (native/ps5_modules.c)

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#include "stb_image.h"

namespace
{

constexpr const char *kRoot = "/app0";
// The title's own storage (param.json downloadDataSize): settings, progress, artwork.
constexpr const char *kData = "/download0/nuvio";

std::uint32_t load_icon(hui::gfx::Renderer &renderer, const std::string &path)
{
    int w = 0, h = 0, comp = 0;
    unsigned char *pixels = stbi_load(path.c_str(), &w, &h, &comp, 4);
    if (!pixels)
    {
        hui::sys::log("[SX] cannot load %s", path.c_str());
        return 0;
    }
    const std::uint32_t texture = renderer.batch().create_texture(w, h, pixels);
    stbi_image_free(pixels);
    return texture;
}

} // namespace

int main()
{
    using namespace hui;
    const std::int64_t startup_t0 = sys::monotonic_us();
    auto startup_previous = startup_t0;
    const auto startup_stage = [&](const char* stage) {
        const auto now = sys::monotonic_us();
        sys::log("[SX] startup stage=%s elapsed_ms=%.1f stage_ms=%.1f", stage,
                 double(now - startup_t0) / 1000, double(now - startup_previous) / 1000);
        startup_previous = now;
    };
    nuvio_diagnostics::init(kData,nuvio_update::current_version);
    sys::log("[SX] entry");
    const std::string root = kRoot;
    // A closed connection must fail a write quietly instead of killing us.
    std::signal(SIGPIPE, SIG_IGN);

    // Read before EGL starts: changing the mode later would destroy GL objects.
    json stored;
    load_json(std::string(kData) + "/settings.json", stored);
    hui::tween::reduced_motion = jbool(stored,"reduced_motion",false);
    sx::theme::set_high_contrast(jbool(stored,"high_contrast",false));
    const int preference = display_output_preference(jstr(stored, "display_output", "ps5"));
    const int refresh = jnum(stored, "display_refresh", 60) == 120 ? 120 : 60;
    const auto selected = display_output_select(preference, ps5::Display::supports_display_modes());
    if (!selected.safe_to_open)
    {
        sys::log("[SX] fatal: VideoOut probe could not close; refusing a second open");
        nuvio_diagnostics::record({{"phase","fatal"}});
        sys::park();
    }
    ps5::Display display;
    bool opened = display.open(selected.width, selected.height, refresh);
    bool display_fallback = selected.fallback;
    if (!opened && refresh == 120)
    {
        sys::log("[SX] high refresh unavailable; retrying %dx%d at 60 Hz", selected.width, selected.height);
        opened = display.open(selected.width, selected.height, 60);
    }
    if (!opened && (selected.width != 1920 || selected.height != 1080))
    {
        sys::log("[SX] requested display %dx%d unavailable; trying 1920x1080", selected.width, selected.height);
        opened = display.open(1920, 1080, 60);
        display_fallback = true;
    }
    if (!opened)
    {
        nuvio_diagnostics::record({{"phase","fatal"}});
        sys::park();
    }
    display_fallback |= display.width() != selected.width || display.height() != selected.height;
    startup_stage("display");
    sys::log("[SX] display: PS5 configured=%ux%u query_rc=%08x requested=%dx%d render=%dx%d SDR; fallback=%d",
             selected.configured_width, selected.configured_height, unsigned(selected.query_result),
             selected.width, selected.height, display.width(), display.height(), display_fallback);

    gfx::Renderer renderer;
    static sx::UiFonts ui_fonts;
    static ui::Fonts fonts;
    if (!renderer.init())
    {
        sys::log("[SX] fatal: renderer failed");
        nuvio_diagnostics::record({{"phase","fatal"}});
        sys::park();
    }

    startup_stage("renderer");
    // Start the mark and loading animation before slow module and network setup.
    // The app name joins it as soon as the fonts are ready.
    static sx::Icons icons;
    const std::string icon_dir = root + "/icons";
    icons.logo_xl = load_icon(renderer, icon_dir + "/logo_xl.png");
    const std::int64_t boot_t0 = sys::monotonic_us();
    const auto boot_seconds = [&] { return static_cast<float>(sys::monotonic_us() - boot_t0) / 1e6f; };
    bool splash_hidden = false;
    ps5::Pad pad;
    bool pad_attempted = false;
    const auto boot_frame = [&] {
        const float boot_age = boot_seconds();
        const sx::Env boot_env{fonts, icons};
        gfx::DrawList list;
        gfx::BackdropSpec backdrop;
        sx::apply_page_backdrop(backdrop, boot_age);
        sx::draw_boot(list, boot_env, boot_age, 0.0f);
        renderer.begin();
        renderer.backdrop(backdrop);
        renderer.draw(list);
        renderer.present(0, display.width(), display.height());
        if (!display.swap())
        {
            sys::log("[SX] fatal: boot swap failed");
            nuvio_diagnostics::record({{"phase","fatal"}});
            sys::park();
        }
        if (!splash_hidden)
        {
            splash_hidden = true;
            sys::log("[SX] first picture; splash hidden: %d", sys::hide_splash_screen() ? 1 : 0);
        }
        if (!pad_attempted) { pad_attempted = true; pad.open(); }
        const auto light = sx::logo_animation(boot_age, 0).light;
        pad.set_light_bar(light.r, light.g, light.b);
    };
    boot_frame();
    startup_stage("first-picture");
    ps5_load_modules(); // the keyboard dialog
    boot_frame();
    HwDecoder::load_module(); // the hardware video decoder
    boot_frame();
    startup_stage("modules");
    if (!sx::load_ui_fonts(renderer, root + "/fonts", root + "/hui-fonts", &ui_fonts, &fonts))
    {
        sys::log("[SX] fatal: fonts failed");
        nuvio_diagnostics::record({{"phase","fatal"}});
        sys::park();
    }
    boot_frame();
    startup_stage("fonts");

    const char *solid[5] = {"nav_board", "nav_discover", "nav_library", "nav_addons", "nav_settings"};
    for (int i = 0; i < 5; ++i)
    {
        icons.nav_solid[i] = load_icon(renderer, icon_dir + "/" + solid[i] + ".png");
        icons.nav_line[i] = load_icon(renderer, icon_dir + "/" + solid[i] + "_o.png");
    }
    icons.search = load_icon(renderer, icon_dir + "/search.png");
    icons.logo = load_icon(renderer, icon_dir + "/logo_l.png");
    boot_frame();
    startup_stage("icons");

    static sx::Session session;
    if (!session.init(root, kData, &renderer))
    {
        sys::log("[SX] fatal: session failed");
        nuvio_diagnostics::record({{"phase","fatal"}});
        sys::park();
    }
    session.set_display_output(display.width(), display.height(), display.refresh_hz());
    startup_stage("session");
    session.set_display_configuration(selected.query_result == 0 ? selected.configured_width : 0,
                                      selected.query_result == 0 ? selected.configured_height : 0,
                                      display_fallback);
    static sx::Env env{fonts, icons};
    static sx::Shell shell(env, session);
    shell.start_sound(root + "/sounds"); // the interface's sounds
    shell.start_boot_at(boot_seconds()); // the animation goes on from where the frames above left it

    startup_stage("input-ready");
    InputTracker tracker;
    PadSample samples[64];

    std::uint64_t frames = 0;
    std::int64_t last_start = sys::monotonic_us();
    std::int64_t window_start = last_start;
    int window_frames = 0;
    float worst_ms = 0.0f;
    unsigned frame_hitches = 0;
    sx::FramePacer pacer;
    sx::ConsoleLights console_lights;
    sx::SystemStatus lighting_system;
    std::int64_t lighting_status_at=0;
    sx::HdrCompositor hdr_compositor;
    std::int64_t restore_retry_at = 0;
    for (;;)
    {
        pacer.configure(display.refresh_hz(), sys::monotonic_us());
        if (const auto delay = pacer.delay_us(sys::monotonic_us()))
            sys::sleep_us(delay);
        const std::int64_t now = sys::monotonic_us();
        pacer.started(now);
        session.set_display_output(display.width(), display.height(), display.refresh_hz());
        float dt = frames == 0 ? 1.0f / 60.0f : static_cast<float>(now - last_start) / 1e6f;
        last_start = now;
        const float frame_ms = dt * 1000.0f;
        dt = std::min(dt, 0.05f); // a hitch must not teleport the animations

        const std::size_t count = pad.read(samples);
        const InputFrame input = tracker.update(std::span<const PadSample>(samples, count), static_cast<std::uint64_t>(now));
        session.poll_presentation_timing();
        session.update(dt, shell.cursor());
        env.clock = session.clock;
        shell.update(input, dt);
        const auto light = shell.logo_light_bar();
        pad.set_light_bar(light.r, light.g, light.b);
        if(session.console_light_sync() && now>=lighting_status_at) { lighting_system=sx::read_system_status(); lighting_status_at=now+250000; }
        console_lights.update(session.console_light_sync() && lighting_system.valid &&
            !lighting_system.background && !lighting_system.overlaid,light.r,light.g,light.b);
        session.set_console_light_status(console_lights.status());
        if (session.wants_exit()) {
            console_lights.stop();
            pad.close();
            session.shutdown();
            if (sx::HdrOutput::active()) {
                glFinish();
                if (sx::HdrOutput::set(false,display.width(),display.height())) {
                    glBindFramebuffer(GL_FRAMEBUFFER,0);
                    glDisable(GL_SCISSOR_TEST); glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
                    glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
                    display.swap();
                } else sys::log("[HDR] SDR restore refused during exit rc=%08x",sx::HdrOutput::result());
            }
            nuvio_diagnostics::shutdown();
            session.complete_update_exit();
            sys::quit();
        }

        bool wants_hdr = session.watching && !session.launch_open && session.video_hdr && session.video_texture;
        if (wants_hdr && sx::HdrOutput::failed()) {
            session.fail_video_output(sx::HdrOutput::error());
            wants_hdr = false;
        }
        if (wants_hdr != sx::HdrOutput::active() && (wants_hdr || now >= restore_retry_at)) {
            std::string error;
            const bool ready = !wants_hdr || hdr_compositor.prepare(display.width(),display.height(),&error);
            glFinish(); // drain GPU writes before VideoOut retypes the registered set
            if (!ready || !sx::HdrOutput::set(wants_hdr,display.width(),display.height())) {
                if (wants_hdr)
                    session.fail_video_output(error.empty()
                        ? sx::HdrOutput::error()
                        : error);
                else {
                    sys::log("[HDR] SDR restore refused rc=%08x; keep packing HDR pixels and retry",sx::HdrOutput::result());
                    restore_retry_at = now + 1000000;
                }
            }
        }

        gfx::DrawList list;
        gfx::BackdropSpec backdrop;
        shell.draw(list, backdrop);
        if (session.calm_backgrounds() || session.reduced_motion()) backdrop.time = 0.0f;
        sx::sync_fonts(renderer, ui_fonts, fonts);
        session.arm_presentation_timing();
        renderer.begin();
        renderer.backdrop(backdrop);
        renderer.draw(list);
        if (sx::HdrOutput::active()) {
            const bool video = session.video_hdr && session.watching && !session.launch_open;
            const VideoRect rect = video_fit(session.video_w,session.video_h,session.video_sar,
                                             float(display.width()),float(display.height()));
            std::string error;
            if (!hdr_compositor.present(renderer,video ? session.video_texture : 0,rect,0,&error,sx::HdrOutput::bgr())) {
                sys::log("[HDR] fatal: %s",error.c_str());
                nuvio_diagnostics::record({{"phase","fatal"}});
                console_lights.stop();
                sys::park();
            }
        } else renderer.present(0, display.width(), display.height());
        if (!display.swap())
        {
            console_lights.stop();
            sys::log("[SX] fatal: swap failed at frame %llu", static_cast<unsigned long long>(frames));
            nuvio_diagnostics::record({{"phase","fatal"}});
            sys::park();
        }
        sx::presentation::disarm();
        sx::HdrOutput::presented();
        ++frames;
        if (frames == 1)
        {
            sys::log("[SX] first frame: %zu shapes, %zu draw calls", renderer.last_instances(), renderer.last_draw_calls());
        }

        // One line every ten seconds: how fast the frames came.
        ++window_frames;
        worst_ms = std::max(worst_ms, frame_ms);
        if(frame_ms>std::max(33.0f,2000.0f/display.refresh_hz())) ++frame_hitches;
        if (now - window_start >= 10'000'000)
        {
            const double seconds = static_cast<double>(now - window_start) / 1e6;
            sys::log("[SX] %.1f fps, worst frame %.1f ms, %zu shapes, %zu draw calls", window_frames / seconds, worst_ms,
                     renderer.last_instances(), renderer.last_draw_calls());
            nuvio_diagnostics::record({{"uiFps",window_frames/seconds},{"worstFrameMs",worst_ms},
                {"renderWidth",display.width()},{"renderHeight",display.height()},{"acceptedRefreshHz",display.refresh_hz()},{"frameHitches",frame_hitches},
                {"playing",session.watching && !session.launch_open},{"paused",session.playing.paused},{"hdrOutputActive",sx::HdrOutput::active()}});
            window_start = now;
            window_frames = 0;
            worst_ms = 0.0f; frame_hitches=0;
        }
    }
}
