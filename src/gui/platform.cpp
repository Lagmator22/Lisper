// Windows and Linux implementations; macOS lives in platform_mac.mm.
#include "gui/platform.h"

#ifndef __APPLE__

#include <SDL2/SDL_syswm.h>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(LISPER_HAVE_X11)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif

namespace gui::platform {

namespace {
#if defined(_WIN32)
HWND g_previous_foreground = nullptr;
#endif
} // namespace

ScreenInfo screen_for_island() {
  ScreenInfo info;
  int mx = 0;
  int my = 0;
  SDL_GetGlobalMouseState(&mx, &my);
  int display = 0;
  for (int i = 0; i < SDL_GetNumVideoDisplays(); ++i) {
    SDL_Rect b;
    if (SDL_GetDisplayBounds(i, &b) == 0 && mx >= b.x && mx < b.x + b.w && my >= b.y &&
        my < b.y + b.h) {
      display = i;
      break;
    }
  }
  SDL_Rect bounds;
  SDL_Rect usable;
  if (SDL_GetDisplayBounds(display, &bounds) != 0) {
    bounds = {0, 0, 1280, 800};
  }
  if (SDL_GetDisplayUsableBounds(display, &usable) != 0) {
    usable = bounds;
  }
  info.x = bounds.x;
  info.y = bounds.y;
  info.w = bounds.w;
  info.h = bounds.h;
  info.top_inset = usable.y - bounds.y; // e.g. a top panel on GNOME

  // Developer aid: preview the notch-attached island on any machine,
  // e.g. LISPER_SIMULATE_NOTCH=200x32.
  if (const char *sim = SDL_getenv("LISPER_SIMULATE_NOTCH")) {
    int w = 0;
    int h = 0;
    if (SDL_sscanf(sim, "%dx%d", &w, &h) == 2 && w > 0 && h > 0) {
      info.has_notch = true;
      info.notch_w = w;
      info.notch_h = h;
      info.top_inset = h;
    }
  }
  return info;
}

bool configure_island_window(SDL_Window *window) {
  SDL_SysWMinfo wm;
  SDL_VERSION(&wm.version);
  if (!SDL_GetWindowWMInfo(window, &wm)) {
    return false;
  }
#if defined(_WIN32)
  HWND hwnd = wm.info.win.window;
  LONG_PTR ex = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
  // Clicking the island must never steal focus from the app being typed in.
  ex |= WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TOPMOST;
  SetWindowLongPtr(hwnd, GWL_EXSTYLE, ex);
  SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
#elif defined(LISPER_HAVE_X11)
  if (wm.subsystem == SDL_SYSWM_X11) {
    XWMHints *hints = XAllocWMHints();
    if (hints != nullptr) {
      hints->flags = InputHint;
      hints->input = False; // do not take keyboard focus when clicked
      XSetWMHints(wm.info.x11.display, wm.info.x11.window, hints);
      XFree(hints);
    }
  }
#endif
  return false; // per-pixel transparency is handled via shaped windows here
}

void place_window(SDL_Window *window, int x, int y, int w, int h) {
  int cw = 0;
  int ch = 0;
  SDL_GetWindowSize(window, &cw, &ch);
  if (cw != w || ch != h) {
    SDL_SetWindowSize(window, w, h);
  }
  int cx = 0;
  int cy = 0;
  SDL_GetWindowPosition(window, &cx, &cy);
  if (cx != x || cy != y) {
    SDL_SetWindowPosition(window, x, y);
  }
}

void remember_focus() {
#if defined(_WIN32)
  g_previous_foreground = GetForegroundWindow();
#endif
}

void restore_focus() {
#if defined(_WIN32)
  if (g_previous_foreground != nullptr && GetForegroundWindow() != g_previous_foreground &&
      IsWindow(g_previous_foreground)) {
    SetForegroundWindow(g_previous_foreground);
  }
#endif
}

} // namespace gui::platform

#endif // !__APPLE__
