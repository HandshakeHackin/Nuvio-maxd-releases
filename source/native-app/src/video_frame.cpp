#include "video_frame.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

bool video_dimensions_valid(int w, int h) {
	return w > 0 && h > 0 && w <= 4096 && h <= 4096;
}

VideoPicture VideoFrame::picture() const {
	VideoPicture p;
	p.w = w; p.h = h; p.format = format; p.shift = shift;
	p.colors = colors; p.full_range = full_range; p.sar = sar;
	p.hdr_peak_nits = hdr_peak_nits;
	p.hdr10_compatible = hdr10_compatible;
	if (pixels.empty()) return p;
	if (format == VideoFormat::Rgba) p.rgba = pixels.data();
	else {
		p.y = pixels.data();
		p.uv = p.y + size_t(w) * h * (format == VideoFormat::Nv12_10 ? 2 : 1);
	}
	return p;
}

bool pack_video_yuv(const YuvPicture& src, VideoFrame& dst) {
	if (!video_dimensions_valid(src.width, src.height) || !src.y || !src.u ||
	    (!src.interleaved && !src.v) || (src.wide && src.shift != 0 && src.shift != 6)) return false;
	const int bytes = src.wide ? 2 : 1;
	const int cw = (src.width + 1) / 2, ch = (src.height + 1) / 2;
	const int y_bytes = src.width * bytes;
	const int c_bytes = cw * bytes * (src.interleaved ? 2 : 1);
	const int v_stride = src.v_stride ? src.v_stride : src.c_stride;
	auto enough = [](int stride, int n) { return stride != std::numeric_limits<int>::min() && std::abs(stride) >= n; };
	if (!enough(src.y_stride, y_bytes) || !enough(src.c_stride, c_bytes) ||
	    (!src.interleaved && !enough(v_stride, cw * bytes))) return false;
	const size_t y_size = size_t(y_bytes) * src.height;
	dst.pixels.resize(y_size + size_t(cw) * ch * 2 * bytes);
	for (int row = 0; row < src.height; ++row)
		std::memcpy(dst.pixels.data() + size_t(row) * y_bytes,
		            src.y + ptrdiff_t(row) * src.y_stride, y_bytes);
	uint8_t* uv = dst.pixels.data() + y_size;
	for (int row = 0; row < ch; ++row) {
		uint8_t* out = uv + size_t(row) * cw * 2 * bytes;
		const uint8_t* u = src.u + ptrdiff_t(row) * src.c_stride;
		if (src.interleaved) std::memcpy(out, u, cw * 2 * bytes);
		else {
			const uint8_t* v = src.v + ptrdiff_t(row) * v_stride;
			for (int x = 0; x < cw; ++x) {
				std::memcpy(out + x * 2 * bytes, u + x * bytes, bytes);
				std::memcpy(out + (x * 2 + 1) * bytes, v + x * bytes, bytes);
			}
		}
	}
	dst.w = src.width; dst.h = src.height;
	dst.format = src.wide ? VideoFormat::Nv12_10 : VideoFormat::Nv12;
	dst.shift = src.wide ? src.shift : 0;
	return true;
}

int video_sample_shift(const YuvPicture& src) {
	if (!src.wide || !src.y || !src.u || src.width <= 0 || src.height <= 0) return 0;
	auto high = [](const uint8_t* row, int count) {
		for (int x = 0; x < count; x += 31) {
			uint16_t sample;
			std::memcpy(&sample, row + x * 2, sizeof(sample));
			if (sample > 1023) return true;
		}
		return false;
	};
	for (int y = 0; y < src.height; y += 97)
		if (high(src.y + ptrdiff_t(y) * src.y_stride, src.width)) return 6;
	const int cw = (src.width + 1) / 2, ch = (src.height + 1) / 2;
	for (int y = 0; y < ch; y += 47) {
		if (high(src.u + ptrdiff_t(y) * src.c_stride, cw * (src.interleaved ? 2 : 1))) return 6;
		if (!src.interleaved && src.v &&
		    high(src.v + ptrdiff_t(y) * (src.v_stride ? src.v_stride : src.c_stride), cw)) return 6;
	}
	return 0;
}

VideoRect video_fit(int w, int h, double sar, float canvas_w, float canvas_h) {
	if (w <= 0 || h <= 0 || canvas_w <= 0 || canvas_h <= 0) return {};
	if (!std::isfinite(sar) || sar <= 0) sar = 1.0;
	const double dar = double(w) * sar / h;
	float dw = canvas_w, dh = float(canvas_w / dar);
	if (dh > canvas_h) { dh = canvas_h; dw = float(canvas_h * dar); }
	return {(canvas_w - dw) * 0.5f, (canvas_h - dh) * 0.5f, dw, dh};
}
