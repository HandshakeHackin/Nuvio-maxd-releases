// Optional libVLC engine; no VLC types cross the application boundary.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "video_frame.h"
#include <memory>
#include <string>
#include <vector>
class VlcBackend {
  public:
    struct Options {
        std::string url;
        std::vector<std::string> headers, audio_languages;
        double start = 0;
        bool surround = false, paused = false, software = false;
        int audio = -1, subtitle = -2;
        double subtitle_delay = 0;
    };
    struct Track {
        int stream = -1;
        std::string label, lang;
        bool is_default = false;
    };
    static bool available();
    static void prepare();
    VlcBackend();
    ~VlcBackend();
    bool open(const Options &);
    void close();
    int state() const; // Player::State values, without sharing Player's implementation
    std::string error() const;
    bool paused() const;
    void set_paused(bool);
    bool buffering(int *percent = nullptr) const;
    double position() const;
    double duration() const;
    void seek(double);
    const VideoPicture *present();
    int serial() const;
    int picture_serial() const;
    std::vector<Track> tracks(bool subtitles) const;
    int audio_stream() const;
    void select_audio(int);
    void select_subtitle(int, double delay);
    std::string audio_output() const;
    std::string codec_summary() const;
    std::string stats() const;
    void log_stats();

  private:
    struct Impl;
    std::unique_ptr<Impl> d_;
};
