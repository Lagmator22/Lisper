#include "gui/overlay.h"

#include <SDL2/SDL_shape.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace gui {

namespace {

constexpr int kWidth = 196;
constexpr int kHeight = 48;
constexpr int kBottomMargin = 64;
constexpr float kPi = 3.14159265f;

struct Rgba {
  Uint8 r, g, b, a;
};

// Filled rounded rectangle via SDL_RenderGeometry (SDL >= 2.0.18).
void fill_round_rect(SDL_Renderer *r, float x, float y, float w, float h, float radius, Rgba c) {
  radius = std::min(radius, std::min(w, h) * 0.5f);
  const int seg = 10;
  std::vector<SDL_Vertex> verts;
  std::vector<int> idx;
  const SDL_Color col{c.r, c.g, c.b, c.a};
  const float cx = x + w * 0.5f;
  const float cy = y + h * 0.5f;
  verts.push_back({{cx, cy}, col, {0, 0}});
  const float corners[4][3] = {{x + w - radius, y + radius, -kPi * 0.5f},
                               {x + w - radius, y + h - radius, 0.0f},
                               {x + radius, y + h - radius, kPi * 0.5f},
                               {x + radius, y + radius, kPi}};
  for (const auto &corner : corners) {
    for (int i = 0; i <= seg; ++i) {
      const float a = corner[2] + (kPi * 0.5f) * static_cast<float>(i) / seg;
      verts.push_back(
          {{corner[0] + std::cos(a) * radius, corner[1] + std::sin(a) * radius}, col, {0, 0}});
    }
  }
  const int n = static_cast<int>(verts.size()) - 1;
  for (int i = 1; i <= n; ++i) {
    idx.push_back(0);
    idx.push_back(i);
    idx.push_back(i == n ? 1 : i + 1);
  }
  SDL_RenderGeometry(r, nullptr, verts.data(), static_cast<int>(verts.size()), idx.data(),
                     static_cast<int>(idx.size()));
}

void fill_circle(SDL_Renderer *r, float cx, float cy, float radius, Rgba c) {
  fill_round_rect(r, cx - radius, cy - radius, radius * 2, radius * 2, radius, c);
}

void thick_line(SDL_Renderer *r, float x0, float y0, float x1, float y1, float width, Rgba c) {
  const float dx = x1 - x0;
  const float dy = y1 - y0;
  const float len = std::max(0.001f, std::sqrt(dx * dx + dy * dy));
  const float nx = -dy / len * width * 0.5f;
  const float ny = dx / len * width * 0.5f;
  const SDL_Color col{c.r, c.g, c.b, c.a};
  SDL_Vertex v[4] = {{{x0 + nx, y0 + ny}, col, {0, 0}},
                     {{x1 + nx, y1 + ny}, col, {0, 0}},
                     {{x1 - nx, y1 - ny}, col, {0, 0}},
                     {{x0 - nx, y0 - ny}, col, {0, 0}}};
  const int idx[6] = {0, 1, 2, 0, 2, 3};
  SDL_RenderGeometry(r, nullptr, v, 4, idx, 6);
  fill_circle(r, x0, y0, width * 0.5f, c);
  fill_circle(r, x1, y1, width * 0.5f, c);
}

} // namespace

Overlay::~Overlay() {
  destroy();
}

void Overlay::destroy() {
  if (renderer_ != nullptr) {
    SDL_DestroyRenderer(renderer_);
    renderer_ = nullptr;
  }
  if (window_ != nullptr) {
    SDL_DestroyWindow(window_);
    window_ = nullptr;
  }
  visible_ = false;
}

