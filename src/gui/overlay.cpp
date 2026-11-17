#include "gui/overlay.h"

#include "gui/embedded_fonts.h"

#include "backends/imgui_impl_sdlrenderer2.h"
#include "imgui.h"

#include <SDL2/SDL_shape.h>

#include <algorithm>
#include <cmath>

namespace gui {

namespace {

constexpr float kPi = 3.14159265f;

// Island geometry (points)
constexpr float kHeaderH = 40.0f;
constexpr float kTextLineH = 18.0f;
constexpr float kFloatMargin = 12.0f;

float ease_out_cubic(float t) {
  t = std::clamp(t, 0.0f, 1.0f);
  const float u = 1.0f - t;
  return 1.0f - u * u * u;
}

ImU32 rgba(int r, int g, int b, float a) {
  return IM_COL32(r, g, b, static_cast<int>(std::clamp(a, 0.0f, 1.0f) * 255.0f));
}

ImFont *add_font(ImGuiIO &io, const unsigned char *data, size_t size, float px) {
  ImFontConfig cfg;
  cfg.FontDataOwnedByAtlas = false; // static data compiled into the binary
  cfg.OversampleH = 2;
  cfg.OversampleV = 2;
  return io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char *>(data), static_cast<int>(size),
                                        px, &cfg);
}

// Fit `text` into at most `lines` wrapped lines of `width`, keeping the end
// (the newest words) and prefixing an ellipsis when trimmed.
std::string tail_fit(ImFont *font, float size, const std::string &text, float width, int lines) {
  const float max_h = size * 1.25f * static_cast<float>(lines) + 1.0f;
  auto height = [&](const std::string &t) {
    return font->CalcTextSizeA(size, FLT_MAX, width, t.c_str()).y;
  };
  if (height(text) <= max_h) {
    return text;
  }
  size_t start = 0;
  while (start < text.size()) {
    start = text.find(' ', start + 1);
    if (start == std::string::npos) {
      return text.substr(text.size() > 40 ? text.size() - 40 : 0);
    }
    const std::string candidate = "\xE2\x80\xA6" + text.substr(start); // …
    if (height(candidate) <= max_h) {
      return candidate;
    }
  }
  return text;
}

std::string format_clock(float seconds) {
  const int s = static_cast<int>(seconds);
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%d:%02d", s / 60, s % 60);
  return buf;
}

} // namespace

// ---------------------------------------------------------------------------

void Spring::step(float target, float dt, float stiffness, float damping) {
  // Semi-implicit Euler in small sub-steps for stability at low frame rates.
  const int steps = std::max(1, static_cast<int>(std::ceil(dt / 0.004f)));
  const float h = dt / static_cast<float>(steps);
  for (int i = 0; i < steps; ++i) {
    const float accel = -stiffness * (value - target) - damping * velocity;
    velocity += accel * h;
    value += velocity * h;
  }
}

bool Spring::settled(float target) const {
  return std::fabs(value - target) < 0.5f && std::fabs(velocity) < 2.0f;
}

// ---------------------------------------------------------------------------

Overlay::~Overlay() {
  destroy();
}

void Overlay::destroy() {
  if (imgui_ != nullptr) {
    ImGuiContext *previous = ImGui::GetCurrentContext();
    ImGui::SetCurrentContext(imgui_);
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui::DestroyContext(imgui_);
    ImGui::SetCurrentContext(previous == imgui_ ? nullptr : previous);
    imgui_ = nullptr;
  }
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
#ifdef __APPLE__
  window_ = SDL_CreateWindow("Lisper", 0, 0, 320, 80, flags);
#else
  // Shaped windows give real rounded corners without a compositor.
  window_ = SDL_CreateShapedWindow("Lisper", 0, 0, 320, 80, flags);
  shaped_ = window_ != nullptr;
  if (window_ == nullptr) {
    window_ = SDL_CreateWindow("Lisper", 0, 0, 320, 80, flags);
  }
#endif
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
  transparent_ = platform::configure_island_window(window_);

  int ww = 0;
  int wh = 0;
  int ow = 0;
  int oh = 0;
  SDL_GetWindowSize(window_, &ww, &wh);
  SDL_GetRendererOutputSize(renderer_, &ow, &oh);
  scale_ = ww > 0 ? std::max(1.0f, static_cast<float>(ow) / static_cast<float>(ww)) : 1.0f;
  SDL_RenderSetScale(renderer_, scale_, scale_);

  ImGuiContext *previous = ImGui::GetCurrentContext();
  imgui_ = ImGui::CreateContext();
  ImGui::SetCurrentContext(imgui_);
  ImGuiIO &io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.LogFilename = nullptr;
  font_label_ =
      add_font(io, lisper_font_inter_semibold, lisper_font_inter_semibold_size, 13.5f * scale_);
  font_text_ =
      add_font(io, lisper_font_inter_regular, lisper_font_inter_regular_size, 13.5f * scale_);
  font_small_ =
      add_font(io, lisper_font_inter_medium, lisper_font_inter_medium_size, 12.0f * scale_);
  io.FontGlobalScale = 1.0f / scale_;
  ImGui_ImplSDLRenderer2_Init(renderer_);
  ImGui::SetCurrentContext(previous);
  return true;
}

