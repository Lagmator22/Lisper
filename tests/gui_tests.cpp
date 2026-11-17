// Tests for GUI-side logic that does not need a display.
#include "gui/hotkey.h"

#include <SDL2/SDL.h>

#include <iostream>

namespace {
int g_failures = 0;
#define CHECK(cond)                                                                                \
  do {                                                                                             \
    if (!(cond)) {                                                                                 \
      ++g_failures;                                                                                \
      std::cerr << "FAILED " << __FILE__ << ":" << __LINE__ << ": " #cond "\n";                    \
    }                                                                                              \
  } while (0)
} // namespace

int main() {
  using gui::parse_hotkey;

  const auto hk = parse_hotkey("Ctrl + Shift + Space");
  CHECK(hk.ctrl && hk.shift && !hk.alt && !hk.super);
  CHECK(hk.key == "SPACE");
  CHECK(hk.valid());
  CHECK(hk.to_string() == "Ctrl+Shift+Space");

  CHECK(parse_hotkey("alt+f9").valid());
  CHECK(parse_hotkey("F13").valid());          // bare function keys are fine
  CHECK(!parse_hotkey("A").valid());           // would hijack normal typing
  CHECK(!parse_hotkey("Shift+A").valid());     // ditto
  CHECK(!parse_hotkey("Ctrl+Shift").valid());  // no key
  CHECK(!parse_hotkey("Ctrl+Escape").valid()); // unsupported key
  CHECK(parse_hotkey("cmd+option+d").super);
  CHECK(parse_hotkey("Win+Alt+D").alt);

  const auto from_sdl = gui::hotkey_from_sdl(SDLK_d, KMOD_LCTRL | KMOD_LALT);
  CHECK(from_sdl.valid() && from_sdl.key == "D" && from_sdl.ctrl && from_sdl.alt);
  CHECK(gui::hotkey_from_sdl(SDLK_F5, 0).key == "F5");
  CHECK(gui::hotkey_from_sdl(SDLK_F20, 0).key == "F20");
  CHECK(!gui::hotkey_from_sdl(SDLK_LSHIFT, KMOD_LSHIFT).valid());

  // Round trip through the persisted string form.
  CHECK(parse_hotkey(from_sdl.to_string()).to_string() == from_sdl.to_string());

  std::cout << (g_failures == 0 ? "gui tests passed\n" : "gui tests FAILED\n");
  return g_failures == 0 ? 0 : 1;
}