bool Overlay::ensure_window() {
  if (window_ != nullptr) {
    return true;
  }
  SDL_SetHint(SDL_HINT_WINDOW_NO_ACTIVATION_WHEN_SHOWN, "1");
  const Uint32 flags = SDL_WINDOW_HIDDEN | SDL_WINDOW_BORDERLESS | SDL_WINDOW_ALWAYS_ON_TOP |
                       SDL_WINDOW_SKIP_TASKBAR | SDL_WINDOW_UTILITY | SDL_WINDOW_ALLOW_HIGHDPI;

  // A shaped window gives true rounded corners; fall back to a plain one.
  window_ = SDL_CreateShapedWindow("Lisper", 0, 0, kWidth, kHeight, flags);
  shaped_ = window_ != nullptr;
  if (window_ == nullptr) {
    window_ = SDL_CreateWindow("Lisper", 0, 0, kWidth, kHeight, flags);
  }
  if (window_ == nullptr) {
    return false;
  }
  renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (renderer_ == nullptr) {
    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);
  }
  if (renderer_ == nullptr) {
    destroy();
    return false;
  }
  SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);

  if (shaped_) {
    SDL_Surface *mask =
        SDL_CreateRGBSurfaceWithFormat(0, kWidth, kHeight, 32, SDL_PIXELFORMAT_RGBA32);
    if (mask != nullptr) {
      SDL_FillRect(mask, nullptr, SDL_MapRGBA(mask->format, 0, 0, 0, 0));
      const float radius = kHeight * 0.5f;
      for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
          const float cx = std::clamp(static_cast<float>(x) + 0.5f, radius, kWidth - radius);
          const float dx = static_cast<float>(x) + 0.5f - cx;
          const float dy = static_cast<float>(y) + 0.5f - radius;
          if (dx * dx + dy * dy <= radius * radius) {
            auto *px = static_cast<Uint32 *>(mask->pixels) + y * (mask->pitch / 4) + x;
            *px = SDL_MapRGBA(mask->format, 255, 255, 255, 255);
          }
        }
      }
      SDL_WindowShapeMode mode;
      mode.mode = ShapeModeBinarizeAlpha;
      mode.parameters.binarizationCutoff = 128;
      SDL_SetWindowShape(window_, mask, &mode);
      SDL_FreeSurface(mask);
    }
  }
  position_window();
  return true;
}

void Overlay::position_window() {
  if (window_ == nullptr) {
    return;
  }
  // Follow the display under the mouse so the bar appears where the user works.
  int mx = 0;
  int my = 0;
  SDL_GetGlobalMouseState(&mx, &my);
  int display = 0;
  const int n = SDL_GetNumVideoDisplays();
  for (int i = 0; i < n; ++i) {
    SDL_Rect b;
    if (SDL_GetDisplayBounds(i, &b) == 0 && mx >= b.x && mx < b.x + b.w && my >= b.y &&
        my < b.y + b.h) {
      display = i;
      break;
    }
  }
  SDL_Rect usable;
  if (SDL_GetDisplayUsableBounds(display, &usable) != 0) {
    return;
  }
  SDL_SetWindowPosition(window_, usable.x + (usable.w - kWidth) / 2,
                        usable.y + usable.h - kHeight - kBottomMargin);
}

bool Overlay::handle_event(const SDL_Event &event, bool &clicked) {
  clicked = false;
  if (window_ == nullptr) {
    return false;
  }
  const Uint32 id = SDL_GetWindowID(window_);
  Uint32 event_window = 0;
  switch (event.type) {
  case SDL_MOUSEBUTTONDOWN:
  case SDL_MOUSEBUTTONUP:
    event_window = event.button.windowID;
    break;
  case SDL_MOUSEMOTION:
    event_window = event.motion.windowID;
    break;
  case SDL_WINDOWEVENT:
    event_window = event.window.windowID;
    break;
  default:
    return false;
  }
  if (event_window != id) {
    return false;
  }
  if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
    clicked = true;
  }
  return true;
}

void Overlay::update(const Dictation &dictation, bool enabled, bool dark_theme) {
  (void)dark_theme; // the bar is always dark, like Flow's, for contrast on any app
  const Dictation::Phase phase = dictation.phase();
  const bool want = enabled && phase != Dictation::Phase::Idle;

  const Uint64 now = SDL_GetTicks64();
  const float dt = last_tick_ == 0 ? 0.016f : static_cast<float>(now - last_tick_) / 1000.0f;
  last_tick_ = now;

  if (!want) {
    if (visible_ && window_ != nullptr) {
      SDL_HideWindow(window_);
    }
    visible_ = false;
    appear_ = 0.0f;
    return;
  }
  if (!ensure_window()) {
    return;
  }
  if (!visible_) {
    position_window();
    SDL_ShowWindow(window_);
    visible_ = true;
  }
  appear_ = std::min(1.0f, appear_ + dt * 6.0f);
  draw(dictation, static_cast<float>(now) / 1000.0f);
}

