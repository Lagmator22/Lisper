#pragma once

#include "gui/dictation.h"

#include <SDL2/SDL.h>

namespace gui {

// The floating "Flow bar": a small pill near the bottom of the screen that
// shows a live waveform while listening, a shimmer while transcribing and a
// check/cross when done. It never takes keyboard focus, so text lands in the
// app the user was typing in. Clicking it stops (or starts) dictation.
class Overlay {
public:
  Overlay() = default;
  ~Overlay();

  Overlay(const Overlay &) = delete;
  Overlay &operator=(const Overlay &) = delete;

  // Draw for the current dictation state; shows/hides the window as needed.
  void update(const Dictation &dictation, bool enabled, bool dark_theme);

  // True if the event belongs to the overlay window (and was consumed).
  bool handle_event(const SDL_Event &event, bool &clicked);

  void destroy();

private:
  bool ensure_window();
  void position_window();
  void draw(const Dictation &dictation, float t);

  SDL_Window *window_ = nullptr;
  SDL_Renderer *renderer_ = nullptr;
  bool visible_ = false;
  bool shaped_ = false;
  float appear_ = 0.0f; // 0..1 entrance animation
  Uint64 last_tick_ = 0;
};

} // namespace gui
