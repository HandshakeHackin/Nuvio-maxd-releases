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
  // Unknown delivered keys can be learned and assigned without firing an old action.
  sx::MediaRemote mapped;
  sample(1,99,hui::pad_bits::kCross);auto learned=mapped.poll(2000000,true);
  assert(learned.observed.valid()&&learned.observed.source==1&&learned.observed.code==99);
  assert(!learned.pad.buttons&&!learned.transport&&!learned.seek);
  std::vector<sx::RemoteBinding> bindings;assert(sx::set_remote_binding(bindings,learned.observed,sx::RemoteAction::search));
  mapped.configure(bindings);assert(!mapped.poll(2000001).pad.buttons); // Learning press is consumed.
  sample(1,0);mapped.poll(2000002);sample(1,99,hui::pad_bits::kCross);
  f=mapped.poll(2000003);assert(f.pad.buttons==hui::pad_bits::kTriangle&&!f.transport);
  sample(1,0);mapped.poll(2000004);
  // A mapped standard key must suppress its original transport operation.
  assert(sx::set_remote_binding(bindings,{1,true,24,0},sx::RemoteAction::toggle));mapped.configure(bindings);
  sample(1,24);f=mapped.poll(2000005);assert(f.transport==1&&!f.seek);assert(mapped.poll(2000006).transport==0);
  // Both sources have independent IDs, held seek repeats, and short taps survive a batch.
  sample(1,0);mapped.poll(2000007);assert(sx::set_remote_binding(bindings,{0,false,hui::pad_bits::kSquare,0},sx::RemoteAction::rewind));mapped.configure(bindings);
  sample(0,0,hui::pad_bits::kSquare);assert(mapped.poll(2000008).seek==-1);
  assert(mapped.poll(2200008).seek==0);assert(mapped.poll(2400008).seek==-1);
  sample(0,0);sample(1,99);sample(1,0);f=mapped.poll(2400009);assert(f.pad.buttons==hui::pad_bits::kTriangle);
  // OS overlays/interception never learn or execute keys, including held keys after returning.
  status.overlaid=true;sample(1,99);assert(!mapped.poll(2400010,true).observed.valid());
  status.overlaid=false;assert(!mapped.poll(2400011,true).observed.valid());
  sample(1,0);mapped.poll(2400012,true);sample(1,99,hui::pad_bits::kIntercepted);
  assert(!mapped.poll(2400013,true).observed.valid());sample(1,0);mapped.poll(2400014,true);
  sample(1,255);assert(!mapped.poll(2400015,true).observed.valid());
  // Existing held keys do not become a fresh candidate when capture starts.
  mapped.poll(2400016);sample(1,13);mapped.poll(2400017);assert(!mapped.poll(2400018,true).observed.valid());
  sample(1,0);mapped.poll(2400019,true);sample(1,13);assert(mapped.poll(2400020,true).observed.code==13);
  // Simultaneous mappings are combined after suppressing originals, independent of order.
  sample(1,0);mapped.poll(2500000);bindings.clear();
  sx::set_remote_binding(bindings,{1,false,hui::pad_bits::kCross,0},sx::RemoteAction::options);
  sx::set_remote_binding(bindings,{1,false,hui::pad_bits::kOptions,0},sx::RemoteAction::search);
  mapped.configure(bindings);sample(1,0,hui::pad_bits::kCross|hui::pad_bits::kOptions);
  assert(mapped.poll(2500001).pad.buttons==(hui::pad_bits::kOptions|hui::pad_bits::kTriangle));
  sample(1,0);mapped.poll(2500002);
  sx::set_remote_binding(bindings,{1,true,13,hui::pad_bits::kCross},sx::RemoteAction::back);mapped.configure(bindings);
  sample(1,13,hui::pad_bits::kCross);assert(mapped.poll(2500003).pad.buttons==hui::pad_bits::kCircle);
  // Saved configuration has strict bounds, distinct source IDs, and no OS/controller remapping.
  nlohmann::json saved=nlohmann::json::object();sx::save_remote_bindings(saved,bindings);assert(sx::load_remote_bindings(saved)==bindings);
  saved["remote_button_mappings"].push_back({{"source",1},{"kind",0},{"code",99},{"aliases",0},{"action",999}});
  saved["remote_button_mappings"].push_back(saved["remote_button_mappings"][0]);
  assert(sx::load_remote_bindings(saved)==bindings);
  assert(!sx::set_remote_binding(bindings,{-1,true,99,0},sx::RemoteAction::search));
  assert(!sx::set_remote_binding(bindings,{1,true,256,0},sx::RemoteAction::search));
  assert(!sx::set_remote_binding(bindings,{1,false,3,0},sx::RemoteAction::search));
  std::vector<sx::RemoteBinding> capped;for(unsigned i=1;i<=16;++i)assert(sx::set_remote_binding(capped,{1,true,i,0},sx::RemoteAction::search));
  assert(!sx::set_remote_binding(capped,{1,true,17,0},sx::RemoteAction::search));
  assert(sx::set_remote_binding(capped,{1,true,1,0},sx::RemoteAction::disabled)&&capped.size()==16);
  sx::save_remote_bindings(saved,capped);assert(sx::load_remote_bindings(saved)==capped);
  mapped.close();
  std::cout << "Remote remapping: learning, consumed capture, raw/CEC/pad identity, alias suppression, repeat, focus, persistence and bounds passed\n";
  std::cout << "Media Remote/CEC tap, transport, repeat, focus and disconnect "
               "tests passed\n";
}
