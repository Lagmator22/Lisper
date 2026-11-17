#include "gui/hotkey.h"

#include <SDL2/SDL.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <future>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <Carbon/Carbon.h>
#elif defined(LISPER_HAVE_X11)
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <poll.h>
#endif

namespace gui {

namespace {

std::string upper(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
  return s;
}

bool is_function_key(const std::string &key, int &number) {
  if (key.size() < 2 || key.size() > 3 || key[0] != 'F') {
    return false;
  }
  try {
    number = std::stoi(key.substr(1));
  } catch (...) {
    return false;
  }
  return number >= 1 && number <= 24;
}

} // namespace

bool Hotkey::valid() const {
  if (is_chord()) {
    // Two or more modifiers alone, e.g. Cmd+Option. Shift+X chords would fire
    // while typing capitals, so Shift must be paired with a non-Shift modifier.
    return modifier_count() >= 2;
  }
  int fn = 0;
  const bool fkey = is_function_key(key, fn);
  const bool alnum = key.size() == 1 && std::isalnum(static_cast<unsigned char>(key[0]));
  if (!fkey && !alnum && key != "SPACE") {
    return false;
  }
  // Plain letters/space without modifiers would hijack normal typing.
  return fkey || ctrl || alt || super || (shift && key == "SPACE");
}

std::string Hotkey::to_string() const {
  std::string out;
  auto add = [&](const char *part) {
    if (!out.empty()) {
      out += "+";
    }
    out += part;
  };
  if (ctrl) {
    add("Ctrl");
  }
#if defined(__APPLE__)
  if (super) {
    add("Cmd");
  }
  if (alt) {
    add("Option");
  }
#else
  if (alt) {
    add("Alt");
  }
#endif
  if (shift) {
    add("Shift");
  }
#if defined(_WIN32)
  if (super) {
    add("Win");
  }
#elif !defined(__APPLE__)
  if (super) {
    add("Super");
  }
#endif
  if (!key.empty()) {
    add(key == "SPACE" ? "Space" : key.c_str());
  }
  return out;
}

std::string Hotkey::display() const {
#if defined(__APPLE__)
  // macOS convention: ⌃ ⌥ ⇧ ⌘ then the key.
  std::string out;
  auto add = [&](const char *part) {
    if (!out.empty()) {
      out += "+";
    }
    out += part;
  };
  if (ctrl) {
    add("\xE2\x8C\x83"); // ⌃
  }
  if (alt) {
    add("\xE2\x8C\xA5"); // ⌥
  }
  if (shift) {
    add("\xE2\x87\xA7"); // ⇧
  }
  if (super) {
    add("\xE2\x8C\x98"); // ⌘
  }
  if (!key.empty()) {
    add(key == "SPACE" ? "Space" : key.c_str());
  }
  return out;
#else
  return to_string();
#endif
}

const char *default_hotkey() {
#if defined(__APPLE__)
  return "Cmd+Option";
#else
  return "Ctrl+Shift+Space";
#endif
}

Hotkey parse_hotkey(const std::string &text) {
  Hotkey hk;
  std::stringstream ss(text);
  std::string part;
  while (std::getline(ss, part, '+')) {
    part.erase(std::remove_if(part.begin(), part.end(),
                              [](unsigned char c) { return std::isspace(c) != 0; }),
               part.end());
    const std::string p = upper(part);
    if (p.empty()) {
      continue;
    }
    if (p == "CTRL" || p == "CONTROL" || p == "\xE2\x8C\x83") {
      hk.ctrl = true;
    } else if (p == "ALT" || p == "OPTION" || p == "OPT" || p == "\xE2\x8C\xA5") {
      hk.alt = true;
    } else if (p == "SHIFT" || p == "\xE2\x87\xA7") {
      hk.shift = true;
    } else if (p == "CMD" || p == "COMMAND" || p == "WIN" || p == "SUPER" || p == "META" ||
               p == "\xE2\x8C\x98") {
      hk.super = true;
    } else {
      hk.key = p;
    }
  }
  return hk;
}

Hotkey chord_from_sdl(int mods) {
  Hotkey hk;
  hk.ctrl = (mods & KMOD_CTRL) != 0;
  hk.alt = (mods & KMOD_ALT) != 0;
  hk.shift = (mods & KMOD_SHIFT) != 0;
  hk.super = (mods & KMOD_GUI) != 0;
  return hk;
}

Hotkey hotkey_from_sdl(int keycode, int mods) {
  Hotkey hk = chord_from_sdl(mods);
  if (keycode == SDLK_SPACE) {
    hk.key = "SPACE";
  } else if (keycode >= SDLK_a && keycode <= SDLK_z) {
    hk.key = std::string(1, static_cast<char>('A' + (keycode - SDLK_a)));
  } else if (keycode >= SDLK_0 && keycode <= SDLK_9) {
    hk.key = std::string(1, static_cast<char>('0' + (keycode - SDLK_0)));
  } else if (keycode >= SDLK_F1 && keycode <= SDLK_F12) {
    hk.key = "F" + std::to_string(1 + keycode - SDLK_F1);
  } else if (keycode >= SDLK_F13 && keycode <= SDLK_F24) {
    hk.key = "F" + std::to_string(13 + keycode - SDLK_F13);
  } else {
    hk.key = "?"; // unsupported key: never valid, never a chord
  }
  return hk;
}

bool ChordTracker::update(bool chord_down, bool other_input, HotkeyEvent &event) {
  if (!armed_) {
    if (chord_down && !other_input) {
      armed_ = true;
      dirty_ = false;
      event = HotkeyEvent::Pressed;
      return true;
    }
    return false;
  }
  if (!chord_down) {
    armed_ = false;
    if (!dirty_) {
      event = HotkeyEvent::Released;
      return true;
    }
    return false;
  }
  if (other_input && !dirty_) {
    // Part of a different shortcut (⌘⌥Esc, ⌘⌥-drag, AltGr+key): back off.
    dirty_ = true;
    event = HotkeyEvent::Cancelled;
    return true;
  }
  return false;
}

// ---------------------------------------------------------------------------
// Modifier chord watcher: polls the global modifier/key state. Polling keeps
// this permission-free (no event taps or keyboard hooks).
// ---------------------------------------------------------------------------

namespace {

struct InputSample {
  bool ctrl = false;
  bool alt = false;
  bool shift = false;
  bool super = false;
  bool other = false; // any non-modifier key or mouse button
};

#if defined(_WIN32)

bool down(int vk) {
  return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

struct Sampler {
  bool ok() const { return true; }
  InputSample sample() {
    InputSample s;
    s.ctrl = down(VK_CONTROL);
    s.alt = down(VK_MENU);
    s.shift = down(VK_SHIFT);
    s.super = down(VK_LWIN) || down(VK_RWIN);
    for (int vk = 0x01; vk <= 0xFE && !s.other; ++vk) {
      switch (vk) {
      case VK_CONTROL:
      case VK_LCONTROL:
      case VK_RCONTROL:
      case VK_MENU:
      case VK_LMENU:
      case VK_RMENU:
      case VK_SHIFT:
      case VK_LSHIFT:
      case VK_RSHIFT:
      case VK_LWIN:
      case VK_RWIN:
      case VK_CAPITAL:
      case VK_NUMLOCK:
      case VK_SCROLL:
        continue;
      default:
        s.other = down(vk);
      }
    }
    return s;
  }
};

#elif defined(__APPLE__)

struct Sampler {
  bool ok() const { return true; }
  InputSample sample() {
    InputSample s;
    const CGEventFlags flags = CGEventSourceFlagsState(kCGEventSourceStateHIDSystemState);
    s.ctrl = (flags & kCGEventFlagMaskControl) != 0;
    s.alt = (flags & kCGEventFlagMaskAlternate) != 0;
    s.shift = (flags & kCGEventFlagMaskShift) != 0;
    s.super = (flags & kCGEventFlagMaskCommand) != 0;
    for (CGKeyCode k = 0; k < 128 && !s.other; ++k) {
      if ((k >= 54 && k <= 63) || k == kVK_Function) {
        continue; // modifier keys (Cmd, Shift, CapsLock, Option, Ctrl, Fn)
      }
      s.other = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, k);
    }
    s.other = s.other ||
              CGEventSourceButtonState(kCGEventSourceStateHIDSystemState, kCGMouseButtonLeft) ||
              CGEventSourceButtonState(kCGEventSourceStateHIDSystemState, kCGMouseButtonRight);
    return s;
  }
};

#elif defined(LISPER_HAVE_X11)

struct Sampler {
  Display *dpy = XOpenDisplay(nullptr);
  std::vector<KeyCode> ctrl, alt, shift, super, ignore;

