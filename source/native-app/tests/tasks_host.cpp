#include "tasks.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <future>
#include <iostream>
void dlog(const char *, ...) {}
int main() {
  Tasks tasks;
  tasks.start(1);
  std::promise<void> started, release, urgent;
  auto ready = release.get_future().share();
  tasks.run([&] {
    started.set_value();
    ready.wait();
  });
  started.get_future().wait();
  std::atomic<int> ran = 0;
  tasks.run([&] { ++ran; });
  tasks.foreground([&] { urgent.set_value(); });
  assert(urgent.get_future().wait_for(std::chrono::seconds(1)) ==
         std::future_status::ready);
  assert(ran == 0);
  release.set_value();
  std::promise<void> ordinary;
  tasks.run([&] { ordinary.set_value(); });
  assert(ordinary.get_future().wait_for(std::chrono::seconds(1)) ==
         std::future_status::ready);
  tasks.stop();
  assert(ran == 1);
  tasks.start(1);
  std::promise<void> restarted;
  tasks.run([&] { restarted.set_value(); });
  assert(restarted.get_future().wait_for(std::chrono::seconds(1)) ==
         std::future_status::ready);
  tasks.stop();
  std::cout << "Foreground metadata remains responsive while ordinary work is "
               "busy; restart passes\n";
}