void Overlay::draw(const Dictation &dictation, float t) {
  int ow = kWidth;
  int oh = kHeight;
  SDL_GetRendererOutputSize(renderer_, &ow, &oh);
  const float s = static_cast<float>(ow) / kWidth; // HiDPI scale
  const float w = kWidth * s;
  const float h = kHeight * s;

  SDL_SetRenderDrawColor(renderer_, 0, 0, 0, shaped_ ? 0 : 255);
  SDL_RenderClear(renderer_);

  const Uint8 alpha = static_cast<Uint8>(255 * (0.4f + 0.6f * appear_));
  fill_round_rect(renderer_, 0, 0, w, h, h * 0.5f, {38, 38, 42, 255});
  fill_round_rect(renderer_, 1.5f * s, 1.5f * s, w - 3 * s, h - 3 * s, h * 0.5f - 1.5f * s,
                  {14, 14, 16, alpha});

  const Dictation::Phase phase = dictation.phase();
  const float cy = h * 0.5f;

  if (phase == Dictation::Phase::Recording) {
    // Pulsing red dot on the left
    const float pulse = 0.75f + 0.25f * std::sin(t * 5.0f);
    fill_circle(renderer_, 24 * s, cy, 5.5f * s * pulse, {255, 69, 58, 255});

    // Live waveform from microphone levels
    const int bars = 22;
    const auto levels = dictation.capture().recent_levels(static_cast<size_t>(bars));
    const float left = 44 * s;
    const float right = w - 22 * s;
    const float step = (right - left) / bars;
    for (int i = 0; i < bars; ++i) {
      const float lvl = levels[static_cast<size_t>(i)];
      const float bh = std::max(3.0f * s, lvl * (h - 20 * s));
      const float x = left + step * i + step * 0.25f;
      fill_round_rect(renderer_, x, cy - bh * 0.5f, step * 0.5f, bh, step * 0.25f,
                      {255, 255, 255, static_cast<Uint8>(150 + 105 * std::min(1.0f, lvl * 1.5f))});
    }
  } else if (phase == Dictation::Phase::Transcribing) {
    // Shimmering dots while the model works
    const int dots = 5;
    const float spacing = 16 * s;
    const float start = w * 0.5f - spacing * (dots - 1) * 0.5f;
    for (int i = 0; i < dots; ++i) {
      const float phase_off = t * 6.0f - static_cast<float>(i) * 0.7f;
      const float k = 0.5f + 0.5f * std::sin(phase_off);
      fill_circle(renderer_, start + spacing * i, cy - k * 4 * s, (3.0f + 1.5f * k) * s,
                  {static_cast<Uint8>(140 + 115 * k), static_cast<Uint8>(140 + 100 * k), 255, 255});
    }
  } else if (phase == Dictation::Phase::Done) {
    const float cx = w * 0.5f;
    thick_line(renderer_, cx - 10 * s, cy, cx - 3 * s, cy + 7 * s, 3.5f * s, {48, 209, 88, 255});
    thick_line(renderer_, cx - 3 * s, cy + 7 * s, cx + 11 * s, cy - 8 * s, 3.5f * s,
               {48, 209, 88, 255});
  } else if (phase == Dictation::Phase::Error) {
    const float cx = w * 0.5f;
    thick_line(renderer_, cx - 8 * s, cy - 8 * s, cx + 8 * s, cy + 8 * s, 3.5f * s,
               {255, 69, 58, 255});
    thick_line(renderer_, cx - 8 * s, cy + 8 * s, cx + 8 * s, cy - 8 * s, 3.5f * s,
               {255, 69, 58, 255});
  }

  SDL_RenderPresent(renderer_);
}

} // namespace gui
