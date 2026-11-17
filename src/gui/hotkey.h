#pragma once

#include <functional>
#include <memory>
#include <string>

namespace gui {

// A parsed shortcut such as "Ctrl+Shift+Space" or "Alt+F9".
struct Hotkey {
  bool ctrl = false;
  bool alt = false;
  bool shift = false;
  bool super = false; // Cmd on macOS, Win on Windows
  std::string key;    // "Space", "A".."Z", "0".."9", "F1".."F24"

  bool valid() const;
  std::string to_string() const;
};

// Parse "Ctrl+Shift+Space" style strings (case-insensitive; Cmd/Win/Super/Meta
// are aliases). Returns an invalid hotkey on failure.
Hotkey parse_hotkey(const std::string &text);

// Build a hotkey from an SDL key event (used by the "press a new shortcut"
// capture in Settings). Returns an invalid hotkey for unsupported keys.
Hotkey hotkey_from_sdl(int sdl_keycode, int sdl_modifiers);

// System-wide hotkey that fires even when Lisper is not focused.
//
//   Windows: RegisterHotKey (release detected by polling key state)
//   macOS:   Carbon RegisterEventHotKey (pressed + released events)
//   Linux:   XGrabKey on X11. On Wayland, bind a compositor shortcut to
//            `lisper_gui --toggle` instead.
//
// Callbacks run on a background thread; marshal to the UI thread yourself.
class GlobalHotkey {
public:
  using Callback = std::function<void(bool pressed)>;

  GlobalHotkey();
  ~GlobalHotkey();

  GlobalHotkey(const GlobalHotkey &) = delete;
  GlobalHotkey &operator=(const GlobalHotkey &) = delete;

  // Replace any registered hotkey. Returns false (with `error`) if the
  // platform refused it, e.g. because another app already owns the shortcut.
  bool register_hotkey(const Hotkey &hotkey, Callback callback, std::string &error);
  void unregister();

  static bool supported();

  struct Impl;

private:
  std::unique_ptr<Impl> impl_;
};

} // namespace gui
