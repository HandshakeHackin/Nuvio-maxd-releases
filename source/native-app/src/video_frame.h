// Owned, native-resolution video planes shared by the decoder and presenter.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "yuv_convert.h"
#include <cstddef>
#include <vector>

enum class VideoFormat { Rgba, Nv12, Nv12_10 };

struct VideoPicture {
	const uint8_t* rgba = nullptr;
	const uint8_t* y = nullptr;
	const uint8_t* uv = nullptr;
	int w = 0, h = 0;
	VideoFormat format = VideoFormat::Rgba;
	int shift = 0; // 10-bit words: 0 for low bits, 6 for P010 high bits
	YuvColors colors = YuvColors::Bt709;
	bool full_range = false;
	bool hdr10_compatible = false; // PQ / BT.2020 non-constant luminance; excludes DV profile 5
	float hdr_peak_nits = 1000.0f; // SDR tone-map reference only; native PQ bypasses it
	double sar = 1.0;
};

struct VideoFrame {
	std::vector<uint8_t> pixels;
	int w = 0, h = 0;
	VideoFormat format = VideoFormat::Rgba;
	int shift = 0;
	YuvColors colors = YuvColors::Bt709;
	bool full_range = false;
	bool hdr10_compatible = false;
	float hdr_peak_nits = 1000.0f;
	double sar = 1.0;
	VideoPicture picture() const;
};

// Copies the visible rows before the decoder reuses its frame buffer. Keeps
// 4:2:0 and 10-bit samples intact, including cropped and odd-sized pictures.
bool pack_video_yuv(const YuvPicture& src, VideoFrame& dst);
// Examine luma AND chroma: a black high-aligned intro can have zero luma.
int video_sample_shift(const YuvPicture& src);
bool video_dimensions_valid(int w, int h);

struct VideoRect { float x, y, w, h; };
VideoRect video_fit(int w, int h, double sar, float canvas_w, float canvas_h);
