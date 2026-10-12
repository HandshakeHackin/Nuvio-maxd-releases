// IEC 61937 output through PS5 AudioOutEx, following Mortalfin's port protocol.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
struct AVCodecParameters;
struct AVPacket;
struct AVFormatContext;
struct AVIOContext;
namespace bitstream {
enum Kind { None=0, Ac3=1, Eac3=2, Dts=3 };
int kind(const AVCodecParameters*);
const char* name(int);
int rate(int);
int open_port(int kind, bool check_receiver);
bool close_port(int handle);
bool ensure_pcm();
bool receiver_supports(const uint8_t* info, size_t bytes, int kind);
class Packer {
public:
    ~Packer();
    bool start(const AVCodecParameters*);
    bool pack(const AVPacket*, std::vector<uint8_t>&);
    void clear();
private:
    static int write(void*, const uint8_t*, int);
    static int write(void* s,uint8_t* p,int n) {return write(s,static_cast<const uint8_t*>(p),n);}
    AVFormatContext* ctx_=nullptr;
    AVIOContext* io_=nullptr;
    std::vector<uint8_t> bytes_;
};
}
