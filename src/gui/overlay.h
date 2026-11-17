#pragma once

#include "gui/dictation.h"
#include "gui/platform.h"
#include "settings.h"

#include <SDL2/SDL.h>

#include <array>
#include <string>

struct ImGuiContext;
struct ImFont;

namespace gui {

// Critically-damped-ish spring used for the island's size animation.
struct Spring {
  float value = 0.0f;
  float velocity = 0.0f;
  void step(float target, float dt, float stiffness = 380.0f, float damping = 28.0f);
  bool settled(float target) const;
};

// The dictation "island": a black capsule that grows out of the MacBook notch
// (or hangs from the top of the screen / floats at the bottom elsewhere),
// showing a live waveform, the words being spoken, then a check mark when the
// text has been inserted. It never takes keyboard focus. Click it to stop.
class Overlay {
public:
  Overlay() = default;
  ~Overlay();

  Overlay(const Overlay &) = delete;
  Overlay &operator=(const Overlay &) = delete;

  void update(const Dictation &dictation, const Settings &settings);

  // True if the event belongs to the island window (and was consumed).
  bool handle_event(const SDL_Event &event, bool &clicked);

  void destroy();

private:
  bool ensure_window();
  void apply_shape(int w, int h, float radius);
  void render(const Dictation &dictation, float dt);

  SDL_Window *window_ = nullptr;
  SDL_Renderer *renderer_ = nullptr;
  ImGuiContext *imgui_ = nullptr;
  ImFont *font_label_ = nullptr;
  ImFont *font_text_ = nullptr;
  ImFont *font_small_ = nullptr;
  float scale_ = 1.0f;
  bool transparent_ = false; // per-pixel alpha (macOS)
  bool shaped_ = false;      // SDL shaped window (Windows/X11)
  bool visible_ = false;
  bool hovered_ = false;

  platform::ScreenInfo screen_;
  bool attached_top_ = true; // flush with the top edge (notch style)
  int header_y_ = 0;         // content starts below the notch / menu bar

  Spring width_;
  Spring height_;
  float content_alpha_ = 0.0f;
  int shape_w_ = 0;
  int shape_h_ = 0;

  Dictation::Phase last_phase_ = Dictation::Phase::Idle;
  float phase_time_ = 0.0f;
  float time_ = 0.0f;
  std::array<float, 20> bars_{};
  std::string last_live_text_;
  float text_flash_ = 0.0f;
  Uint64 last_tick_ = 0;
};

} // namespace gui
