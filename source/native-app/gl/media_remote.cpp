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
  bits &= 0x0010fffeu;
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
RemoteFrame MediaRemote::poll(std::uint64_t now) {
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
      const auto value = connected ? decode(data) : 0;
      pressed |= value & ~src.buttons;
      src.buttons = value;
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
