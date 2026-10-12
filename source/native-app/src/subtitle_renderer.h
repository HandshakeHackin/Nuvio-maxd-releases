// Styled text and bitmap subtitle rendering, independent of the video decoder.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
struct AVCodecParameters;
struct AVPacket;
struct AVSubtitle;
struct SubtitleOverlay {
    int x=0, y=0, w=0, h=0;
    bool supported=false, changed=false;
    std::vector<uint8_t> rgba;
};
class SubtitleRenderer {
public:
    SubtitleRenderer();
    ~SubtitleRenderer();
    void configure(const std::string& fonts);
    void clear();
    void add_track(int id, const AVCodecParameters*, const std::string& language);
    void packet(int id, const AVPacket*, double pts, double duration, const AVSubtitle* decoded);
    bool external(int id, const std::string& utf8, const std::string& language);
    void seek();
    SubtitleOverlay render(int id, double seconds, int video_w, int video_h, double sar, float lift, int font=1, int colour=0, int effect=1, int background=0, int size=1);
private:
    struct Impl;
    std::unique_ptr<Impl> d_;
};
