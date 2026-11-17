#pragma once

#include <SDL2/SDL.h>

namespace gui::platform {

// Geometry of the display the island should appear on, in SDL (top-left,
// point) coordinates.
struct ScreenInfo {
  int x = 0, y = 0, w = 0, h = 0; // full display bounds
  int top_inset = 0;              // menu bar / safe-area height at the top
  bool has_notch = false;         // MacBook camera housing
  int notch_w = 0;                // width of the notch
  int notch_h = 0;                // height of the notch (== menu bar height)
};

// Describe the display under the mouse (or the main display).
ScreenInfo screen_for_island();

// One-time window setup for the floating island: non-activating, above the
// menu bar on macOS, visible on all Spaces and over full-screen apps,
// transparent outside the rounded shape where the platform allows it.
// Returns true if the window can be drawn with per-pixel transparency.
// `renderer` (may be null) lets macOS reach the CAMetalLayer it draws into.
bool configure_island_window(SDL_Window *window, SDL_Renderer *renderer);

// Move/resize the island's canvas window. On macOS this bypasses the
// menu-bar constraint so the island can grow out of the notch.
void place_window(SDL_Window *window, int x, int y, int w, int h);

// Remember which app had focus when dictation started, and give focus back to
// it before pasting (clicking the island may have activated Lisper).
void remember_focus();
void restore_focus();

} // namespace gui::platform
