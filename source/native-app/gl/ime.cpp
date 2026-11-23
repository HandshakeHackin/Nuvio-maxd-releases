#include "ime.hpp"

#include "util.h"

#include <cstdint>
#include <cstring>

#ifdef PLATFORM_PS5_NATIVE

extern "C"
{
    int sceUserServiceInitialize(const void *params);
    int sceUserServiceGetForegroundUser(int *user);
    int sceImeDialogInit(const void *param, const void *extended);
    int sceImeDialogGetStatus(void);
    int sceImeDialogGetResult(void *result);
    int sceImeDialogTerm(void);
}

namespace sx
{

namespace
{

// OrbisImeDialogSetting. SDL's driver for the console fills in only the user, the
// length and the buffer, and leaves everything else zero; so does this, apart from where
// the dialog is put.
struct ImeParam
{
    std::uint32_t user_id;
    std::uint32_t type;
    std::uint64_t supported_languages;
    std::int32_t enter_label;
    std::int32_t input_method;
    void *filter;
    std::uint32_t option;
    std::uint32_t max_text_length;
    std::uint16_t *buffer;
    float pos_x, pos_y;
    std::int32_t horizontal_alignment, vertical_alignment;
    const std::uint16_t *placeholder;
    const std::uint16_t *title;
    std::uint8_t reserved[16];
};
static_assert(sizeof(ImeParam) == 96, "the dialog's setting is 96 bytes");

struct ImeResult
{
    std::int32_t end_status; // 0: the user accepted the text; 1 and 2: closed
    std::int32_t reserved[12];
};

constexpr int kMaxChars = 2048;
constexpr int kRunning = 1, kFinished = 2;
std::uint16_t g_text[kMaxChars + 8];
ImeParam g_param;
std::uint16_t g_title[128];

void utf16_of(const std::string& input,std::uint16_t* out,std::size_t capacity) {
    if(!is_valid_utf8(input)||capacity<1)return;
    std::size_t used=0;
    for(std::size_t i=0;i<input.size();){
        const auto c=static_cast<unsigned char>(input[i++]);
        int extra=c<128?0:(c&0xe0)==0xc0?1:(c&0xf0)==0xe0?2:3;
        std::uint32_t cp=c&(extra==0?127:extra==1?31:extra==2?15:7);
        for(int n=0;n<extra;++n)cp=(cp<<6)|(static_cast<unsigned char>(input[i++])&63);
        if(cp==0||used+(cp>=0x10000?2:1)>=capacity)break;
        if(cp>=0x10000){cp-=0x10000;out[used++]=0xd800+(cp>>10);out[used++]=0xdc00+(cp&1023);}
        else out[used++]=static_cast<std::uint16_t>(cp);
    }
    out[used]=0;
}

// The dialog writes UTF-16. Some firmware versions have been seen to widen the
// units to 32 bits; both are read.
std::string utf8_of(const std::uint16_t *u)
{
    const bool wide = u[0] != 0 && u[1] == 0 && u[2] != 0;
    std::string out;
    const auto put = [&](std::uint32_t cp) {
        if (cp < 0x80)
            out += static_cast<char>(cp);
        else if (cp < 0x800)
        {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
        else if (cp < 0x10000)
        {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
        else
        {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    };
    const int step = wide ? 2 : 1;
    for (int i = 0; i < kMaxChars; i += step)
    {
        std::uint32_t c = u[i];
        if (c == 0)
            break;
        if (c >= 0xD800 && c < 0xDC00 && u[i + step] >= 0xDC00 && u[i + step] < 0xE000)
        {
            c = 0x10000 + ((c - 0xD800) << 10) + (u[i + step] - 0xDC00);
            i += step;
        }
        put(c);
    }
    return out;
}

} // namespace

bool Ime::open(const std::string& title, bool secret,const std::string& initial)
{
    if (active_)
        return true;
    const int init = sceUserServiceInitialize(nullptr);
    (void)init; // already initialised is fine
    int user = 0;
    if (sceUserServiceGetForegroundUser(&user) != 0)
    {
        dlog("ime: no foreground user");
        return false;
    }
    std::memset(g_text, 0, sizeof(g_text));
    if(!secret)utf16_of(initial,g_text,kMaxChars+1);
    std::memset(&g_param, 0, sizeof(g_param));
    g_param.user_id = static_cast<std::uint32_t>(user);
    // Public OrbisImeOption ABI: PASSWORD=4, NO_LEARNING=32,
    // NO_AUTO_CAPITALIZATION=2, DISABLE_RESUME=256, DISABLE_AUTO_SPACE=512.
    // Password mode requires Basic Latin (OrbisImeType value 1), including on
    // older firmware. Default text fields keep the multilingual type (0).
    if (secret) {
        g_param.type = 1;
        g_param.option = 4u | 32u | 2u | 256u | 512u;
    }
    g_param.max_text_length = kMaxChars;
    g_param.buffer = g_text;
    std::memset(g_title,0,sizeof(g_title));
    utf16_of(title,g_title,128);
    g_param.title=g_title;
    // The dialog sits at the top right, not in the corner it opens in by default: its top right corner
    // is put at this point of the 1920 x 1080 screen (alignments: 0 left/top, 1 centre, 2 right/bottom).
    g_param.horizontal_alignment = 2;
    g_param.vertical_alignment = 0;
    g_param.pos_x = 1890.0f;
    g_param.pos_y = 30.0f;
    const int r = sceImeDialogInit(&g_param, nullptr);
    if (r != 0)
    {
        dlog("ime: sceImeDialogInit 0x%08x", static_cast<unsigned>(r));
        return false;
    }
    active_ = true;
    seen_running_ = false;
    opened_ = now_seconds();
    return true;
}

Ime::State Ime::poll(std::string *text)
{
    if (!active_)
        return State::idle;
    const int status = sceImeDialogGetStatus();
    if (status == kRunning)
    {
        seen_running_ = true;
        return State::open;
    }
    if (status != kFinished)
    {
        // Never started: give up after a few seconds rather than wait for ever.
        if (!seen_running_ && now_seconds() - opened_ > 3.0)
        {
            sceImeDialogTerm();
            std::memset(g_text,0,sizeof(g_text));
            active_ = false;
            return State::cancelled;
        }
        if(seen_running_){sceImeDialogTerm();std::memset(g_text,0,sizeof(g_text));active_=false;return State::cancelled;}
        return State::open;
    }
    ImeResult result;
    std::memset(&result, 0, sizeof(result));
    State out = State::cancelled;
    if (sceImeDialogGetResult(&result) == 0 && result.end_status == 0)
    {
        if (text)
            *text = utf8_of(g_text);
        out = State::done;
    }
    sceImeDialogTerm();
    std::memset(g_text,0,sizeof(g_text));
    active_ = false;
    return out;
}

void Ime::cancel() {
    if(active_)sceImeDialogTerm();
    std::memset(g_text,0,sizeof(g_text));active_=false;
}

} // namespace sx

#else // not on the console: there is no keyboard dialog

namespace sx
{
bool Ime::open(const std::string&, bool,const std::string&)
{
    return false;
}
Ime::State Ime::poll(std::string *)
{
    return State::idle;
}
void Ime::cancel(){active_=false;}
} // namespace sx

#endif