  Sampler() {
    if (dpy == nullptr) {
      return;
    }
    auto codes = [&](std::initializer_list<KeySym> syms, std::vector<KeyCode> &out) {
      for (KeySym sym : syms) {
        const KeyCode c = XKeysymToKeycode(dpy, sym);
        if (c != 0) {
          out.push_back(c);
        }
      }
    };
    codes({XK_Control_L, XK_Control_R}, ctrl);
    codes({XK_Alt_L, XK_Alt_R, XK_Meta_L, XK_Meta_R, XK_ISO_Level3_Shift}, alt);
    codes({XK_Shift_L, XK_Shift_R}, shift);
    codes({XK_Super_L, XK_Super_R, XK_Hyper_L, XK_Hyper_R}, super);
    codes({XK_Caps_Lock, XK_Num_Lock}, ignore);
  }
  ~Sampler() {
    if (dpy != nullptr) {
      XCloseDisplay(dpy);
    }
  }
  Sampler(const Sampler &) = delete;
  Sampler &operator=(const Sampler &) = delete;

  bool ok() const { return dpy != nullptr; }

  InputSample sample() {
    char keys[32] = {};
    XQueryKeymap(dpy, keys);
    auto is_down = [&](KeyCode c) { return (keys[c / 8] & (1 << (c % 8))) != 0; };
    auto any = [&](const std::vector<KeyCode> &v) {
      return std::any_of(v.begin(), v.end(), is_down);
    };
    InputSample s;
    s.ctrl = any(ctrl);
    s.alt = any(alt);
    s.shift = any(shift);
    s.super = any(super);
    for (int c = 8; c < 256 && !s.other; ++c) {
      const auto kc = static_cast<KeyCode>(c);
      auto in = [&](const std::vector<KeyCode> &v) {
        return std::find(v.begin(), v.end(), kc) != v.end();
      };
      if (in(ctrl) || in(alt) || in(shift) || in(super) || in(ignore)) {
        continue;
      }
      s.other = is_down(kc);
    }
    Window root_ret, child_ret;
    int rx, ry, wx, wy;
    unsigned int mask = 0;
    if (XQueryPointer(dpy, DefaultRootWindow(dpy), &root_ret, &child_ret, &rx, &ry, &wx, &wy,
                      &mask)) {
      s.other = s.other || (mask & (Button1Mask | Button2Mask | Button3Mask)) != 0;
    }
    return s;
  }
};

#else

struct Sampler {
  bool ok() const { return false; }
  InputSample sample() { return {}; }
};

#endif

} // namespace

struct GlobalHotkey::ChordWatcher {
  std::thread thread;
  std::atomic<bool> stop{false};
  std::atomic<int> state{0}; // 0 starting, 1 running, -1 failed

