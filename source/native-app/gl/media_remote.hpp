// Media Remote and HDMI-CEC input. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "core/input.hpp"
#include "remote_bindings.hpp"
#include <array>
#include <cstdint>
namespace sx {
struct RemoteFrame {
  hui::PadSample pad;
  int transport = 0, seek = 0;
  RemoteButton observed;
}; // 1 toggle, 2 play, 3 pause
class MediaRemote {
public:
  ~MediaRemote();
  RemoteFrame poll(std::uint64_t now, bool learn = false);
  void configure(const std::vector<RemoteBinding>& bindings);
  void close();
  static std::uint32_t decode(const unsigned char *sample);

private:
  struct Source {
    bool connected = false;
    int handle = -1;
    std::uint32_t buttons = 0, physical = 0, blocked_pad = 0;
    unsigned key = 0, blocked_key = 0;
    std::uint64_t timestamp = 0;
  };
  std::array<Source, 2> sources_;
  std::uint64_t retry_at_ = 0, seek_at_ = 0;
  std::uint32_t previous_ = 0, suppressed_ = 0;
  std::vector<RemoteBinding> bindings_;
  bool learning_ = false;
  void suppress_held();
};
hui::InputFrame merge_remote(const hui::InputFrame &, const hui::InputFrame &);
} // namespace sx