void Overlay::apply_shape(int w, int h, float radius) {
  if (!shaped_ || (w == shape_w_ && h == shape_h_)) {
    return;
  }
  SDL_Surface *mask = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
  if (mask == nullptr) {
    return;
  }
  SDL_FillRect(mask, nullptr, SDL_MapRGBA(mask->format, 0, 0, 0, 0));
  const Uint32 on = SDL_MapRGBA(mask->format, 255, 255, 255, 255);
  const float r = std::min(radius, std::min(w, h) * 0.5f);
  for (int y = 0; y < h; ++y) {
    auto *row = static_cast<Uint32 *>(mask->pixels) + y * (mask->pitch / 4);
    for (int x = 0; x < w; ++x) {
      const float px = static_cast<float>(x) + 0.5f;
      const float py = static_cast<float>(y) + 0.5f;
      const float cx = std::clamp(px, r, static_cast<float>(w) - r);
      // Flush top edge when attached to the top of the screen.
      const float cy = attached_top_ ? std::min(py, static_cast<float>(h) - r)
                                     : std::clamp(py, r, static_cast<float>(h) - r);
      const float dx = px - cx;
      const float dy = py - cy;
      if (dx * dx + dy * dy <= r * r) {
        row[x] = on;
      }
    }
  }
  SDL_WindowShapeMode mode;
  mode.mode = ShapeModeBinarizeAlpha;
  mode.parameters.binarizationCutoff = 128;
  if (SDL_SetWindowShape(window_, mask, &mode) == 0) {
    shape_w_ = w;
    shape_h_ = h;
  }
  SDL_FreeSurface(mask);
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
    if (event_window == id) {
      if (event.window.event == SDL_WINDOWEVENT_ENTER) {
        hovered_ = true;
      } else if (event.window.event == SDL_WINDOWEVENT_LEAVE) {
        hovered_ = false;
      }
    }
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

void Overlay::update(const Dictation &dictation, const Settings &settings) {
  const Uint64 now = SDL_GetTicks64();
  const float dt = last_tick_ == 0
                       ? 1.0f / 60.0f
                       : std::min(0.05f, static_cast<float>(now - last_tick_) / 1000.0f);
  last_tick_ = now;
  time_ += dt;

  const Dictation::Phase phase = dictation.phase();
  if (phase != last_phase_) {
    last_phase_ = phase;
    phase_time_ = 0.0f;
  } else {
    phase_time_ += dt;
  }

  const bool active = settings.show_overlay && phase != Dictation::Phase::Idle;
  if (!active && !visible_) {
    return;
  }
  if (!ensure_window()) {
    return;
  }

  if (!visible_) {
    // Fresh appearance: start collapsed (inside the notch) and grow out.
    screen_ = platform::screen_for_island();
#ifdef __APPLE__
    attached_top_ = settings.island_top;
#else
    attached_top_ = false;
#endif
    header_y_ = attached_top_ ? std::max(screen_.top_inset, screen_.notch_h) : 0;
    const float collapsed_w =
        screen_.has_notch && attached_top_ ? static_cast<float>(screen_.notch_w) : 120.0f;
    const float collapsed_h = attached_top_ ? static_cast<float>(header_y_) : 34.0f;
    width_ = Spring{collapsed_w, 0.0f};
    height_ = Spring{collapsed_h, 0.0f};
    content_alpha_ = 0.0f;
    last_live_text_.clear();
    std::fill(bars_.begin(), bars_.end(), 0.0f);
    SDL_ShowWindow(window_);
    visible_ = true;
  }

  // ---- target size for the current state ----------------------------------
  const float notch_w =
      screen_.has_notch && attached_top_ ? static_cast<float>(screen_.notch_w) : 0.0f;
  const std::string live = dictation.live_text();
  float target_w = 120.0f;
  float target_h = static_cast<float>(header_y_) + kHeaderH;
  int text_lines = 0;
  switch (phase) {
  case Dictation::Phase::Recording:
    target_w = std::max(notch_w + 160.0f, 380.0f);
    text_lines = live.empty() ? 0 : 2;
    break;
  case Dictation::Phase::Transcribing:
    target_w = std::max(notch_w + 120.0f, 300.0f);
    text_lines = live.empty() ? 0 : 2;
    break;
  case Dictation::Phase::Done:
    target_w = std::max(notch_w + 120.0f, 320.0f);
    text_lines = live.empty() ? 0 : 1;
    break;
  case Dictation::Phase::Error:
    target_w = std::max(notch_w + 160.0f, 400.0f);
    text_lines = dictation.message().empty() ? 0 : 2;
    break;
  case Dictation::Phase::Idle:
    target_w = notch_w > 0 ? notch_w : 120.0f;
    target_h = attached_top_ ? static_cast<float>(header_y_) : 34.0f;
    break;
  }
  if (text_lines > 0) {
    target_h += kTextLineH * static_cast<float>(text_lines) + 12.0f;
  }

  width_.step(target_w, dt);
  height_.step(target_h, dt, 420.0f, 30.0f);
  const float expand = active ? 1.0f : 0.0f;
  content_alpha_ += (expand - content_alpha_) * std::min(1.0f, dt * (active ? 9.0f : 18.0f));

  if (!active && width_.settled(target_w) && height_.settled(target_h) && content_alpha_ < 0.02f) {
    SDL_HideWindow(window_);
    visible_ = false;
    return;
  }

  // ---- place the window ------------------------------------------------------
  const int w = std::max(40, static_cast<int>(std::lround(width_.value)));
  const int h = std::max(20, static_cast<int>(std::lround(height_.value)));
  const int x = screen_.x + (screen_.w - w) / 2;
  int y = screen_.y;
  if (!attached_top_) {
    y = settings.island_top ? screen_.y + screen_.top_inset + static_cast<int>(kFloatMargin)
                            : screen_.y + screen_.h - h - 72;
  }
  const float radius = attached_top_ ? std::min(22.0f, h * 0.45f) : h * 0.5f;
  platform::place_island(window_, x, y, w, h, radius, !attached_top_);
  apply_shape(w, h, radius);

  render(dictation, dt);
}

void Overlay::render(const Dictation &dictation, float dt) {
  ImGuiContext *previous = ImGui::GetCurrentContext();
  ImGui::SetCurrentContext(imgui_);
  ImGuiIO &io = ImGui::GetIO();

  int ww = 0;
  int wh = 0;
  SDL_GetWindowSize(window_, &ww, &wh);
  io.DisplaySize = ImVec2(static_cast<float>(ww), static_cast<float>(wh));
  io.DeltaTime = std::max(dt, 1.0f / 240.0f);

  ImGui_ImplSDLRenderer2_NewFrame();
  ImGui::NewFrame();
  ImDrawList *dl = ImGui::GetForegroundDrawList();

  const float W = io.DisplaySize.x;
  const float H = io.DisplaySize.y;
  const float a = content_alpha_;
  const Dictation::Phase phase = last_phase_;

  // Shake horizontally on error.
  float shake = 0.0f;
  if (phase == Dictation::Phase::Error && phase_time_ < 0.45f) {
    shake = std::sin(phase_time_ * 48.0f) * 5.0f * (1.0f - phase_time_ / 0.45f);
  }

  // ---- capsule -------------------------------------------------------------
  const float radius = attached_top_ ? std::min(22.0f, H * 0.45f) : H * 0.5f;
  const ImDrawFlags corners =
      attached_top_ ? ImDrawFlags_RoundCornersBottom : ImDrawFlags_RoundCornersAll;
  dl->AddRectFilled(ImVec2(0, 0), ImVec2(W, H), IM_COL32(0, 0, 0, 255), radius, corners);
  if (hovered_ && phase == Dictation::Phase::Recording) {
    dl->AddRectFilled(ImVec2(0, 0), ImVec2(W, H), IM_COL32(255, 255, 255, 10), radius, corners);
  }
  if (!attached_top_) {
    dl->AddRect(ImVec2(0.5f, 0.5f), ImVec2(W - 0.5f, H - 0.5f), IM_COL32(255, 255, 255, 22), radius,
                corners, 1.0f);
  }

  const float top = static_cast<float>(header_y_);
  const float cy = top + kHeaderH * 0.5f;
  const float left = 18.0f + shake;
  const float right = W - 18.0f + shake;

  // Smooth the waveform: each bar eases toward the live level history.
  const auto levels = dictation.capture().recent_levels(bars_.size());
  for (size_t i = 0; i < bars_.size(); ++i) {
    const float target = phase == Dictation::Phase::Recording ? levels[i] : 0.0f;
    const float speed = target > bars_[i] ? 22.0f : 9.0f; // fast attack, slow release
    bars_[i] += (target - bars_[i]) * std::min(1.0f, dt * speed);
  }

  if (a > 0.01f) {
    // ---- leading icon --------------------------------------------------------
    const ImVec2 icon(left + 7.0f, cy);
    if (phase == Dictation::Phase::Recording) {
      const float level = dictation.capture().level();
      const float r = 4.5f + level * 3.0f;
      dl->AddCircleFilled(icon, r + 5.0f + 2.0f * std::sin(time_ * 3.0f),
                          rgba(255, 92, 66, 0.16f * a), 32);
      dl->AddCircleFilled(icon, r, rgba(255, 92, 66, a), 32);
    } else if (phase == Dictation::Phase::Transcribing) {
      const float start = time_ * 6.0f;
      dl->PathArcTo(icon, 7.0f, start, start + kPi * 1.3f, 24);
      dl->PathStroke(rgba(255, 255, 255, 0.9f * a), 0, 2.2f);
    } else if (phase == Dictation::Phase::Done) {
      // Check mark that draws itself.
      const float t = ease_out_cubic(phase_time_ / 0.35f);
      const ImVec2 p0(icon.x - 6.0f, icon.y + 0.5f);
      const ImVec2 p1(icon.x - 1.5f, icon.y + 5.0f);
      const ImVec2 p2(icon.x + 7.0f, icon.y - 5.5f);
      dl->AddCircleFilled(icon, 10.0f, rgba(52, 199, 89, 0.18f * a), 32);
      const float first = std::min(1.0f, t * 2.0f);
      dl->AddLine(p0, ImVec2(p0.x + (p1.x - p0.x) * first, p0.y + (p1.y - p0.y) * first),
                  rgba(80, 220, 120, a), 2.4f);
      if (t > 0.5f) {
        const float second = (t - 0.5f) * 2.0f;
        dl->AddLine(p1, ImVec2(p1.x + (p2.x - p1.x) * second, p1.y + (p2.y - p1.y) * second),
                    rgba(80, 220, 120, a), 2.4f);
      }
    } else if (phase == Dictation::Phase::Error) {
      dl->AddCircleFilled(icon, 8.0f, rgba(255, 95, 80, a), 32);
      dl->AddLine(ImVec2(icon.x, icon.y - 4.0f), ImVec2(icon.x, icon.y + 1.0f), rgba(0, 0, 0, a),
                  2.0f);
      dl->AddCircleFilled(ImVec2(icon.x, icon.y + 3.8f), 1.1f, rgba(0, 0, 0, a), 8);
    }

    // ---- label -----------------------------------------------------------------
    const char *label = phase == Dictation::Phase::Recording      ? "Listening"
                        : phase == Dictation::Phase::Transcribing ? "Writing"
                        : phase == Dictation::Phase::Done
                            ? (dictation.pasted() ? "Inserted" : "Copied")
                        : phase == Dictation::Phase::Error ? "Couldn't dictate"
                                                           : "";
    ImGui::PushFont(font_label_);
    const float label_h = ImGui::GetFontSize();
    dl->AddText(ImVec2(left + 22.0f, cy - label_h * 0.5f), rgba(255, 255, 255, a), label);
    ImGui::PopFont();

    // ---- trailing: waveform + timer while recording, shimmer while writing ----
    if (phase == Dictation::Phase::Recording || phase == Dictation::Phase::Transcribing) {
      const int n = static_cast<int>(bars_.size());
      const float bar_w = 3.0f;
      const float gap = 2.5f;
      const float wave_w = n * bar_w + (n - 1) * gap;
      float wx = right - wave_w;
      if (phase == Dictation::Phase::Recording) {
        ImGui::PushFont(font_small_);
        const std::string clock =
            format_clock(static_cast<float>(dictation.capture().recorded_ms()) / 1000.0f);
        const ImVec2 ts = ImGui::CalcTextSize(clock.c_str());
        dl->AddText(ImVec2(right - ts.x, cy - ts.y * 0.5f), rgba(170, 168, 160, a), clock.c_str());
        ImGui::PopFont();
        wx -= ts.x + 12.0f;
      }
      for (int i = 0; i < n; ++i) {
        float v = bars_[static_cast<size_t>(i)];
        if (phase == Dictation::Phase::Transcribing) {
          v = 0.12f +
              0.18f * (0.5f + 0.5f * std::sin(time_ * 7.0f - static_cast<float>(i) * 0.55f));
        }
        const float bh = std::max(3.0f, v * 22.0f);
        const float x0 = wx + static_cast<float>(i) * (bar_w + gap);
        // warm gradient: clay on the left to soft cream on the right
        const float k = static_cast<float>(i) / static_cast<float>(n - 1);
        const int r = static_cast<int>(232 + (245 - 232) * k);
        const int g = static_cast<int>(122 + (226 - 122) * k);
        const int b = static_cast<int>(92 + (205 - 92) * k);
        dl->AddRectFilled(ImVec2(x0, cy - bh * 0.5f), ImVec2(x0 + bar_w, cy + bh * 0.5f),
                          rgba(r, g, b, (0.55f + 0.45f * std::min(1.0f, v * 2.0f)) * a), 2.0f);
      }
    }

    // ---- live words / message ---------------------------------------------------
    std::string body =
        phase == Dictation::Phase::Error ? dictation.message() : dictation.live_text();
    if (phase == Dictation::Phase::Done && !dictation.pasted()) {
#ifdef __APPLE__
      body = "Press \xE2\x8C\x98V to paste it.";
#else
      body = "Press Ctrl+V to paste it.";
#endif
    }
    if (body != last_live_text_) {
      last_live_text_ = body;
      text_flash_ = 1.0f;
    }
    text_flash_ = std::max(0.0f, text_flash_ - dt * 3.0f);
    if (!body.empty() && H > top + kHeaderH + 8.0f) {
      ImGui::PushFont(font_text_);
      const float size = ImGui::GetFontSize();
      const float width = W - 36.0f;
      const int lines = phase == Dictation::Phase::Done ? 1 : 2;
      body = tail_fit(ImGui::GetFont(), size, body, width, lines);
      const float text_alpha = a * std::clamp((H - top - kHeaderH) / 30.0f, 0.0f, 1.0f);
      const ImU32 color = phase == Dictation::Phase::Error
                              ? rgba(255, 160, 150, text_alpha)
                              : rgba(240, 238, 230, text_alpha * (0.82f + 0.18f * text_flash_));
      dl->AddText(ImGui::GetFont(), size, ImVec2(18.0f + shake, top + kHeaderH - 2.0f), color,
                  body.c_str(), nullptr, width);
      ImGui::PopFont();
    }
  }

  ImGui::Render();
  SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);
  SDL_SetRenderDrawColor(renderer_, 0, 0, 0, transparent_ ? 0 : 255);
  SDL_RenderClear(renderer_);
  ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer_);
  SDL_RenderPresent(renderer_);

  ImGui::SetCurrentContext(previous);
}

} // namespace gui
