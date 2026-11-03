#include "ime.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
struct Param {
  uint32_t user, type;
  uint64_t langs;
  int32_t label, method;
  void *filter;
  uint32_t option, max;
  uint16_t *buffer;
};
static Param captured{};
static int status = 2, init_rc = 0;
extern "C" {
int sceUserServiceInitialize(const void *) { return 0; }
int sceUserServiceGetForegroundUser(int *user) {
  *user = 1;
  return 0;
}
int sceImeDialogInit(const void *p, const void *) {
  captured = *static_cast<const Param *>(p);
  if ((captured.option & 4) && captured.type != 1)
    return static_cast<int>(0x80bc0030u);
  return init_rc;
}
int sceImeDialogGetStatus() { return status; }
int sceImeDialogGetResult(void *p) {
  *static_cast<int32_t *>(p) = 0;
  return 0;
}
int sceImeDialogTerm() { return 0; }
}
int main() {
  sx::Ime ime;
  std::string text;
  assert(ime.open("Password", true));
  assert(captured.type == 1 && captured.option == 0x326);
  captured.buffer[0] = 's';
  captured.buffer[1] = 'e';
  captured.buffer[2] = 0;
  assert(ime.poll(&text) == sx::Ime::State::done && text == "se");
  assert(captured.buffer[0] == 0 && !ime.active());
  assert(ime.open("Username"));
  assert(captured.type == 0 && captured.option == 0);
  status = 1;
  ime.poll(&text);
  captured.buffer[0] = 'x';
  status = 0;
  assert(ime.poll(&text) == sx::Ime::State::cancelled && !ime.active() &&
         captured.buffer[0] == 0);
  init_rc = static_cast<int>(0x80bc0030u);
  assert(!ime.open("Password", true) && !ime.active());
  init_rc = 0;
  status = 2;
  assert(ime.open("Password", true) && captured.type == 1 &&
         captured.option == 0x326);
  assert(ime.poll(&text) == sx::Ime::State::done && !ime.active());
  std::cout << "Password IME uses Basic Latin with masking and learning/resume "
               "protections; Unicode text, failure recovery and buffer "
               "clearing passed\n";
}
