// VLC access module using Nuvio's existing authenticated, cancellable HTTP input.
// SPDX-License-Identifier: GPL-3.0-or-later
#define MODULE_NAME nuviohttp
#define MODULE_STRING "nuviohttp"
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "netstream.h"
#include "vlc_cancel.h"
#include <vlc_common.h> // must precede other VLC private headers

#include <vlc_access.h>
#include <vlc_interrupt.h>
#include <vlc_plugin.h>
#ifdef NUVIO_VLC_STATIC
#include "torrent/torrent_stream.h"
#endif
#include "../third_party/json.hpp"
#include <algorithm>
#include <atomic>
#include <new>
#include <cerrno>
#include <cstdlib>
#include <cstring>
extern "C" {
#include <libavformat/avio.h>
#include <libavutil/error.h>
}
struct NuvioHttp {
    AVIOContext *input = nullptr;
    long serial = 0;
    std::atomic<bool> stop{false}, interrupt{false};
#ifdef NUVIO_VLC_STATIC
    bool torrent = false;
#endif
    int read_data(uint8_t *bytes, int count) {
#ifdef NUVIO_VLC_STATIC
        if (torrent)
            return TorrentStream::of(input)->read(bytes, count);
#endif
        return NetStream::of(input)->read(bytes, count);
    }
    int64_t size() {
#ifdef NUVIO_VLC_STATIC
        if (torrent)
            return TorrentStream::of(input)->size();
#endif
        return NetStream::of(input)->size();
    }
    int64_t seek_data(int64_t at) {
#ifdef NUVIO_VLC_STATIC
        if (torrent)
            return TorrentStream::of(input)->seek(at, SEEK_SET);
#endif
        return NetStream::of(input)->seek(at, SEEK_SET);
    }
    void close_stream() {
        stop = true;
        interrupt = true;
#ifdef NUVIO_VLC_STATIC
        if (torrent) {
            TorrentStream::close_avio(&input);
            return;
        }
#endif
        NetStream::close_avio(&input);
    }
};
static void interrupt(void *opaque) {
    static_cast<NuvioHttp *>(opaque)->interrupt = true;
}
static ssize_t read(stream_t *access, void *bytes, size_t count) {
    auto &s = *static_cast<NuvioHttp *>(access->p_sys);
    // The owner has closed this stream. EOF lets VLC prefetch/probing
    // unwind; EINTR can be retried indefinitely by adaptive child readers.
    if (nuvio_vlc_stream_cancelled(s.serial)) return 0;
    s.interrupt = false;
    vlc_interrupt_register(interrupt, &s);
    const int n = s.read_data(static_cast<uint8_t *>(bytes), int(std::min(count, size_t(INT32_MAX))));
    const int cancelled = vlc_interrupt_unregister();
    if (nuvio_vlc_stream_cancelled(s.serial)) return 0;
    if (n == AVERROR_EOF)
        return 0;
    if (n < 0 || cancelled) {
        errno = cancelled ? EINTR : EIO;
        return -1;
    }
    return n;
}
static int seek(stream_t *access, uint64_t position) {
    auto &s = *static_cast<NuvioHttp *>(access->p_sys);
    if (nuvio_vlc_stream_cancelled(s.serial)) return VLC_EGENERIC;
    s.interrupt = false;
    vlc_interrupt_register(interrupt, &s);
    const bool ok = position <= INT64_MAX && s.seek_data(int64_t(position)) >= 0;
    const int cancelled = vlc_interrupt_unregister();
    return ok && !cancelled ? VLC_SUCCESS : VLC_EGENERIC;
}
static int control(stream_t *access, int query, va_list args) {
    auto &s = *static_cast<NuvioHttp *>(access->p_sys);
    const int64_t size = s.size();
    switch (query) {
    case STREAM_CAN_SEEK:
        *va_arg(args, bool *) = size >= 0;
        return VLC_SUCCESS;
    case STREAM_CAN_FASTSEEK:
        *va_arg(args, bool *) = false;
        return VLC_SUCCESS;
    case STREAM_CAN_PAUSE:
    case STREAM_CAN_CONTROL_PACE:
        *va_arg(args, bool *) = true;
        return VLC_SUCCESS;
    case STREAM_GET_SIZE:
        if (size < 0)
            return VLC_EGENERIC;
        *va_arg(args, uint64_t *) = uint64_t(size);
        return VLC_SUCCESS;
    case STREAM_GET_PTS_DELAY:
        *va_arg(args, int64_t *) = 1000 * var_InheritInteger(access, "network-caching");
        return VLC_SUCCESS;
    case STREAM_SET_PAUSE_STATE:
        return VLC_SUCCESS;
    default:
        return VLC_EGENERIC;
    }
}
static void Close(vlc_object_t *object) {
    auto *access = reinterpret_cast<stream_t *>(object);
    auto *s = static_cast<NuvioHttp *>(access->p_sys);
    if (!s)
        return;
    s->close_stream();
    delete s;
    access->p_sys = nullptr;
}
static int Open(vlc_object_t *object) {
    auto *access = reinterpret_cast<stream_t *>(object);
    auto *s = new (std::nothrow) NuvioHttp;
    if (!s)
        return VLC_ENOMEM;
    s->serial = long(var_InheritInteger(access, "nuvio-stream-serial"));
    if (nuvio_vlc_stream_cancelled(s->serial)) { delete s; return VLC_EGENERIC; }
    std::vector<std::string> headers;
    char *encoded = var_InheritString(access, "nuvio-http-headers");
    if (encoded) {
        try {
            auto j = nlohmann::json::parse(encoded);
            if (j.is_array())
                headers = j.get<std::vector<std::string>>();
        } catch (...) {
            free(encoded);
            delete s;
            return VLC_EGENERIC;
        }
        free(encoded);
    }
    vlc_interrupt_register(interrupt, s);
    std::string why;
#ifdef NUVIO_VLC_STATIC
    s->torrent = TorrentStream::is_url(access->psz_url);
    if (s->torrent)
        s->input = TorrentStream::open_avio(access->psz_url, &s->interrupt, &why);
    else
#endif
        s->input = NetStream::open_avio(access->psz_url, headers, &s->stop, &why, false, &s->interrupt);
    vlc_interrupt_unregister();
    if (!s->input) {
        delete s;
        return VLC_EGENERIC;
    }
    std::string resolved = access->psz_url;
#ifdef NUVIO_VLC_STATIC
    if (!s->torrent)
#endif
        resolved = NetStream::of(s->input)->effective_url();
    if (resolved != access->psz_url) {
        // The stream exposes this URL as the base for relative HLS segments.
        char *url = strdup(resolved.c_str());
        if (!url) {
            s->close_stream();
            delete s;
            return VLC_ENOMEM;
        }
        free(access->psz_url);
        access->psz_url = url;
        access->psz_location = strstr(url, "://") + 3;
    }
    access->p_sys = s;
    access->pf_read = read;
    access->pf_block = nullptr;
    access->pf_seek = seek;
    access->pf_control = control;
    return VLC_SUCCESS;
}
vlc_module_begin() set_shortname("Nuvio HTTP") set_description("HTTP/HTTPS through Nuvio's network input")
    set_capability("access", 400) add_shortcut("http", "https", "torrent")
        add_string("nuvio-http-headers", "", "HTTP headers", "Private per-media headers", true)
        add_integer("nuvio-stream-serial", 0, "Stream generation", "Private cancellation generation", true)
            set_callbacks(Open, Close) vlc_module_end()
