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
  if (key.empty()) {
    return false;
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
  if (alt) {
    add("Alt");
  }
  if (shift) {
    add("Shift");
  }
  if (super) {
#if defined(__APPLE__)
    add("Cmd");
#elif defined(_WIN32)
    add("Win");
#else
    add("Super");
#endif
  }
  add(key == "SPACE" ? "Space" : key.c_str());
  return out;
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
    if (p == "CTRL" || p == "CONTROL") {
      hk.ctrl = true;
    } else if (p == "ALT" || p == "OPTION" || p == "OPT") {
      hk.alt = true;
    } else if (p == "SHIFT") {
      hk.shift = true;
    } else if (p == "CMD" || p == "COMMAND" || p == "WIN" || p == "SUPER" || p == "META") {
      hk.super = true;
    } else {
      hk.key = p;
    }
  }
  return hk;
}

Hotkey hotkey_from_sdl(int keycode, int mods) {
  Hotkey hk;
  hk.ctrl = (mods & KMOD_CTRL) != 0;
  hk.alt = (mods & KMOD_ALT) != 0;
  hk.shift = (mods & KMOD_SHIFT) != 0;
  hk.super = (mods & KMOD_GUI) != 0;
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
  }
  return hk;
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
        callback(true);
        // Wait for the key to be released so push-to-talk can stop.
        while ((GetAsyncKeyState(static_cast<int>(vk)) & 0x8000) != 0) {
          std::this_thread::sleep_for(std::chrono::milliseconds(15));
        }
        callback(false);
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

bool GlobalHotkey::register_hotkey(const Hotkey &hotkey, Callback callback, std::string &error) {
  unregister();
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

void GlobalHotkey::unregister() {
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
      self->callback(pressed);
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

bool GlobalHotkey::register_hotkey(const Hotkey &hotkey, Callback callback, std::string &error) {
  unregister();
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
    unregister();
    return false;
  }
  return true;
}

void GlobalHotkey::unregister() {
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
          callback(true);
        } else if (ev.type == KeyRelease && ev.xkey.keycode == code && down) {
          down = false;
          callback(false);
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

bool GlobalHotkey::register_hotkey(const Hotkey &hotkey, Callback callback, std::string &error) {
  unregister();
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

void GlobalHotkey::unregister() {
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
GlobalHotkey::~GlobalHotkey() = default;

bool GlobalHotkey::supported() {
  return false;
}

bool GlobalHotkey::register_hotkey(const Hotkey &, Callback, std::string &error) {
  error = "Global shortcuts are not available on this platform. Bind a system shortcut to "
          "`lisper_gui --toggle` instead.";
  return false;
}

void GlobalHotkey::unregister() {}

#endif

} // namespace gui
