#pragma once

#include <string>

namespace gui {

// Send a "paste" keystroke (Cmd+V / Ctrl+V) to whichever application has
// keyboard focus.
//
//   macOS:   CGEventPost (requires the Accessibility permission)
//   Windows: SendInput
//   Linux:   wtype on Wayland, xdotool on X11 (external helpers)
//
// Returns false with a human-readable reason when no method is available.
bool send_paste_keystroke(std::string &error);

// True if pasting is likely to work (helper present / permission granted).
bool paste_supported(std::string &hint);

#ifdef __APPLE__
// Ask macOS to show the Accessibility permission prompt if not yet granted.
bool request_accessibility_permission();
#endif

} // namespace gui