  void start(const Hotkey hk, GlobalHotkey::Callback callback) {
    thread = std::thread([this, hk, callback] {
      Sampler sampler;
      if (!sampler.ok()) {
        state = -1;
        return;
      }
      state = 1;
      ChordTracker tracker;
      while (!stop) {
        const InputSample s = sampler.sample();
        const bool chord_down = (!hk.ctrl || s.ctrl) && (!hk.alt || s.alt) &&
                                (!hk.shift || s.shift) && (!hk.super || s.super);
        const bool extra_modifier = (s.ctrl && !hk.ctrl) || (s.alt && !hk.alt) ||
                                    (s.shift && !hk.shift) || (s.super && !hk.super);
        HotkeyEvent event;
        if (tracker.update(chord_down, s.other || extra_modifier, event)) {
          callback(event);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(12));
      }
    });
    while (state == 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
  }

  ~ChordWatcher() {
    stop = true;
    if (thread.joinable()) {
      thread.join();
    }
  }
};

bool GlobalHotkey::register_hotkey(const Hotkey &hotkey, Callback callback, std::string &error) {
  unregister();
  if (!hotkey.valid()) {
    error = "Invalid shortcut";
    return false;
  }
  if (!hotkey.is_chord()) {
    return register_key(hotkey, std::move(callback), error);
  }
  chord_ = std::make_unique<ChordWatcher>();
  chord_->start(hotkey, std::move(callback));
  if (chord_->state < 0) {
    chord_.reset();
    error = "Modifier-only shortcuts need an X11 session here. Choose a key shortcut or bind "
            "`lisper_gui --toggle` in your system settings.";
    return false;
  }
  return true;
}

void GlobalHotkey::unregister() {
  chord_.reset();
  unregister_key();
}

// ===========================================================================
// Windows
// ===========================================================================
#if defined(_WIN32)

struct GlobalHotkey::Impl {
  std::thread thread;
  std::atomic<DWORD> thread_id{0};
  Callback callback;
  UINT vk = 0;
  UINT mods = 0;
  std::atomic<bool> ok{false};
  std::string error;

