#include "gui/text_insert.h"

#include "subprocess.h"

#include <chrono>
#include <cstdlib>
#include <thread>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>
#endif

namespace gui {

#if defined(_WIN32)

bool send_paste_keystroke(std::string &error) {
  INPUT inputs[4] = {};
  inputs[0].type = INPUT_KEYBOARD;
  inputs[0].ki.wVk = VK_CONTROL;
  inputs[1].type = INPUT_KEYBOARD;
  inputs[1].ki.wVk = 'V';
  inputs[2].type = INPUT_KEYBOARD;
  inputs[2].ki.wVk = 'V';
  inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
  inputs[3].type = INPUT_KEYBOARD;
  inputs[3].ki.wVk = VK_CONTROL;
  inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
  if (SendInput(4, inputs, sizeof(INPUT)) != 4) {
    error = "Windows blocked the paste keystroke (the target app may be running as administrator).";
    return false;
  }
  return true;
}

bool paste_supported(std::string &hint) {
  hint.clear();
  return true;
}

#elif defined(__APPLE__)

bool request_accessibility_permission() {
  const void *keys[] = {kAXTrustedCheckOptionPrompt};
  const void *values[] = {kCFBooleanTrue};
  CFDictionaryRef options =
      CFDictionaryCreate(kCFAllocatorDefault, keys, values, 1, &kCFCopyStringDictionaryKeyCallBacks,
                         &kCFTypeDictionaryValueCallBacks);
  const bool trusted = AXIsProcessTrustedWithOptions(options);
  CFRelease(options);
  return trusted;
}

bool send_paste_keystroke(std::string &error) {
  if (!AXIsProcessTrusted()) {
    error = "Allow Lisper in System Settings > Privacy & Security > Accessibility to paste "
            "into other apps.";
    return false;
  }
  CGEventSourceRef source = CGEventSourceCreate(kCGEventSourceStateCombinedSessionState);
  CGEventRef down = CGEventCreateKeyboardEvent(source, static_cast<CGKeyCode>(kVK_ANSI_V), true);
  CGEventRef up = CGEventCreateKeyboardEvent(source, static_cast<CGKeyCode>(kVK_ANSI_V), false);
  CGEventSetFlags(down, kCGEventFlagMaskCommand);
  CGEventSetFlags(up, kCGEventFlagMaskCommand);
  CGEventPost(kCGAnnotatedSessionEventTap, down);
  CGEventPost(kCGAnnotatedSessionEventTap, up);
  CFRelease(down);
  CFRelease(up);
  if (source != nullptr) {
    CFRelease(source);
  }
  return true;
}

bool paste_supported(std::string &hint) {
  if (AXIsProcessTrusted()) {
    hint.clear();
    return true;
  }
  hint = "Grant Accessibility access so Lisper can paste into other apps.";
  return false;
}

#else // Linux and other Unix

namespace {

bool wayland_session() {
  const char *wayland = std::getenv("WAYLAND_DISPLAY");
  return wayland != nullptr && *wayland != '\0';
}

} // namespace

bool send_paste_keystroke(std::string &error) {
  if (wayland_session() && proc::run({"wtype", "-M", "ctrl", "v", "-m", "ctrl"}) == 0) {
    return true;
  }
  if (proc::run({"xdotool", "key", "--clearmodifiers", "ctrl+v"}) == 0) {
    return true;
  }
  error = wayland_session() ? "Install wtype (or xdotool for XWayland apps) to paste "
                              "automatically. The text is on your clipboard."
                            : "Install xdotool to paste automatically. The text is on your "
                              "clipboard.";
  return false;
}

bool paste_supported(std::string &hint) {
  const bool have_xdotool = proc::program_available("xdotool", "--version");
  const bool have_wtype = wayland_session() && proc::program_available("wtype", "-h");
  if (have_xdotool || have_wtype) {
    hint.clear();
    return true;
  }
  hint = wayland_session() ? "Install wtype to paste into other apps automatically."
                           : "Install xdotool to paste into other apps automatically.";
  return false;
}

#endif

} // namespace gui
