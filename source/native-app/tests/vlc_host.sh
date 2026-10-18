#!/usr/bin/env bash
# Real host libVLC and the production HTTP plugin; no fake decoder/API.
set -euo pipefail
R=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
read -r -a cflags <<< "${HOST_LIFECYCLE_CFLAGS:-$(pkg-config --cflags libavformat libavcodec libavutil libswresample)}"
read -r -a libs <<< "${HOST_LIFECYCLE_LIBS:-$(pkg-config --libs libavformat libavcodec libavutil libcurl)}"
read -r -a vcflags <<< "${HOST_VLC_CFLAGS:-$(pkg-config --cflags libvlc)}"
read -r -a vlibs <<< "${HOST_VLC_LIBS:-$(pkg-config --libs libvlc)}"
# Debian exposes libvlccore.pc; Ubuntu packages its private headers under
# vlc/plugins without that pkg-config file. Honor explicit host overrides.
core_cflags=${HOST_VLC_CORE_CFLAGS:-}
core_libs=${HOST_VLC_CORE_LIBS:-}
if pkg-config --exists libvlccore; then
    core_cflags=${core_cflags:-$(pkg-config --cflags libvlccore)}
    core_libs=${core_libs:-$(pkg-config --libs libvlccore)}
elif pkg-config --exists vlc-plugin; then
    core_cflags=${core_cflags:-$(pkg-config --cflags vlc-plugin)}
    core_libs=${core_libs:-$(pkg-config --libs vlc-plugin)}
else
    core_cflags=${core_cflags:--I$(pkg-config --variable=includedir libvlc)/vlc/plugins}
    core_libs=${core_libs:--lvlccore}
fi
read -r -a ccflags <<< "$core_cflags"
read -r -a clibs <<< "$core_libs"
O=$R/test-output
mkdir -p "$O/vlc-plugins/access"
${HOST_CXX:-g++} -std=c++20 -O0 -g -Wall -Wextra -Werror -pthread -DNUVIO_HAVE_VLC -DPLATFORM_PS5_NATIVE \
 -I"$R/src" -I"$R/third_party" "${cflags[@]}" "${vcflags[@]}" \
 "$R/tests/vlc_host.cpp" "$R/src/"{vlc_backend,vlc_cancel,pcm_out,bitstream,video_frame,util}.cpp \
 "${vlibs[@]}" "${libs[@]}" -rdynamic -o "$O/vlc-host"
${HOST_CXX:-g++} -std=c++20 -O1 -fPIC -shared -D__PLUGIN__ -pthread \
 -I"$R/src" -I"$R/third_party" "${cflags[@]}" "${ccflags[@]}" \
 "$R/src/"{vlc_http,netstream,http,util}.cpp "${clibs[@]}" "${libs[@]}" -o "$O/vlc-plugins/access/libnuviohttp_plugin.so"
export VLC_PLUGIN_PATH="$O/vlc-plugins${VLC_PLUGIN_PATH:+:$VLC_PLUGIN_PATH}"
python3 "$R/tests/vlc_network_test.py"