  static UINT to_vk(const std::string &key) {
    int fn = 0;
    if (key == "SPACE") {
      return VK_SPACE;
    }
    if (is_function_key(key, fn)) {
      return static_cast<UINT>(VK_F1 + fn - 1);
    }
    return static_cast<UINT>(key[0]); // 'A'-'Z' and '0'-'9' match their VK codes
  }

  void run(std::promise<void> *ready) {
    thread_id = GetCurrentThreadId();
    MSG msg;
    PeekMessage(&msg, nullptr, WM_USER, WM_USER, PM_NOREMOVE); // create queue
    ok = RegisterHotKey(nullptr, 1, mods | MOD_NOREPEAT, vk) != 0;
    if (!ok) {
      error = "The shortcut is already in use by another application.";
    }
    ready->set_value();
    if (!ok) {
      return;
    }
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
      if (msg.message == WM_HOTKEY) {
        callback(HotkeyEvent::Pressed);
        // Wait for the key to be released so push-to-talk can stop.
        while ((GetAsyncKeyState(static_cast<int>(vk)) & 0x8000) != 0) {
          std::this_thread::sleep_for(std::chrono::milliseconds(15));
        }
        callback(HotkeyEvent::Released);
      }
    }
    UnregisterHotKey(nullptr, 1);
  }
};

GlobalHotkey::GlobalHotkey() : impl_(std::make_unique<Impl>()) {}
GlobalHotkey::~GlobalHotkey() {
  unregister();
}

bool GlobalHotkey::supported() {
  return true;
}

bool GlobalHotkey::register_key(const Hotkey &hotkey, Callback callback, std::string &error) {
  unregister_key();
  if (!hotkey.valid()) {
    error = "Invalid shortcut";
    return false;
  }
  impl_->callback = std::move(callback);
  impl_->vk = Impl::to_vk(hotkey.key);
  impl_->mods = (hotkey.ctrl ? MOD_CONTROL : 0) | (hotkey.alt ? MOD_ALT : 0) |
                (hotkey.shift ? MOD_SHIFT : 0) | (hotkey.super ? MOD_WIN : 0);
  std::promise<void> ready;
  auto fut = ready.get_future();
  impl_->thread = std::thread([this, &ready] { impl_->run(&ready); });
  fut.wait();
  if (!impl_->ok) {
    error = impl_->error;
    impl_->thread.join();
    return false;
  }
  return true;
}

void GlobalHotkey::unregister_key() {
  if (impl_ && impl_->thread.joinable()) {
    PostThreadMessage(impl_->thread_id, WM_QUIT, 0, 0);
    impl_->thread.join();
  }
}

// ===========================================================================
// macOS
// ===========================================================================
#elif defined(__APPLE__)

struct GlobalHotkey::Impl {
  EventHotKeyRef ref = nullptr;
  EventHandlerRef handler = nullptr;
  Callback callback;

