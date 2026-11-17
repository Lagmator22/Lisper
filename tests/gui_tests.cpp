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
  CHECK(!parse_hotkey("Ctrl").valid());        // one modifier alone is not a chord
  CHECK(!parse_hotkey("Ctrl+Escape").valid()); // unsupported key
  CHECK(parse_hotkey("cmd+option+d").super);
  CHECK(parse_hotkey("Win+Alt+D").alt);

  const auto from_sdl = gui::hotkey_from_sdl(SDLK_d, KMOD_LCTRL | KMOD_LALT);
  CHECK(from_sdl.valid() && from_sdl.key == "D" && from_sdl.ctrl && from_sdl.alt);
  CHECK(gui::hotkey_from_sdl(SDLK_F5, 0).key == "F5");
  CHECK(gui::hotkey_from_sdl(SDLK_F20, 0).key == "F20");
  CHECK(!gui::hotkey_from_sdl(SDLK_LSHIFT, KMOD_LSHIFT).valid());

  // Modifier-only chords (e.g. Cmd+Option on macOS)
  const auto chord = parse_hotkey("Cmd+Option");
  CHECK(chord.is_chord() && chord.valid() && chord.super && chord.alt);
  CHECK(parse_hotkey(chord.to_string()).to_string() == chord.to_string());
  CHECK(parse_hotkey("\xE2\x8C\x98+\xE2\x8C\xA5").super); // ⌘+⌥ symbols parse too
  CHECK(gui::chord_from_sdl(KMOD_LGUI | KMOD_LALT).valid());
  CHECK(!gui::chord_from_sdl(KMOD_LSHIFT).valid());
  CHECK(parse_hotkey(gui::default_hotkey()).valid());

  {
    using gui::HotkeyEvent;
    gui::ChordTracker t;
    HotkeyEvent e{};
    // Clean tap: press -> release
    CHECK(!t.update(false, false, e));
    CHECK(t.update(true, false, e) && e == HotkeyEvent::Pressed);
    CHECK(!t.update(true, false, e)); // held: nothing new
    CHECK(t.update(false, false, e) && e == HotkeyEvent::Released);
    // Part of another shortcut (e.g. Cmd+Option+Esc): cancelled, no release
    CHECK(t.update(true, false, e) && e == HotkeyEvent::Pressed);
    CHECK(t.update(true, true, e) && e == HotkeyEvent::Cancelled);
    CHECK(!t.update(true, true, e));
    CHECK(!t.update(false, false, e));
    // Chord pressed while another key is already down: ignored entirely
    CHECK(!t.update(true, true, e));
    CHECK(!t.update(false, false, e));
  }

  // Round trip through the persisted string form.
  CHECK(parse_hotkey(from_sdl.to_string()).to_string() == from_sdl.to_string());

  std::cout << (g_failures == 0 ? "gui tests passed\n" : "gui tests FAILED\n");
  return g_failures == 0 ? 0 : 1;
}
