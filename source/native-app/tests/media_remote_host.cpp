#include "media_remote.hpp"
#include "system_status.hpp"
#include <array>
#include <cassert>
#include <cstring>
#include <deque>
#include <iostream>
using Sample = std::array<unsigned char, 120>;
static std::deque<Sample> queues[2];
static sx::SystemStatus status{true, false, false};
static int closed = 0;
static uint64_t ts = 1;
extern "C" int sceUserServiceGetInitialUser(int *p) {
  *p = 1;
  return 0;
}
extern "C" int scePadOpen(int, int port, int, const void *) {
  return port == 16 ? 0 : 1;
}
extern "C" int scePadClose(int) {
  ++closed;
  return 0;
}
extern "C" int scePadRead(int h, void *p, int n) {
  int count = 0;
  while (!queues[h].empty() && count < n) {
    std::memcpy(static_cast<char *>(p) + 120 * count, queues[h].front().data(),
                120);
    queues[h].pop_front();
    ++count;
  }
  return count;
}
namespace sx {
SystemStatus read_system_status() { return status; }
} // namespace sx
void sample(int source, int key, uint32_t bits = 0, bool connected = true) {
  Sample b{};
  std::memcpy(b.data(), &bits, 4);
  int c = connected;
  std::memcpy(b.data() + 0x4c, &c, 4);
  std::memcpy(b.data() + 0x50, &ts, 8);
  ++ts;
  b[0x6c] = key;
  queues[source].push_back(b);
}
int main() {
  sx::MediaRemote remote;
  sample(0, 13);
  sample(0, 0);
  auto f = remote.poll(1);
  assert(f.pad.buttons & hui::pad_bits::kCross);
  assert(f.pad.connected);
  assert(remote.poll(2).pad.buttons == 0);
  sample(1, 22);
  assert(remote.poll(3).transport == 2);
  assert(remote.poll(4).transport == 0);
  sample(1, 0);
  remote.poll(5);
  sample(1, 26);
  assert(remote.poll(6).transport == 3);
  sample(1, 0);
  sample(0, 24);
  assert(remote.poll(10).seek == 1);
  assert(remote.poll(200000).seek == 0);
  assert(remote.poll(400010).seek == 1);
  status.overlaid = true;
  remote.poll(500000);
  status.overlaid = false;
  assert(remote.poll(1000000).seek == 0);
  sample(0, 0);
  remote.poll(1000001);
  sample(0, 24);
  assert(remote.poll(1000002).seek == 1);
  sample(0, 0, 0, false);
  sample(1, 0, 0, false);
  assert(!remote.poll(1000003).pad.connected);
  hui::InputFrame a{}, b{};
  a.held = 1;
  b.held = 2;
  a.released = 2;
  b.pressed = 2;
  auto merged = sx::merge_remote(a, b);
  assert(merged.held == 3 && merged.released == 0 && merged.pressed == 2);
  remote.close();
  assert(closed == 2);
  std::cout << "Media Remote/CEC tap, transport, repeat, focus and disconnect "
               "tests passed\n";
}