  static UInt32 to_keycode(const std::string &key) {
    static const struct {
      const char *name;
      UInt32 code;
    } kMap[] = {
        {"A", kVK_ANSI_A},    {"B", kVK_ANSI_B}, {"C", kVK_ANSI_C}, {"D", kVK_ANSI_D},
        {"E", kVK_ANSI_E},    {"F", kVK_ANSI_F}, {"G", kVK_ANSI_G}, {"H", kVK_ANSI_H},
        {"I", kVK_ANSI_I},    {"J", kVK_ANSI_J}, {"K", kVK_ANSI_K}, {"L", kVK_ANSI_L},
        {"M", kVK_ANSI_M},    {"N", kVK_ANSI_N}, {"O", kVK_ANSI_O}, {"P", kVK_ANSI_P},
        {"Q", kVK_ANSI_Q},    {"R", kVK_ANSI_R}, {"S", kVK_ANSI_S}, {"T", kVK_ANSI_T},
        {"U", kVK_ANSI_U},    {"V", kVK_ANSI_V}, {"W", kVK_ANSI_W}, {"X", kVK_ANSI_X},
        {"Y", kVK_ANSI_Y},    {"Z", kVK_ANSI_Z}, {"0", kVK_ANSI_0}, {"1", kVK_ANSI_1},
        {"2", kVK_ANSI_2},    {"3", kVK_ANSI_3}, {"4", kVK_ANSI_4}, {"5", kVK_ANSI_5},
        {"6", kVK_ANSI_6},    {"7", kVK_ANSI_7}, {"8", kVK_ANSI_8}, {"9", kVK_ANSI_9},
        {"SPACE", kVK_Space}, {"F1", kVK_F1},    {"F2", kVK_F2},    {"F3", kVK_F3},
        {"F4", kVK_F4},       {"F5", kVK_F5},    {"F6", kVK_F6},    {"F7", kVK_F7},
        {"F8", kVK_F8},       {"F9", kVK_F9},    {"F10", kVK_F10},  {"F11", kVK_F11},
        {"F12", kVK_F12},     {"F13", kVK_F13},  {"F14", kVK_F14},  {"F15", kVK_F15},
        {"F16", kVK_F16},     {"F17", kVK_F17},  {"F18", kVK_F18},  {"F19", kVK_F19},
        {"F20", kVK_F20},
    };
    for (const auto &entry : kMap) {
      if (key == entry.name) {
        return entry.code;
      }
    }
    return UINT32_MAX;
  }

  static OSStatus on_event(EventHandlerCallRef, EventRef event, void *user) {
    auto *self = static_cast<Impl *>(user);
    const bool pressed = GetEventKind(event) == kEventHotKeyPressed;
    if (self->callback) {
      self->callback(pressed ? HotkeyEvent::Pressed : HotkeyEvent::Released);
    }
    return noErr;
  }
};

GlobalHotkey::GlobalHotkey() : impl_(std::make_unique<Impl>()) {}
GlobalHotkey::~GlobalHotkey() {
  unregister();
}

bool GlobalHotkey::supported() {
  return true;
}

bool GlobalHotkey::register_key(const Hotkey &hotkey, Callback callback, std::string &error) {
  unregister_key();
  const UInt32 code = Impl::to_keycode(hotkey.key);
  if (!hotkey.valid() || code == UINT32_MAX) {
    error = "Invalid shortcut";
    return false;
  }
  impl_->callback = std::move(callback);

  const EventTypeSpec types[] = {{kEventClassKeyboard, kEventHotKeyPressed},
                                 {kEventClassKeyboard, kEventHotKeyReleased}};
  InstallApplicationEventHandler(&Impl::on_event, 2, types, impl_.get(), &impl_->handler);

  UInt32 mods = 0;
  mods |= hotkey.ctrl ? controlKey : 0;
  mods |= hotkey.alt ? optionKey : 0;
  mods |= hotkey.shift ? shiftKey : 0;
  mods |= hotkey.super ? cmdKey : 0;
  const EventHotKeyID id = {'LSPR', 1};
  if (RegisterEventHotKey(code, mods, id, GetApplicationEventTarget(), 0, &impl_->ref) != noErr) {
    error = "The shortcut is already in use by another application.";
    unregister_key();
    return false;
  }
  return true;
}

void GlobalHotkey::unregister_key() {
  if (!impl_) {
    return;
  }
  if (impl_->ref != nullptr) {
    UnregisterEventHotKey(impl_->ref);
    impl_->ref = nullptr;
  }
  if (impl_->handler != nullptr) {
    RemoveEventHandler(impl_->handler);
    impl_->handler = nullptr;
  }
}

// ===========================================================================
// Linux / X11
// ===========================================================================
#elif defined(LISPER_HAVE_X11)

struct GlobalHotkey::Impl {
  std::thread thread;
  std::atomic<bool> stop{false};
  Callback callback;
  KeySym keysym = NoSymbol;
  unsigned int mods = 0;
  std::atomic<int> state{0}; // 0 pending, 1 ok, -1 failed
  std::string error;

  static KeySym to_keysym(const std::string &key) {
    int fn = 0;
    if (key == "SPACE") {
      return XK_space;
    }
    if (is_function_key(key, fn)) {
      return static_cast<KeySym>(XK_F1 + fn - 1);
    }
    if (key.size() == 1 && std::isalpha(static_cast<unsigned char>(key[0]))) {
      return static_cast<KeySym>(XK_a + (std::tolower(static_cast<unsigned char>(key[0])) - 'a'));
    }
    if (key.size() == 1 && std::isdigit(static_cast<unsigned char>(key[0]))) {
      return static_cast<KeySym>(XK_0 + (key[0] - '0'));
    }
    return NoSymbol;
  }

