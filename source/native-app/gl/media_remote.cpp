#include "media_remote.hpp"
#include "system_status.hpp"
#include <cstring>
extern "C" {
int sceUserServiceGetInitialUser(int *);
int scePadOpen(int, int, int, const void *);
int scePadRead(int, void *, int);
int scePadClose(int);
}
namespace sx {
namespace {
constexpr std::uint32_t toggle = 1u << 24, play = 1u << 25, pause = 1u << 26,
                        rewind = 1u << 27, forward = 1u << 28;
constexpr std::uint32_t media = toggle | play | pause | rewind | forward;
struct alignas(8) Sample {
  unsigned char bytes[120];
};
} // namespace
std::uint32_t MediaRemote::decode(const unsigned char *s) {
  std::uint32_t bits = 0;
  std::memcpy(&bits, s, 4);
  bits &= remote_pad_mask;
  switch (s[0x6c]) {
  case 13:
    bits |= hui::pad_bits::kCross;
    break;
  case 15:
    bits |= hui::pad_bits::kCircle;
    break;
  case 18:
  case 27:
    bits |= hui::pad_bits::kOptions;
    break;
  case 22:
    bits |= play;
    break;
  case 26:
    bits |= pause;
    break;
  case 45:
    bits |= toggle;
    break;
  case 23:
    bits |= rewind;
    break;
  case 24:
    bits |= forward;
    break;
  default:
    break;
  }
  return bits;
}
MediaRemote::~MediaRemote() { close(); }
void MediaRemote::close() {
  for (auto &s : sources_) {
    if (s.handle >= 0)
      scePadClose(s.handle);
    s = {};
  }
  previous_ = suppressed_ = 0;
  retry_at_ = seek_at_ = 0;
}
void MediaRemote::suppress_held() {
 for(auto& s:sources_){s.blocked_pad|=s.physical;s.blocked_key=s.key;s.buttons=0;}
 previous_=0;seek_at_=0;
}
void MediaRemote::configure(const std::vector<RemoteBinding>& bindings) {
 if(bindings_==bindings)return;
 bindings_.clear();
 for(const auto& b:bindings)if(b.button.valid()&&valid_remote_action(b.action)&&bindings_.size()<16)bindings_.push_back(b);
 suppress_held();
}
RemoteFrame MediaRemote::poll(std::uint64_t now, bool learn) {
 if(learn!=learning_){suppress_held();learning_=learn;}
 const auto focus=read_system_status();
 const bool foreground=focus.valid&&!focus.background&&!focus.overlaid;
  if (now >= retry_at_) {
    int user = -1;
    const bool have_user = sceUserServiceGetInitialUser(&user) == 0;
    for (unsigned i = 0; i < sources_.size(); ++i)
      if (sources_[i].handle < 0 && (i == 0 || have_user))
        sources_[i].handle =
            scePadOpen(i == 0 ? 0xff : user, i == 0 ? 16 : 2, 0, nullptr);
    retry_at_ = now + 5000000;
  }
  RemoteFrame out;
  std::uint32_t bits = 0, pressed = 0;
  bool any = false;
  for (auto &src : sources_) {
    if (src.handle < 0)
      continue;
    Sample samples[64];
    const int n = scePadRead(src.handle, samples, 64);
    if (n < 0) {
      scePadClose(src.handle);
      src = {};
      continue;
    }
    for (int i = 0; i < n; ++i) {
      const auto *data = samples[i].bytes;
      std::uint64_t ts = 0;
      std::memcpy(&ts, data + 0x50, 8);
      if (ts && ts == src.timestamp)
        continue;
      src.timestamp = ts;
      std::int32_t connected = 0;
      std::memcpy(&connected, data + 0x4c, 4);
      src.connected = connected != 0;
      std::uint32_t physical=0;std::memcpy(&physical,data,4);
      const bool intercepted=(physical&hui::pad_bits::kIntercepted)!=0;
      physical=connected?(physical&remote_pad_mask):0;
      // Ignore idle/no-key sentinel values rather than learning a release.
      const unsigned key=connected&&data[0x6c]!=0xff?data[0x6c]:0;
      const auto fresh=physical&~src.physical;
      const bool key_edge=key&&key!=src.key;
      src.blocked_pad&=physical;
      if(key!=src.blocked_key)src.blocked_key=0;
      if(!foreground||intercepted){src.blocked_pad|=physical;src.blocked_key=key;}
      const auto visible_pad=physical&~src.blocked_pad;
      const auto visible_key=src.blocked_key?0:key;
      if(learn&&foreground&&!intercepted&&!out.observed.valid()){
        const int source=int(&src-sources_.data());
        if(key_edge&&visible_key){
          unsigned char raw[120]{};raw[0x6c]=static_cast<unsigned char>(key);
          out.observed={source,true,key,(fresh|decode(raw))&remote_pad_mask};
        }else if(fresh&visible_pad){
          const auto single=fresh&visible_pad;
          if((single&(single-1))==0)out.observed={source,false,single,0};
        }
      }
      unsigned char filtered[120]{};std::memcpy(filtered,&visible_pad,4);filtered[0x6c]=static_cast<unsigned char>(visible_key);
      auto value=foreground&&!intercepted&&!learn?decode(filtered):0;
      if(!learn&&foreground&&!intercepted){
        std::uint32_t removed=0,assigned=0,raw_aliases=0;
        for(const auto& binding:bindings_)if(binding.button.key&&binding.button.source==int(&src-sources_.data())&&visible_key==binding.button.code){
          unsigned char raw[120]{};raw[0x6c]=static_cast<unsigned char>(binding.button.code);
          raw_aliases|=binding.button.aliases|(decode(raw)&remote_pad_mask);
        }
        for(const auto& binding:bindings_){
          const auto& button=binding.button;
          if(button.source!=int(&src-sources_.data()))continue;
          const bool held=button.key?visible_key==button.code:(visible_pad&button.code)!=0;
          if(!held||(!button.key&&(raw_aliases&button.code)))continue;
          unsigned char raw[120]{};
          if(button.key)raw[0x6c]=static_cast<unsigned char>(button.code);
          removed|=button.aliases|decode(raw)|(button.key?0:button.code);
          using namespace hui::pad_bits;
          switch(binding.action){
          case RemoteAction::search:assigned|=kTriangle;break;
          case RemoteAction::options:assigned|=kOptions;break;
          case RemoteAction::select:assigned|=kCross;break;
          case RemoteAction::back:assigned|=kCircle;break;
          case RemoteAction::toggle:assigned|=toggle;break;
          case RemoteAction::play:assigned|=play;break;
          case RemoteAction::pause:assigned|=pause;break;
          case RemoteAction::rewind:assigned|=rewind;break;
          case RemoteAction::forward:assigned|=forward;break;
          case RemoteAction::up:assigned|=kUp;break;
          case RemoteAction::down:assigned|=kDown;break;
          case RemoteAction::left:assigned|=kLeft;break;
          case RemoteAction::right:assigned|=kRight;break;
          default:break;
          }
        }
        value=(value&~removed)|assigned;
      }
      pressed|=value&~src.buttons;
      src.buttons=value;src.physical=physical;src.key=key;
    }
    bits |= src.buttons;
    any |= src.connected;
  }
  const auto physical = bits;
  if (bits || pressed) {
    const auto status = read_system_status();
    if (!status.valid || status.background || status.overlaid) {
      suppressed_ |= bits | pressed;
      bits = pressed = 0;
    }
  }
  suppressed_ &= physical;
  bits &= ~suppressed_;
  pressed &= ~suppressed_;
  if (pressed & toggle)
    out.transport = 1;
  else if (pressed & play)
    out.transport = 2;
  else if (pressed & pause)
    out.transport = 3;
  const auto seeking = bits & (rewind | forward);
  if (seeking && (!(previous_ & (rewind | forward)) || now >= seek_at_)) {
    out.seek = (seeking & rewind) ? -1 : 1;
    seek_at_ = now + 400000;
  }
  if (!seeking)
    seek_at_ = 0;
  previous_ = bits;
  out.pad.buttons = (bits | pressed) & ~media;
  out.pad.connected = any;
  out.pad.timestamp_us = now;
  return out;
}
hui::InputFrame merge_remote(const hui::InputFrame &a,
                             const hui::InputFrame &b) {
  auto out = a;
  out.pressed |= b.pressed;
  out.held |= b.held;
  out.released = (a.released | b.released) & ~out.held;
  if (out.nav == hui::Direction::none) {
    out.nav = b.nav;
    out.nav_repeat = b.nav_repeat;
    out.nav_from_stick = b.nav_from_stick;
  }
  out.connected = a.connected || b.connected;
  out.focus_lost = a.focus_lost || b.focus_lost;
  return out;
}
} // namespace sx
