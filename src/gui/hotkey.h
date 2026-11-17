#pragma once

#include <functional>
#include <memory>
#include <string>

namespace gui {

// A shortcut such as "Ctrl+Shift+Space", "Alt+F9", or a modifier-only chord
// such as "Cmd+Option" (no key: pressing just those modifiers together).
struct Hotkey {
  bool ctrl = false;
  bool alt = false;
  bool shift = false;
  bool super = false; // Cmd on macOS, Win on Windows
  std::string key;    // "SPACE", "A".."Z", "0".."9", "F1".."F24", or empty for a chord

  bool is_chord() const { return key.empty(); }
  int modifier_count() const { return ctrl + alt + shift + super; }
  bool valid() const;
  std::string to_string() const; // persisted form, e.g. "Cmd+Option"
  std::string display() const;   // for keycaps, e.g. "⌘+⌥" on macOS
};

// Parse "Ctrl+Shift+Space" style strings (case-insensitive; Cmd/Win/Super/Meta
// and Option/Opt/Alt are aliases). Returns an invalid hotkey on failure.
Hotkey parse_hotkey(const std::string &text);

// Build a hotkey from an SDL key event (for the shortcut recorder).
Hotkey hotkey_from_sdl(int sdl_keycode, int sdl_modifiers);

// Build a modifier-only chord from SDL modifier flags.
Hotkey chord_from_sdl(int sdl_modifiers);

// The platform default: ⌘⌥ on macOS, Ctrl+Shift+Space elsewhere.
const char *default_hotkey();

enum class HotkeyEvent {
  Pressed,  // shortcut went down
  Released, // shortcut released cleanly
  Cancelled // chord only: another key/mouse button was used while held, or held too long
};

// System-wide shortcut that fires even when Lisper is not focused.
//
// Key shortcuts:
//   Windows: RegisterHotKey (release detected by polling key state)
//   macOS:   Carbon RegisterEventHotKey (pressed + released events)
//   Linux:   XGrabKey on X11
// Modifier chords (e.g. ⌘⌥) are detected by polling modifier state, which
// needs no special permission on any platform.
//
// On Wayland, bind a compositor shortcut to `lisper_gui --toggle` instead.
// Callbacks run on a background thread; marshal to the UI thread yourself.
class GlobalHotkey {
public:
  using Callback = std::function<void(HotkeyEvent)>;

  GlobalHotkey();
  ~GlobalHotkey();

  GlobalHotkey(const GlobalHotkey &) = delete;
  GlobalHotkey &operator=(const GlobalHotkey &) = delete;

  bool register_hotkey(const Hotkey &hotkey, Callback callback, std::string &error);
  void unregister();

  static bool supported();

  struct Impl;
  struct ChordWatcher;

private:
  bool register_key(const Hotkey &hotkey, Callback callback, std::string &error);
  void unregister_key();

  std::unique_ptr<Impl> impl_;
  std::unique_ptr<ChordWatcher> chord_;
};

// Decides what a chord's raw modifier samples mean. Exposed for unit tests.
class ChordTracker {
public:
  // Feed one sample: whether all of the chord's modifiers are held, and
  // whether anything else (another modifier, key or mouse button) is down.
  // Returns true and sets `event` when something should be emitted.
  bool update(bool chord_down, bool other_input, HotkeyEvent &event);

private:
  bool armed_ = false;
  bool dirty_ = false;
};

} // namespace gui
