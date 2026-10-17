// Media Remote and HDMI-CEC input. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "core/input.hpp"
#include <array>
#include <cstdint>
namespace sx {
struct RemoteFrame {
  hui::PadSample pad;
  int transport = 0, seek = 0;
}; // 1 toggle, 2 play, 3 pause
class MediaRemote {
public:
  ~MediaRemote();
  RemoteFrame poll(std::uint64_t now);
  void close();
  static std::uint32_t decode(const unsigned char *sample);

private:
  struct Source {
    bool connected = false;
    int handle = -1;
    std::uint32_t buttons = 0;
    std::uint64_t timestamp = 0;
  };
  std::array<Source, 2> sources_;
  std::uint64_t retry_at_ = 0, seek_at_ = 0;
  std::uint32_t previous_ = 0, suppressed_ = 0;
};
hui::InputFrame merge_remote(const hui::InputFrame &, const hui::InputFrame &);
} // namespace sx