  static int grab_error(Display *, XErrorEvent *e) {
    if (e->error_code == BadAccess) {
      grab_failed() = true;
    }
    return 0;
  }
  static bool &grab_failed() {
    static bool failed = false;
    return failed;
  }

  void run() {
    Display *dpy = XOpenDisplay(nullptr);
    if (dpy == nullptr) {
      error = "Global shortcuts need an X11 session. On Wayland, bind a system shortcut to "
              "`lisper_gui --toggle`.";
      state = -1;
      return;
    }
    const Window root = DefaultRootWindow(dpy);
    const KeyCode code = XKeysymToKeycode(dpy, keysym);
    Bool detectable = False;
    XkbSetDetectableAutoRepeat(dpy, True, &detectable);

    // Grab with every combination of the lock modifiers so CapsLock/NumLock
    // do not break the shortcut.
    const unsigned int locks[] = {0, LockMask, Mod2Mask, LockMask | Mod2Mask};
    grab_failed() = false;
    auto previous = XSetErrorHandler(&Impl::grab_error);
    for (unsigned int lock : locks) {
      XGrabKey(dpy, code, mods | lock, root, False, GrabModeAsync, GrabModeAsync);
    }
    XSync(dpy, False);
    XSetErrorHandler(previous);
    if (code == 0 || grab_failed()) {
      error = "The shortcut is already in use by another application.";
      XCloseDisplay(dpy);
      state = -1;
      return;
    }
    XSelectInput(dpy, root, KeyPressMask | KeyReleaseMask);
    state = 1;

    bool down = false;
    const int fd = ConnectionNumber(dpy);
    while (!stop) {
      while (XPending(dpy) > 0) {
        XEvent ev;
        XNextEvent(dpy, &ev);
        if (ev.type == KeyPress && ev.xkey.keycode == code && !down) {
          down = true;
          callback(HotkeyEvent::Pressed);
        } else if (ev.type == KeyRelease && ev.xkey.keycode == code && down) {
          down = false;
          callback(HotkeyEvent::Released);
        }
      }
      pollfd pfd{fd, POLLIN, 0};
      poll(&pfd, 1, 100);
    }

    for (unsigned int lock : locks) {
      XUngrabKey(dpy, code, mods | lock, root);
    }
    XCloseDisplay(dpy);
  }
};

GlobalHotkey::GlobalHotkey() : impl_(std::make_unique<Impl>()) {
  static std::once_flag once;
  std::call_once(once, [] { XInitThreads(); });
}
GlobalHotkey::~GlobalHotkey() {
  unregister();
}

bool GlobalHotkey::supported() {
  return true;
}

bool GlobalHotkey::register_key(const Hotkey &hotkey, Callback callback, std::string &error) {
  unregister_key();
  if (!hotkey.valid()) {
    error = "Invalid shortcut";
    return false;
  }
  impl_->callback = std::move(callback);
  impl_->keysym = Impl::to_keysym(hotkey.key);
  impl_->mods = (hotkey.ctrl ? ControlMask : 0) | (hotkey.alt ? Mod1Mask : 0) |
                (hotkey.shift ? ShiftMask : 0) | (hotkey.super ? Mod4Mask : 0);
  impl_->stop = false;
  impl_->state = 0;
  impl_->thread = std::thread([this] { impl_->run(); });
  while (impl_->state == 0) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  if (impl_->state < 0) {
    error = impl_->error;
    impl_->thread.join();
    return false;
  }
  return true;
}

void GlobalHotkey::unregister_key() {
  if (impl_ && impl_->thread.joinable()) {
    impl_->stop = true;
    impl_->thread.join();
  }
}

// ===========================================================================
// Unsupported platform
// ===========================================================================
#else

struct GlobalHotkey::Impl {};

GlobalHotkey::GlobalHotkey() : impl_(std::make_unique<Impl>()) {}
GlobalHotkey::~GlobalHotkey() {
  unregister();
}

bool GlobalHotkey::supported() {
  return false;
}

bool GlobalHotkey::register_key(const Hotkey &, Callback, std::string &error) {
  error = "Global shortcuts are not available on this platform. Bind a system shortcut to "
          "`lisper_gui --toggle` instead.";
  return false;
}

void GlobalHotkey::unregister_key() {}

#endif

} // namespace gui
