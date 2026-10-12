// Independent HDR-to-SDR picture checks, using display-light test patches.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "hdr_tone_map.h"
#include <limits>
namespace {
double patch_pq(double nits) {
    const double p = std::pow(nits / 10000.0, 0.1593017578125);
    return std::pow((0.8359375 + 18.8515625 * p) / (1.0 + 18.6875 * p), 78.84375);
}
void hdr_patch(Source& source, double r, double g, double b, int shift = 0) {
    r = patch_pq(r); g = patch_pq(g); b = patch_pq(b);
    const double y = 0.2627 * r + 0.6780 * g + 0.0593 * b;
    source.fill(int(std::lround(64 + 876 * y)),
                int(std::lround(512 + 896 * (b - y) / 1.8814)),
                int(std::lround(512 + 896 * (r - y) / 1.4746)), shift);
}
void sdr_gpu_tests() {
    test("HDR-to-SDR keeps greys neutral, opens shadows and rolls highlights", [] {
        Source source(16,8,true); sx::VideoSurface surface;
        VideoFrame frame; frame.colors = YuvColors::Hdr10;
        // Display-light patch targets, rounded to allow 10-bit YUV input and
        // SDR readback quantization. Avoid comparing with the old film curve.
        const double nits[]{0,1,10,100,203,1000};
        const int low[]{0,34,90,188,215,253}, high[]{0,40,97,195,222,255};
        for (int shift : {0,6}) {
            int previous = -1;
            for (int i=0;i<6;++i) {
                hdr_patch(source,nits[i],nits[i],nits[i],shift);
                require(pack_video_yuv(source.view(shift),frame),"HDR patch packing failed");
                auto pixels = render(surface,frame);
                gray(pixels,0,low[i],high[i]);
                require(pixels[0]>previous,"HDR shadow/highlight levels collapsed");
                previous = pixels[0];
            }
        }
    });
    test("HDR-to-SDR preserves in-gamut warm colour balance", [] {
        // A BT.709 display-light patch with a known 4:2:1 RGB ratio,
        // encoded into BT.2020/PQ before the production GPU conversion.
        const double r=60,g=30,b=15;
        Source source(16,8,true); sx::VideoSurface surface;
        for (int shift : {0,6}) {
            hdr_patch(source,0.627404*r+0.329283*g+0.043313*b,
                      0.069097*r+0.919540*g+0.011362*b,
                      0.016391*r+0.088013*g+0.895595*b,shift);
            VideoFrame frame; frame.colors=YuvColors::Hdr10;
            require(pack_video_yuv(source.view(shift),frame),"colour patch packing failed");
            auto pixels=render(surface,frame);
            const double red=std::pow(pixels[0]/255.0,2.4);
            const double green=std::pow(pixels[1]/255.0,2.4);
            const double blue=std::pow(pixels[2]/255.0,2.4);
            require(std::abs(red/green-2)<0.06 && std::abs(red/blue-4)<0.15,
                    "tone mapping shifted warm colour towards orange");
        }
    });
    test("HDR-to-SDR fits wide-gamut primaries at their mapped luminance", [] {
        Source source(16,8,true); sx::VideoSurface surface;
        for (int channel=0;channel<3;++channel) {
            hdr_patch(source,channel==0?100:0,channel==1?100:0,channel==2?100:0);
            VideoFrame frame; frame.colors=YuvColors::Hdr10;
            require(pack_video_yuv(source.view(),frame),"gamut patch packing failed");
            auto pixels=render(surface,frame);
            const double luminance=0.2126*std::pow(pixels[0]/255.0,2.4)+
                                   0.7152*std::pow(pixels[1]/255.0,2.4)+
                                   0.0722*std::pow(pixels[2]/255.0,2.4);
            const double expected[]{0.2086,0.4068,0.0560};
            require(std::abs(luminance-expected[channel])<0.01,
                    "hard gamut clipping changed highlight luminance");
        }
    });
    test("HDR peak metadata is bounded, propagated and affects highlight rolloff", [] {
        require(hdr_peak_nits(4000,1000)==4000 && hdr_peak_nits(0,2000)==2000 &&
                hdr_peak_nits(0,0)==1000 && hdr_peak_nits(50,4000)==4000 &&
                hdr_peak_nits(10001,0)==1000 && hdr_peak_nits(std::numeric_limits<double>::quiet_NaN(),
                std::numeric_limits<double>::infinity())==1000,"bad source peak policy");
        Source source(16,8,true); hdr_patch(source,1000,1000,1000);
        VideoFrame frame; frame.colors=YuvColors::Hdr10; frame.hdr_peak_nits=4000;
        require(pack_video_yuv(source.view(),frame)&&frame.picture().hdr_peak_nits==4000,"peak metadata lost");
        sx::VideoSurface surface; auto high_peak=render(surface,frame);
        frame.hdr_peak_nits=1000; auto low_peak=render(surface,frame);
        require(low_peak[0]>=253 && high_peak[0]>=244 && high_peak[0]<=248,
                "source peak was ignored in highlight rolloff");
    });
}
}
