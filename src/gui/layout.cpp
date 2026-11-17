#include "gui/layout.h"

#include "audio_capture.h"
#include "gui/file_dialog.h"
#include "gui/hotkey.h"
#include "paths.h"

#include "imgui_internal.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace gui {

namespace {

constexpr float kSidebarWidth = 236.0f;
constexpr float kContentMaxWidth = 820.0f;
constexpr float kPi = 3.14159265f;

ImU32 u32(const ImVec4 &c) {
  return ImGui::GetColorU32(c);
}

void mark_dirty(AppState &state) {
  state.settings_dirty = true;
}

double steady_seconds() {
  return static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::steady_clock::now().time_since_epoch())
                                 .count()) /
         1000.0;
}

std::tm local_tm(std::time_t t) {
  std::tm out{};
#ifdef _WIN32
  localtime_s(&out, &t);
#else
  localtime_r(&t, &out);
#endif
  return out;
}

std::string day_label(int64_t ts_ms) {
  const std::tm a = local_tm(static_cast<std::time_t>(ts_ms / 1000));
  const std::time_t now = std::time(nullptr);
  const std::tm b = local_tm(now);
  if (a.tm_year == b.tm_year && a.tm_yday == b.tm_yday) {
    return "Today";
  }
  const std::tm y = local_tm(now - 86400);
  if (a.tm_year == y.tm_year && a.tm_yday == y.tm_yday) {
    return "Yesterday";
  }
  char buf[64];
  std::strftime(buf, sizeof(buf), "%A, %B %d", &a);
  return buf;
}

std::string time_label(int64_t ts_ms) {
  const std::tm a = local_tm(static_cast<std::time_t>(ts_ms / 1000));
  char buf[16];
  std::strftime(buf, sizeof(buf), "%H:%M", &a);
  return buf;
}

const char *greeting() {
  const int hour = local_tm(std::time(nullptr)).tm_hour;
  if (hour < 5) {
    return "Working late";
  }
  if (hour < 12) {
    return "Good morning";
  }
  if (hour < 18) {
    return "Good afternoon";
  }
  return "Good evening";
}

std::string shortcut_display(const Settings &s) {
  return parse_hotkey(s.hotkey).display();
}

std::string shortcut_verb(const Settings &s) {
  if (s.hotkey_hold) {
    return "Hold";
  }
  return parse_hotkey(s.hotkey).is_chord() ? "Tap" : "Press";
}

// ---------------------------------------------------------------------------
// Page frame with an entrance transition (fade + short rise).
// ---------------------------------------------------------------------------

float g_page_alpha = 1.0f;

void begin_page(UiContext &ctx, const char *id) {
  const float t = static_cast<float>(ImGui::GetTime() - ctx.state.page_changed_at) / 0.32f;
  const float e = ease_out_cubic(t);
  g_page_alpha = 0.0f + e;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::BeginChild(id, ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_None);
  ImGui::PopStyleVar();
  ImGui::PushStyleVar(ImGuiStyleVar_Alpha, std::max(0.0f, g_page_alpha));
  const float avail = ImGui::GetContentRegionAvail().x;
  const float width = std::min(kContentMaxWidth, avail - 96.0f);
  const float pad = std::max(48.0f, (avail - width) * 0.5f);
  ImGui::SetCursorPos(ImVec2(pad, 52.0f + (1.0f - e) * 14.0f));
  ImGui::BeginGroup();
  ImGui::PushItemWidth(width);
  ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width);
  ImGui::Dummy(ImVec2(width, 0.0f));
}

void end_page() {
  ImGui::Dummy(ImVec2(0.0f, 56.0f));
  ImGui::PopTextWrapPos();
  ImGui::PopItemWidth();
  ImGui::EndGroup();
  ImGui::PopStyleVar();
  ImGui::EndChild();
}

float page_width() {
  return ImGui::CalcItemWidth();
}

void page_header(const Fonts &fonts, const char *title, const char *subtitle) {
  hero(fonts, title);
  if (subtitle != nullptr) {
    ImGui::Dummy(ImVec2(0.0f, 2.0f));
    ImGui::PushFont(fonts.lead);
    muted(subtitle);
    ImGui::PopFont();
  }
  ImGui::Dummy(ImVec2(0.0f, 20.0f));
}

void gap(float h = 14.0f) {
  ImGui::Dummy(ImVec2(0.0f, h));
}

// ---------------------------------------------------------------------------
// Sidebar
// ---------------------------------------------------------------------------

void set_page(AppState &state, Page page) {
  if (state.page != page) {
    state.page = page;
    state.page_changed_at = ImGui::GetTime();
    state.capturing_hotkey = false;
  }
}

void render_sidebar(UiContext &ctx) {
  AppState &state = ctx.state;
  const Palette &p = palette();

  ImGui::PushStyleColor(ImGuiCol_ChildBg, p.sidebar);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 24.0f));
  ImGui::BeginChild("sidebar", ImVec2(kSidebarWidth, 0.0f), ImGuiChildFlags_AlwaysUseWindowPadding,
                    ImGuiWindowFlags_NoScrollbar);
  ImDrawList *dl = ImGui::GetWindowDrawList();

  // Hairline between sidebar and content.
  const ImVec2 wpos = ImGui::GetWindowPos();
  dl->AddLine(ImVec2(wpos.x + kSidebarWidth - 1.0f, wpos.y),
              ImVec2(wpos.x + kSidebarWidth - 1.0f, wpos.y + ImGui::GetWindowHeight()),
              u32(p.border));

  // Brand
  {
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    draw_brand_mark(dl, ImVec2(pos.x + 4.0f, pos.y), 30.0f, static_cast<float>(ImGui::GetTime()));
    ImGui::SetCursorScreenPos(ImVec2(pos.x + 44.0f, pos.y + 1.0f));
    ImGui::PushFont(ctx.fonts.heading);
    ImGui::TextUnformatted("Lisper");
    ImGui::PopFont();
    ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + 52.0f));
  }

  struct NavEntry {
    const char *label;
    Icon icon;
    Page page;
  };
  const NavEntry entries[] = {
      {"Home", Icon::Home, Page::Home},
      {"Transcribe files", Icon::File, Page::Transcribe},
      {"Dictionary", Icon::Book, Page::Dictionary},
      {"Snippets", Icon::Bolt, Page::Snippets},
      {"Style", Icon::Brush, Page::Style},
      {"Settings", Icon::Gear, Page::Settings},
  };

  // Sliding selection indicator (spring) drawn under the items.
  const ImVec2 list_pos = ImGui::GetCursorScreenPos();
  const float item_h = 36.0f;
  const float spacing = 4.0f;
  int selected = 0;
  for (int i = 0; i < 6; ++i) {
    if (entries[i].page == state.page) {
      selected = i;
    }
  }
  const float y = spring(ImGui::GetID("nav_indicator"),
                         static_cast<float>(selected) * (item_h + spacing), 300.0f, 26.0f);
  const float w = ImGui::GetContentRegionAvail().x;
  const ImVec2 ia(list_pos.x, list_pos.y + y);
  const ImVec2 ib(list_pos.x + w, list_pos.y + y + item_h);
  if (!p.dark) {
    dl->AddRectFilled(ImVec2(ia.x, ia.y + 1.0f), ImVec2(ib.x, ib.y + 1.5f),
                      IM_COL32(60, 45, 30, 12), 10.0f);
  }
  dl->AddRectFilled(ia, ib, u32(p.dark ? p.card_hover : p.card), 10.0f);
  if (!p.dark) {
    dl->AddRect(ia, ib, u32(p.border), 10.0f);
  }

  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, spacing));
  for (const auto &e : entries) {
    if (nav_item(ctx.fonts, e.label, e.icon, state.page == e.page)) {
      set_page(state, e.page);
    }
  }
  ImGui::PopStyleVar();

  // Footer: live status and the shortcut.
  const float footer_h = 112.0f;
  ImGui::SetCursorPosY(
      std::max(ImGui::GetCursorPosY() + 16.0f, ImGui::GetWindowHeight() - footer_h));
  divider();

  const Dictation::Phase phase = ctx.dictation.phase();
  ImVec4 dot = p.text_faint;
  std::string status = ctx.dictation.model_status();
  bool pulse = false;
  if (ctx.dictation.model_loading()) {
    dot = p.warning;
    pulse = true;
  } else if (ctx.dictation.model_ready()) {
    dot = phase == Dictation::Phase::Recording ? p.accent : p.success;
    pulse = phase == Dictation::Phase::Recording;
    status = phase == Dictation::Phase::Idle ? "Ready" : phase_label(phase);
  }
  status_dot(dot, 3.5f, pulse);
  ImGui::SameLine(0.0f, 6.0f);
  ImGui::PushFont(ctx.fonts.small);
  ImGui::PushStyleColor(ImGuiCol_Text, p.text_muted);
  ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + kSidebarWidth - 60.0f);
  ImGui::TextUnformatted(status.c_str());
  ImGui::PopTextWrapPos();
  ImGui::PopStyleColor();
  ImGui::PopFont();
  ImGui::Dummy(ImVec2(0.0f, 2.0f));

  ImGui::PushFont(ctx.fonts.small);
  keycaps(shortcut_display(state.settings));
  ImGui::PushStyleColor(ImGuiCol_Text, p.text_faint);
  ImGui::TextUnformatted(state.settings.hotkey_hold ? "hold to dictate" : "to start and stop");
  ImGui::PopStyleColor();
  ImGui::PopFont();

  ImGui::EndChild();
  ImGui::PopStyleVar();
  ImGui::PopStyleColor();
}

// ---------------------------------------------------------------------------
// Home: the dictation stage
// ---------------------------------------------------------------------------

ImVec4 hex_accent_dark() {
  return ImVec4(0.66f, 0.30f, 0.18f, 1.0f); // deep clay for the orb rim
}

// Outline button for use on the dark stage.
bool draw_ghost_on_stage(const char *label) {
  const Palette &p = palette();
  const ImVec2 ts = ImGui::CalcTextSize(label);
  const ImVec2 size(ts.x + 30.0f, 38.0f);
  const ImVec2 pos = ImGui::GetCursorScreenPos();
  const bool pressed = ImGui::InvisibleButton(label, size);
  const float hv = anim(ImGui::GetItemID(), ImGui::IsItemHovered() ? 1.0f : 0.0f, 14.0f);
  ImDrawList *dl = ImGui::GetWindowDrawList();
  dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                    u32(with_alpha(p.stage_text, 0.06f * hv)), 10.0f);
  dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y),
              u32(with_alpha(p.stage_text, 0.25f + 0.2f * hv)), 10.0f);
  dl->AddText(ImVec2(pos.x + 15.0f, pos.y + (size.y - ts.y) * 0.5f), u32(p.stage_text), label);
  return pressed;
}

// The clay orb: breathes when idle, ripples with the live microphone level
// while listening, spins while writing and resolves into a check when done.
bool draw_orb(UiContext &ctx, const ImVec2 &center, float radius) {
  const Palette &p = palette();
  ImDrawList *dl = ImGui::GetWindowDrawList();
  const Dictation::Phase phase = ctx.dictation.phase();
  const float t = static_cast<float>(ImGui::GetTime());
  const float level = anim(ImGui::GetID("orb_level"), ctx.dictation.capture().level(), 18.0f);

  ImGui::SetCursorScreenPos(ImVec2(center.x - radius - 30.0f, center.y - radius - 30.0f));
  const bool clicked =
      ImGui::InvisibleButton("orb", ImVec2((radius + 30.0f) * 2.0f, (radius + 30.0f) * 2.0f));
  const float hover = anim(ImGui::GetID("orb_hover"), ImGui::IsItemHovered() ? 1.0f : 0.0f, 12.0f);
  const float press = anim(ImGui::GetID("orb_press"), ImGui::IsItemActive() ? 1.0f : 0.0f, 22.0f);
  if (ImGui::IsItemHovered()) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
  }

  const bool recording = phase == Dictation::Phase::Recording;
  const float rec = anim(ImGui::GetID("orb_rec"), recording ? 1.0f : 0.0f, 8.0f);

  // Ripples emitted while recording.
  if (rec > 0.01f) {
    for (int i = 0; i < 3; ++i) {
      const float k = std::fmod(t * 0.7f + static_cast<float>(i) / 3.0f, 1.0f);
      const float r = radius + 6.0f + k * (26.0f + level * 20.0f);
      dl->AddCircle(center, r, u32(with_alpha(p.accent, (1.0f - k) * 0.45f * rec)), 64, 1.5f);
    }
    dl->AddCircleFilled(center, radius + 4.0f + level * 16.0f,
                        u32(with_alpha(p.accent, 0.16f * rec)), 64);
  } else {
    // Idle breathing glow.
    const float breath = 0.5f + 0.5f * std::sin(t * 1.6f);
    dl->AddCircleFilled(center, radius + 6.0f + breath * 4.0f + hover * 4.0f,
                        u32(with_alpha(p.accent, 0.10f + 0.05f * breath + 0.06f * hover)), 64);
  }

  // Body: a soft radial gradient built from concentric discs.
  const float scale = 1.0f + 0.04f * hover - 0.05f * press + 0.05f * level * rec;
  const float r = radius * scale;
  const ImVec4 outer = mix(hex_accent_dark(), p.accent, 0.4f);
  for (int i = 0; i < 12; ++i) {
    const float k = static_cast<float>(i) / 11.0f;
    const ImVec4 col = mix(outer, mix(p.accent, ImVec4(1.0f, 0.86f, 0.76f, 1.0f), 0.35f), k);
    dl->AddCircleFilled(ImVec2(center.x - k * r * 0.12f, center.y - k * r * 0.16f),
                        r * (1.0f - k * 0.55f), u32(col), 64);
  }

  const ImU32 ink = IM_COL32(255, 250, 244, 255);
  if (phase == Dictation::Phase::Transcribing) {
    const float a0 = t * 5.0f;
    dl->PathArcTo(center, r + 9.0f, a0, a0 + kPi * 1.25f, 48);
    dl->PathStroke(u32(p.accent), 0, 2.5f);
    draw_icon(dl, Icon::Mic, center, r * 0.75f, ink);
  } else if (phase == Dictation::Phase::Done) {
    draw_icon(dl, Icon::Check, center, r * 0.85f, ink);
  } else if (recording) {
    draw_icon(dl, Icon::Stop, center, r * 0.7f, ink);
  } else {
    draw_icon(dl, Icon::Mic, center, r * 0.8f, ink);
  }
  return clicked;
}

void render_stage(UiContext &ctx) {
  const Palette &p = palette();
  AppState &state = ctx.state;
  const Dictation::Phase phase = ctx.dictation.phase();
  const float width = page_width();
  const bool model_ok = ctx.dictation.model_ready() || ctx.dictation.model_loading();

  ImGui::PushStyleColor(ImGuiCol_ChildBg, p.stage);
  ImGui::PushStyleColor(ImGuiCol_Border, p.dark ? p.border : p.stage);
  ImGui::PushStyleColor(ImGuiCol_Text, p.stage_text);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(30.0f, 28.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 20.0f);
  ImGui::BeginChild("stage", ImVec2(width, 236.0f),
                    ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding,
                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  ImDrawList *dl = ImGui::GetWindowDrawList();
  const ImVec2 wpos = ImGui::GetWindowPos();
  const ImVec2 wsize = ImGui::GetWindowSize();

  // Warm light leak in the corner behind the orb.
  for (int i = 0; i < 10; ++i) {
    const float k = static_cast<float>(i) / 9.0f;
    dl->AddCircleFilled(ImVec2(wpos.x + wsize.x - 120.0f, wpos.y + wsize.y * 0.5f),
                        170.0f * (1.0f - k * 0.8f), u32(with_alpha(p.accent, 0.012f + 0.006f * k)),
                        64);
  }

  const float text_w = wsize.x - 30.0f - 220.0f;

  // Title (serif) changes with the phase, cross-fading.
  const char *title = "Speak, and it's written.";
  if (!model_ok) {
    title = "Let's get you set up.";
  } else if (phase == Dictation::Phase::Recording) {
    title = "Listening\xE2\x80\xA6";
  } else if (phase == Dictation::Phase::Transcribing) {
    title = "Writing it down\xE2\x80\xA6";
  } else if (phase == Dictation::Phase::Done) {
    title = ctx.dictation.pasted() ? "Done." : "Copied.";
  } else if (phase == Dictation::Phase::Error) {
    title = "That didn't work.";
  }
  const float title_fade = anim(ImGui::GetID(title), 1.0f, 10.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * (0.3f + 0.7f * title_fade));
  ImGui::PushFont(ctx.fonts.hero);
  ImGui::TextUnformatted(title);
  ImGui::PopFont();
  ImGui::PopStyleVar();

  ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + text_w);
  const std::string live = ctx.dictation.live_text();
  if (!model_ok) {
    ImGui::PushStyleColor(ImGuiCol_Text, p.stage_muted);
    ImGui::TextUnformatted("Lisper needs a speech model to run on this computer. Tiny is 75 MB and "
                           "starts instantly; Quality is the best everyday choice.");
    ImGui::PopStyleColor();
    gap(4.0f);
    if (state.downloading) {
      const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(ImGui::GetTime()) * 4.0f);
      ImGui::PushStyleColor(ImGuiCol_Text, mix(p.stage_muted, p.stage_text, pulse));
      ImGui::TextUnformatted(state.download_status.c_str());
      ImGui::PopStyleColor();
    } else {
      if (primary_button("Download Quality  (574 MB)", ImVec2(0, 38.0f))) {
        ctx.download_model("quality");
      }
      ImGui::SameLine();
      ImGui::PushStyleColor(ImGuiCol_Text, p.stage_text);
      if (draw_ghost_on_stage("Tiny (75 MB)")) {
        ctx.download_model("tiny");
      }
      ImGui::PopStyleColor();
    }
  } else if ((phase == Dictation::Phase::Recording || phase == Dictation::Phase::Transcribing ||
              phase == Dictation::Phase::Done) &&
             !live.empty()) {
    // Live words, newest at the end, with a blinking caret while listening.
    ImGui::PushFont(ctx.fonts.lead);
    std::string shown = live;
    const float max_h = ImGui::GetFontSize() * 1.3f * 3.0f;
    while (ImGui::CalcTextSize(shown.c_str(), nullptr, false, text_w).y > max_h &&
           shown.size() > 8) {
      const size_t cut = shown.find(' ', 8);
      shown = cut == std::string::npos ? shown.substr(shown.size() / 2)
                                       : "\xE2\x80\xA6" + shown.substr(cut);
    }
    ImGui::TextUnformatted(shown.c_str());
    if (phase == Dictation::Phase::Recording && std::fmod(ImGui::GetTime(), 1.0) < 0.55) {
      const ImVec2 end = ImGui::GetItemRectMax();
      dl->AddRectFilled(ImVec2(end.x + 3.0f, end.y - ImGui::GetFontSize() - 2.0f),
                        ImVec2(end.x + 5.0f, end.y - 3.0f), u32(p.accent));
    }
    ImGui::PopFont();
  } else if (phase == Dictation::Phase::Error) {
    ImGui::PushStyleColor(ImGuiCol_Text, mix(p.danger, p.stage_text, 0.35f));
    ImGui::TextUnformatted(ctx.dictation.message().c_str());
    ImGui::PopStyleColor();
  } else {
    ImGui::PushStyleColor(ImGuiCol_Text, p.stage_muted);
    ImGui::TextUnformatted(shortcut_verb(state.settings).c_str());
    ImGui::PopStyleColor();
    ImGui::SameLine(0.0f, 6.0f);
    keycaps(shortcut_display(state.settings), true);
    ImGui::SameLine(0.0f, 6.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, p.stage_muted);
    ImGui::TextUnformatted("anywhere, talk, and Lisper types it where your cursor is.");
    ImGui::PopStyleColor();
    gap(2.0f);
    std::string hint;
    if (!state.paste_hint.empty() && state.settings.insert_mode == Settings::InsertMode::Paste) {
      hint = state.paste_hint;
    } else if (!state.hotkey_registered && !state.hotkey_error.empty()) {
      hint = "Shortcut unavailable: " + state.hotkey_error;
    }
    if (!hint.empty()) {
      ImGui::PushStyleColor(ImGuiCol_Text, mix(p.warning, p.stage_text, 0.3f));
      ImGui::TextUnformatted(hint.c_str());
      ImGui::PopStyleColor();
    } else {
      ImGui::PushStyleColor(ImGuiCol_Text, with_alpha(p.stage_muted, 0.75f));
      ImGui::TextUnformatted("Everything stays on this device.");
      ImGui::PopStyleColor();
    }
  }
  ImGui::PopTextWrapPos();

  // Waveform strip along the bottom while listening.
  {
    const ImVec2 base(wpos.x + 30.0f, wpos.y + wsize.y - 36.0f);
    const int bars = 56;
    const float strip_w = text_w;
    const float step = strip_w / static_cast<float>(bars);
    const auto levels = ctx.dictation.capture().recent_levels(static_cast<size_t>(bars));
    const float rec =
        anim(ImGui::GetID("strip_rec"), phase == Dictation::Phase::Recording ? 1.0f : 0.0f, 6.0f);
    for (int i = 0; i < bars; ++i) {
      ImGui::PushID(i);
      const float v =
          anim(ImGui::GetID("bar"),
               phase == Dictation::Phase::Recording ? levels[static_cast<size_t>(i)] : 0.0f,
               levels[static_cast<size_t>(i)] > 0.1f ? 20.0f : 8.0f);
      ImGui::PopID();
      const float idle =
          0.06f + 0.05f * std::sin(static_cast<float>(ImGui::GetTime()) * 1.8f + i * 0.35f);
      const float hgt = std::max(2.0f, (v * rec + idle * (1.0f - rec)) * 26.0f);
      const float x = base.x + step * static_cast<float>(i);
      const float k = static_cast<float>(i) / static_cast<float>(bars);
      const ImVec4 col = mix(p.accent, ImVec4(0.96f, 0.89f, 0.82f, 1.0f), k);
      dl->AddRectFilled(ImVec2(x, base.y - hgt * 0.5f),
                        ImVec2(x + step * 0.5f, base.y + hgt * 0.5f),
                        u32(with_alpha(col, 0.25f + 0.75f * rec)), 2.0f);
    }
  }

  // Orb on the right.
  const ImVec2 orb_center(wpos.x + wsize.x - 120.0f, wpos.y + wsize.y * 0.5f);
  if (model_ok && phase != Dictation::Phase::Transcribing) {
    if (draw_orb(ctx, orb_center, 42.0f)) {
      ctx.dictation.toggle();
    }
  } else {
    if (!model_ok) {
      ImGui::BeginDisabled();
    }
    draw_orb(ctx, orb_center, 42.0f);
    if (!model_ok) {
      ImGui::EndDisabled();
    }
  }

  ImGui::EndChild();
  ImGui::PopStyleVar(2);
  ImGui::PopStyleColor(3);
}

void render_stats(UiContext &ctx) {
  const auto &st = ctx.state.stats;
  const float spacing = 14.0f;
  const float w = (page_width() - spacing * 3.0f) / 4.0f;
  const double typing_minutes = static_cast<double>(st.total_words) / 40.0;
  const double speaking_minutes = static_cast<double>(st.total_audio_ms) / 60000.0;
  const double saved = std::max(0.0, typing_minutes - speaking_minutes);

  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(spacing, 10.0f));
  stat_card(ctx.fonts, "stat_words", static_cast<double>(st.total_words), "", "words dictated",
            Icon::Mic, w);
  ImGui::SameLine();
  stat_card(ctx.fonts, "stat_wpm", st.words_per_minute, "", "words per minute", Icon::Bolt, w);
  ImGui::SameLine();
  stat_card(ctx.fonts, "stat_streak", st.day_streak, st.day_streak == 1 ? " day" : " days",
            "current streak", Icon::Check, w);
  ImGui::SameLine();
  stat_card(ctx.fonts, "stat_saved", saved, " min", "saved vs. typing", Icon::Home, w);
  ImGui::PopStyleVar();
}

void render_history(UiContext &ctx) {
  AppState &state = ctx.state;
  const Palette &p = palette();

  heading(ctx.fonts, "Recent");
  const float search_w = 220.0f;
  ImGui::SameLine(page_width() - search_w);
  ImGui::SetNextItemWidth(search_w);
  ImGui::InputTextWithHint("##history_search", "Search your dictations",
                           state.history_search.data(), state.history_search.size());
  gap(4.0f);

  if (!state.settings.save_history) {
    muted("History is off. Turn it on in Settings \xE2\x86\x92 Privacy.");
    return;
  }
  if (state.history.empty()) {
    if (begin_card("history_empty", ImVec2(page_width(), 0.0f))) {
      ImGui::PushFont(ctx.fonts.heading);
      ImGui::TextUnformatted("Nothing here yet");
      ImGui::PopFont();
      muted(("Put your cursor in any text box, " +
             std::string(state.settings.hotkey_hold ? "hold " : "tap ") +
             shortcut_display(state.settings) + " and say something. It'll show up here.")
                .c_str());
    }
    end_card();
    return;
  }

  std::string query = trim_copy(state.history_search.data());
  std::transform(query.begin(), query.end(), query.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

  std::string current_day;
  int shown = 0;
  std::string pending_delete;
  const double now = ImGui::GetTime();
  for (const auto &entry : state.history) {
    if (!query.empty()) {
      std::string lower = entry.text;
      std::transform(lower.begin(), lower.end(), lower.begin(),
                     [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
      if (lower.find(query) == std::string::npos) {
        continue;
      }
    }
    if (++shown > 200) {
      break;
    }
    const std::string day = day_label(entry.timestamp_ms);
    if (day != current_day) {
      if (!current_day.empty()) {
        end_card();
        gap(8.0f);
      }
      current_day = day;
      section_label(ctx.fonts, day.c_str());
      gap(-2.0f);
      begin_card(("day_" + day).c_str(), ImVec2(page_width(), 0.0f), true, 6.0f);
    }

    ImGui::PushID(entry.id.c_str());
    // Rows slide in the first time they appear.
    ImGuiStorage *st = ImGui::GetStateStorage();
    const ImGuiID seen_id = ImGui::GetID("seen");
    float seen = st->GetFloat(seen_id, -1.0f);
    if (seen < 0.0f) {
      seen = static_cast<float>(now) + 0.03f * static_cast<float>(std::min(shown, 8));
      st->SetFloat(seen_id, seen);
    }
    const float appear = ease_out_cubic(static_cast<float>(now - seen) / 0.35f);

    const ImVec2 row_start = ImGui::GetCursorScreenPos();
    const float row_w = ImGui::GetContentRegionAvail().x;
    ImDrawList *dl = ImGui::GetWindowDrawList();
    dl->ChannelsSplit(2);
    dl->ChannelsSetCurrent(1);

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * appear);
    ImGui::SetCursorScreenPos(
        ImVec2(row_start.x + 14.0f + (1.0f - appear) * 10.0f, row_start.y + 12.0f));
    ImGui::PushFont(ctx.fonts.small);
    ImGui::PushStyleColor(ImGuiCol_Text, p.text_faint);
    ImGui::TextUnformatted(time_label(entry.timestamp_ms).c_str());
    ImGui::PopStyleColor();
    ImGui::PopFont();
    ImGui::SameLine(76.0f);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 1.0f);
    const float text_w = row_w - 76.0f - 84.0f;
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + text_w);
    ImGui::TextUnformatted(entry.text.c_str());
    ImGui::PopTextWrapPos();
    ImGui::PopStyleVar();
    const float row_end_y = ImGui::GetCursorScreenPos().y + 6.0f;
    const ImVec2 row_end(row_start.x + row_w, row_end_y);
    const bool hovered = ImGui::IsMouseHoveringRect(row_start, row_end) && ImGui::IsWindowHovered();
    const float hv = anim(ImGui::GetID("hover"), hovered ? 1.0f : 0.0f, 16.0f);

    if (hv > 0.02f) {
      ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * hv);
      ImGui::SetCursorScreenPos(ImVec2(row_start.x + row_w - 74.0f, row_start.y + 6.0f));
      if (ghost_icon_button("copy", Icon::Copy, "Copy")) {
        ctx.copy_text(entry.text);
        show_toast(state, "Copied");
      }
      ImGui::SameLine(0.0f, 4.0f);
      if (ghost_icon_button("delete", Icon::Trash, "Delete")) {
        pending_delete = entry.id;
      }
      ImGui::PopStyleVar();
    }
    dl->ChannelsSetCurrent(0);
    if (hv > 0.01f) {
      dl->AddRectFilled(row_start, row_end, u32(with_alpha(p.card_hover, hv)), 10.0f);
    }
    dl->ChannelsMerge();
    ImGui::SetCursorScreenPos(ImVec2(row_start.x, row_end_y));
    ImGui::Dummy(ImVec2(row_w, 0.0f));
    ImGui::PopID();
  }
  if (!current_day.empty()) {
    end_card();
  }
  if (shown == 0) {
    muted("Nothing matches that search.");
  }
  if (!pending_delete.empty()) {
    history::remove(pending_delete);
    state.history_dirty = true;
  }
}

void render_home(UiContext &ctx) {
  begin_page(ctx, "page_home");
  hero(ctx.fonts, greeting());
  ImGui::Dummy(ImVec2(0.0f, 18.0f));
  render_stage(ctx);
  gap(18.0f);
  render_stats(ctx);
  gap(30.0f);
  render_history(ctx);
  end_page();
}

// ---------------------------------------------------------------------------
// Transcribe files
// ---------------------------------------------------------------------------

void render_transcribe(UiContext &ctx) {
  AppState &state = ctx.state;
  BackgroundTranscriber &job = ctx.job;
  const Palette &p = palette();
  begin_page(ctx, "page_transcribe");
  page_header(ctx.fonts, "Transcribe files",
              "Recordings, meetings and videos into text, subtitles or chunks for search.");

  const float width = page_width();
  if (begin_card("drop_zone", ImVec2(width, 0.0f))) {
    const std::string input = trim_copy(state.input_path.data());
    ImDrawList *dl = ImGui::GetWindowDrawList();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float zone_w = ImGui::GetContentRegionAvail().x;
    const float zone_h = 132.0f;
    const bool hovered = ImGui::IsMouseHoveringRect(pos, ImVec2(pos.x + zone_w, pos.y + zone_h));
    const float hv = anim(ImGui::GetID("drop_hover"), hovered ? 1.0f : 0.0f, 12.0f);
    dl->AddRectFilled(pos, ImVec2(pos.x + zone_w, pos.y + zone_h),
                      u32(mix(p.input, p.accent_soft, hv)), 14.0f);
    // Marching dashed outline
    const float dash = 7.0f;
    const float offset = std::fmod(static_cast<float>(ImGui::GetTime()) * 14.0f * hv, dash * 2.0f);
    const ImU32 line = u32(mix(p.border_strong, p.accent, hv));
    for (float x = pos.x + 14.0f + offset; x < pos.x + zone_w - 14.0f; x += dash * 2.0f) {
      const float x2 = std::min(x + dash, pos.x + zone_w - 14.0f);
      dl->AddLine(ImVec2(x, pos.y + 0.5f), ImVec2(x2, pos.y + 0.5f), line, 1.2f);
      dl->AddLine(ImVec2(x, pos.y + zone_h - 0.5f), ImVec2(x2, pos.y + zone_h - 0.5f), line, 1.2f);
    }
    const float lift = hv * 3.0f;
    draw_icon(dl, Icon::File, ImVec2(pos.x + zone_w * 0.5f, pos.y + 40.0f - lift), 26.0f,
              u32(mix(p.text_muted, p.accent, hv)));
    const std::string headline = input.empty() ? std::string("Drop a file here, or click to choose")
                                               : filename_for_display(input);
    ImGui::PushFont(ctx.fonts.medium);
    const ImVec2 ts = ImGui::CalcTextSize(headline.c_str());
    dl->AddText(ImVec2(pos.x + (zone_w - ts.x) * 0.5f, pos.y + 68.0f), u32(p.text),
                headline.c_str());
    ImGui::PopFont();
    ImGui::PushFont(ctx.fonts.small);
    const char *sub = "WAV \xC2\xB7 MP3 \xC2\xB7 M4A \xC2\xB7 FLAC \xC2\xB7 MP4 \xC2\xB7 MOV "
                      "\xC2\xB7 MKV \xC2\xB7 WEBM";
    const ImVec2 ss = ImGui::CalcTextSize(sub);
    dl->AddText(ImVec2(pos.x + (zone_w - ss.x) * 0.5f, pos.y + 94.0f), u32(p.text_faint), sub);
    ImGui::PopFont();
    if (ImGui::InvisibleButton("drop_click", ImVec2(zone_w, zone_h))) {
      if (const auto selected = pick_input_media_file(input)) {
        copy_string(state.input_path, *selected);
      }
    }
    gap(4.0f);
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##input_path", "or paste a path", state.input_path.data(),
                             state.input_path.size());
  }
  end_card();

  gap();
  if (begin_card("transcribe_options", ImVec2(width, 0.0f))) {
    section_label(ctx.fonts, "Format");
    segmented("format", &state.format_index, {"Text", "Subtitles", "JSON", "Chunks"});
    gap(6.0f);
    section_label(ctx.fonts, "Save to");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 200.0f);
    ImGui::InputTextWithHint("##output_path", "Preview only", state.output_path.data(),
                             state.output_path.size());
    ImGui::SameLine();
    if (secondary_button("Folder", ImVec2(90.0f, 0))) {
      if (const auto folder = pick_output_folder(trim_copy(state.output_path.data()))) {
        copy_string(state.output_path, *folder);
      }
    }
    ImGui::SameLine();
    if (secondary_button("File", ImVec2(90.0f, 0))) {
      const std::string preview = computed_output_preview(state);
      if (const auto file =
              pick_output_file(preview.empty() ? trim_copy(state.output_path.data()) : preview)) {
        copy_string(state.output_path, *file);
      }
    }
    const std::string preview = computed_output_preview(state);
    if (!preview.empty()) {
      caption(ctx.fonts, ("Saves as " + preview).c_str());
    }

    gap(8.0f);
    state.resolved_model_path = state.settings.model_path.empty()
                                    ? resolve_model_path("", state.settings.model_profile)
                                    : state.settings.model_path;
    const auto issues = validation_issues(state);
    const bool can_start = !job.running() && issues.empty();
    if (job.running()) {
      // Indeterminate progress bar sweeping across.
      const ImVec2 bpos = ImGui::GetCursorScreenPos();
      const float bw = ImGui::GetContentRegionAvail().x - 130.0f;
      ImDrawList *dl = ImGui::GetWindowDrawList();
      dl->AddRectFilled(ImVec2(bpos.x, bpos.y + 16.0f), ImVec2(bpos.x + bw, bpos.y + 20.0f),
                        u32(p.input), 2.0f);
      const float k = std::fmod(static_cast<float>(ImGui::GetTime()) * 0.7f, 1.0f);
      const float seg = bw * 0.28f;
      const float x0 = bpos.x - seg + (bw + seg) * ease_in_out(k);
      dl->AddRectFilled(ImVec2(std::max(bpos.x, x0), bpos.y + 16.0f),
                        ImVec2(std::min(bpos.x + bw, x0 + seg), bpos.y + 20.0f), u32(p.accent),
                        2.0f);
      ImGui::Dummy(ImVec2(bw, 36.0f));
      ImGui::SameLine();
      if (danger_button("Cancel", ImVec2(120.0f, 36.0f))) {
        job.cancel();
      }
      caption(ctx.fonts, job.status().c_str());
    } else {
      if (!can_start) {
        ImGui::BeginDisabled();
      }
      if (primary_button("Transcribe", ImVec2(150.0f, 40.0f))) {
        auto request = build_request(state);
        request.shared_engine = ctx.dictation.engine();
        state.status_line =
            "Transcribing " + filename_for_display(request.input_path) + "\xE2\x80\xA6";
        state.preview_text.clear();
        job.start(request);
      }
      if (!can_start) {
        ImGui::EndDisabled();
        for (const auto &issue : issues) {
          caption(ctx.fonts, issue.c_str());
        }
      }
    }
  }
  end_card();

  if (!state.preview_text.empty()) {
    gap();
    if (begin_card("transcript_result", ImVec2(width, 0.0f))) {
      heading(ctx.fonts, "Transcript");
      if (state.duration_ms > 0) {
        caption(ctx.fonts, (format_duration_ms(state.duration_ms) + " of audio \xC2\xB7 " +
                            std::to_string(state.segment_count) + " segments \xC2\xB7 " +
                            (state.detected_language.empty() ? "?" : state.detected_language))
                               .c_str());
      }
      if (!state.final_output_path.empty()) {
        caption(ctx.fonts, ("Saved to " + state.final_output_path).c_str());
      }
      gap(4.0f);
      ImGui::PushStyleColor(ImGuiCol_ChildBg, p.input);
      ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 14));
      ImGui::BeginChild("transcript_text", ImVec2(0, 300.0f),
                        ImGuiChildFlags_AlwaysUseWindowPadding);
      ImGui::PushFont(ctx.fonts.lead);
      ImGui::PushTextWrapPos(0.0f);
      ImGui::TextUnformatted(state.preview_text.c_str());
      ImGui::PopTextWrapPos();
      ImGui::PopFont();
      ImGui::EndChild();
      ImGui::PopStyleVar();
      ImGui::PopStyleColor();
      gap(4.0f);
      if (secondary_button("Copy transcript")) {
        ctx.copy_text(state.preview_text);
        show_toast(state, "Transcript copied");
      }
    }
    end_card();
  } else if (!state.status_line.empty() && !job.running()) {
    gap(6.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, state.last_run_success ? p.success : p.danger);
    ImGui::TextWrapped("%s", state.status_line.c_str());
    ImGui::PopStyleColor();
  }
  end_page();
}

// ---------------------------------------------------------------------------
// Dictionary
// ---------------------------------------------------------------------------

void render_dictionary(UiContext &ctx) {
  AppState &state = ctx.state;
  Settings &s = state.settings;
  const Palette &p = palette();
  begin_page(ctx, "page_dictionary");
  page_header(
      ctx.fonts, "Dictionary",
      "Names, jargon and spellings. Corrections fix what Lisper mishears; vocabulary helps it "
      "hear them right the first time.");

  const float width = page_width();
  if (begin_card("dict_add", ImVec2(width, 0.0f))) {
    section_label(ctx.fonts, "Corrections");
    gap(2.0f);
    const float field_w = (ImGui::GetContentRegionAvail().x - 130.0f) * 0.5f;
    ImGui::SetNextItemWidth(field_w);
    ImGui::InputTextWithHint("##dict_from", "When I say\xE2\x80\xA6  (lisa per)",
                             state.dict_from.data(), state.dict_from.size());
    ImGui::SameLine(0.0f, 10.0f);
    ImGui::AlignTextToFramePadding();
    ImGui::PushStyleColor(ImGuiCol_Text, p.text_faint);
    ImGui::TextUnformatted("\xE2\x86\x92");
    ImGui::PopStyleColor();
    ImGui::SameLine(0.0f, 10.0f);
    ImGui::SetNextItemWidth(field_w);
    const bool enter =
        ImGui::InputTextWithHint("##dict_to", "Write  (Lisper)", state.dict_to.data(),
                                 state.dict_to.size(), ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    const std::string from = trim_copy(state.dict_from.data());
    const std::string to = trim_copy(state.dict_to.data());
    if ((dark_button("Add", ImVec2(-1.0f, 0)) || enter) && !from.empty() && !to.empty()) {
      s.dictionary.push_back({from, to});
      copy_string(state.dict_from, "");
      copy_string(state.dict_to, "");
      mark_dirty(state);
    }

    gap(4.0f);
    if (s.dictionary.empty()) {
      caption(ctx.fonts, "No corrections yet.");
    }
    int remove_index = -1;
    for (size_t i = 0; i < s.dictionary.size(); ++i) {
      ImGui::PushID(static_cast<int>(i));
      divider();
      ImGui::AlignTextToFramePadding();
      ImGui::PushStyleColor(ImGuiCol_Text, p.text_muted);
      ImGui::TextUnformatted(s.dictionary[i].from.c_str());
      ImGui::PopStyleColor();
      ImGui::SameLine(field_w + 50.0f);
      ImGui::PushFont(ctx.fonts.medium);
      ImGui::TextUnformatted(s.dictionary[i].to.c_str());
      ImGui::PopFont();
      ImGui::SameLine(ImGui::GetContentRegionMax().x - 30.0f);
      if (ghost_icon_button("del", Icon::Trash, "Remove")) {
        remove_index = static_cast<int>(i);
      }
      ImGui::PopID();
    }
    if (remove_index >= 0) {
      s.dictionary.erase(s.dictionary.begin() + remove_index);
      mark_dirty(state);
    }
  }
  end_card();

  gap();
  if (begin_card("vocab", ImVec2(width, 0.0f))) {
    section_label(ctx.fonts, "Vocabulary");
    gap(2.0f);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 96.0f);
    const bool enter =
        ImGui::InputTextWithHint("##vocab", "Add a word or name", state.vocab_word.data(),
                                 state.vocab_word.size(), ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    const std::string word = trim_copy(state.vocab_word.data());
    if ((dark_button("Add", ImVec2(-1.0f, 0)) || enter) && !word.empty()) {
      s.vocabulary.push_back(word);
      copy_string(state.vocab_word, "");
      mark_dirty(state);
    }
    gap(4.0f);
    int remove_index = -1;
    const float right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    for (size_t i = 0; i < s.vocabulary.size(); ++i) {
      ImGui::PushID(static_cast<int>(i));
      const std::string label = s.vocabulary[i] + "   \xC3\x97";
      const float w = ImGui::CalcTextSize(label.c_str()).x + 26.0f;
      if (i > 0 && ImGui::GetItemRectMax().x + 8.0f + w < right) {
        ImGui::SameLine(0.0f, 8.0f);
      }
      const ImVec2 pos = ImGui::GetCursorScreenPos();
      if (ImGui::InvisibleButton("chip", ImVec2(w, 30.0f))) {
        remove_index = static_cast<int>(i);
      }
      const float hv = anim(ImGui::GetItemID(), ImGui::IsItemHovered() ? 1.0f : 0.0f, 14.0f);
      ImDrawList *dl = ImGui::GetWindowDrawList();
      dl->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + 30.0f),
                        u32(mix(p.accent_soft, with_alpha(p.danger, 0.14f), hv)), 999.0f);
      dl->AddText(ImVec2(pos.x + 13.0f, pos.y + 7.0f), u32(mix(p.accent_text, p.danger, hv)),
                  label.c_str());
      ImGui::PopID();
    }
    if (s.vocabulary.empty()) {
      caption(ctx.fonts, "Product names, people, acronyms\xE2\x80\xA6");
    }
    if (remove_index >= 0) {
      s.vocabulary.erase(s.vocabulary.begin() + remove_index);
      mark_dirty(state);
    }
  }
  end_card();
  end_page();
}

// ---------------------------------------------------------------------------
// Snippets
// ---------------------------------------------------------------------------

void render_snippets(UiContext &ctx) {
  AppState &state = ctx.state;
  Settings &s = state.settings;
  const Palette &p = palette();
  begin_page(ctx, "page_snippets");
  page_header(ctx.fonts, "Snippets",
              "Say a short cue, get the full text: links, addresses, sign-offs, replies you send "
              "every day.");

  const float width = page_width();
  if (begin_card("snip_add", ImVec2(width, 0.0f))) {
    section_label(ctx.fonts, "New snippet");
    gap(2.0f);
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##snip_trigger", "When I say\xE2\x80\xA6  (my calendar link)",
                             state.snippet_trigger.data(), state.snippet_trigger.size());
    ImGui::InputTextMultiline("##snip_text", state.snippet_text.data(), state.snippet_text.size(),
                              ImVec2(-1.0f, 92.0f));
    const std::string trigger = trim_copy(state.snippet_trigger.data());
    const std::string text = state.snippet_text.data();
    const bool ok = !trigger.empty() && !text.empty();
    if (!ok) {
      ImGui::BeginDisabled();
    }
    if (dark_button("Save snippet")) {
      s.snippets.push_back({trigger, text});
      copy_string(state.snippet_trigger, "");
      copy_string(state.snippet_text, "");
      mark_dirty(state);
    }
    if (!ok) {
      ImGui::EndDisabled();
    }
  }
  end_card();

  gap();
  int remove_index = -1;
  for (size_t i = 0; i < s.snippets.size(); ++i) {
    ImGui::PushID(static_cast<int>(i));
    if (begin_card("snippet", ImVec2(width, 0.0f), true, 18.0f)) {
      ImGui::PushFont(ctx.fonts.heading);
      ImGui::PushStyleColor(ImGuiCol_Text, p.accent_text);
      ImGui::Text("\xE2\x80\x9C%s\xE2\x80\x9D", s.snippets[i].trigger.c_str());
      ImGui::PopStyleColor();
      ImGui::PopFont();
      ImGui::SameLine(ImGui::GetContentRegionMax().x - 30.0f);
      if (ghost_icon_button("del", Icon::Trash, "Remove")) {
        remove_index = static_cast<int>(i);
      }
      muted(s.snippets[i].text.c_str());
    }
    end_card();
    gap(4.0f);
    ImGui::PopID();
  }
  if (s.snippets.empty()) {
    muted("No snippets yet.");
  }
  if (remove_index >= 0) {
    s.snippets.erase(s.snippets.begin() + remove_index);
    mark_dirty(state);
  }
  end_page();
}

// ---------------------------------------------------------------------------
// Style
// ---------------------------------------------------------------------------

void style_choice(UiContext &ctx, const char *id, const char *title, const char *sample,
                  text_cleanup::Style value, float width) {
  Settings &s = ctx.state.settings;
  const Palette &p = palette();
  const bool selected = s.style == value;
  const float sel = anim(ImGui::GetID(id), selected ? 1.0f : 0.0f, 12.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f + sel);
  if (begin_card(id, ImVec2(width, 0.0f), true, 22.0f, mix(p.border, p.accent, sel))) {
    heading(ctx.fonts, title);
    gap(2.0f);
    ImGui::PushFont(ctx.fonts.lead);
    ImGui::PushStyleColor(ImGuiCol_Text, mix(p.text_muted, p.text, sel));
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(sample);
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
    ImGui::PopFont();
    const ImVec2 min = ImGui::GetWindowPos();
    const ImVec2 max(min.x + ImGui::GetWindowWidth(), min.y + ImGui::GetWindowHeight());
    if (ImGui::IsMouseHoveringRect(min, max)) {
      ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
      if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        s.style = value;
        mark_dirty(ctx.state);
      }
    }
  }
  end_card();
  ImGui::PopStyleVar();
}

void render_style(UiContext &ctx) {
  AppState &state = ctx.state;
  Settings &s = state.settings;
  begin_page(ctx, "page_style");
  page_header(ctx.fonts, "Style", "How your words read once they land on the page.");

  const float w = (page_width() - 14.0f) * 0.5f;
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(14.0f, 10.0f));
  style_choice(ctx, "style_formal", "Formal",
               "Hi Sam, thanks for the update. Let's ship it on Friday.",
               text_cleanup::Style::Formal, w);
  ImGui::SameLine();
  style_choice(ctx, "style_casual", "Casual",
               "hi Sam, thanks for the update. let's ship it on Friday",
               text_cleanup::Style::Casual, w);
  ImGui::PopStyleVar();

  gap();
  if (begin_card("cleanup", ImVec2(page_width(), 0.0f))) {
    section_label(ctx.fonts, "Cleanup");
    gap(2.0f);
    if (setting_row_toggle(
            "Remove filler words",
            "Drop \xE2\x80\x9Cum\xE2\x80\x9D, \xE2\x80\x9Cuh\xE2\x80\x9D and friends.",
            &s.remove_fillers)) {
      mark_dirty(state);
    }
    divider();
    if (setting_row_toggle("Voice commands",
                           "Say \xE2\x80\x9Cnew line\xE2\x80\x9D or \xE2\x80\x9Cnew "
                           "paragraph\xE2\x80\x9D to format as you go.",
                           &s.voice_commands)) {
      mark_dirty(state);
    }
  }
  end_card();
  end_page();
}

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------

const char *kLanguages[][2] = {
    {"auto", "Detect automatically"},
    {"en", "English"},
    {"es", "Spanish"},
    {"fr", "French"},
    {"de", "German"},
    {"it", "Italian"},
    {"pt", "Portuguese"},
    {"nl", "Dutch"},
    {"pl", "Polish"},
    {"ru", "Russian"},
    {"uk", "Ukrainian"},
    {"tr", "Turkish"},
    {"ar", "Arabic"},
    {"hi", "Hindi"},
    {"zh", "Chinese"},
    {"ja", "Japanese"},
    {"ko", "Korean"},
    {"sv", "Swedish"},
};

void render_settings(UiContext &ctx) {
  AppState &state = ctx.state;
  Settings &s = state.settings;
  const Palette &p = palette();
  begin_page(ctx, "page_settings");
  page_header(ctx.fonts, "Settings", nullptr);
  const float width = page_width();

  // Shortcut
  if (begin_card("set_shortcut", ImVec2(width, 0.0f))) {
    section_label(ctx.fonts, "Shortcut");
    gap(2.0f);
    const Hotkey hk = parse_hotkey(s.hotkey);
    if (state.capturing_hotkey) {
      const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(ImGui::GetTime()) * 5.0f);
      ImGui::PushStyleColor(ImGuiCol_Text, mix(p.accent, p.text, pulse * 0.3f));
      ImGui::TextUnformatted(
          "Press the new shortcut, or just the modifier keys (like \xE2\x8C\x98\xE2\x8C\xA5) "
          "and let go.  Esc cancels.");
      ImGui::PopStyleColor();
    } else {
      ImGui::PushFont(ctx.fonts.medium);
      keycaps(hk.display());
      ImGui::PopFont();
      ImGui::SameLine(0.0f, 16.0f);
      if (secondary_button("Change")) {
        state.capturing_hotkey = true;
      }
      ImGui::SameLine(0.0f, 8.0f);
      if (secondary_button("Reset")) {
        s.hotkey = default_hotkey();
        mark_dirty(state);
        ctx.rebind_hotkey();
      }
    }
    if (!state.hotkey_registered && !state.hotkey_error.empty()) {
      ImGui::PushStyleColor(ImGuiCol_Text, p.warning);
      ImGui::TextWrapped("%s", state.hotkey_error.c_str());
      ImGui::PopStyleColor();
    }
    gap(4.0f);
    int mode = s.hotkey_hold ? 1 : 0;
    if (segmented("hotkey_mode", &mode,
                  {hk.is_chord() ? "Tap to start / stop" : "Press to start / stop", "Hold to talk"},
                  380.0f)) {
      s.hotkey_hold = mode == 1;
      mark_dirty(state);
    }
#if !defined(_WIN32) && !defined(__APPLE__)
    caption(ctx.fonts,
            "On Wayland, bind a system shortcut to \xE2\x80\x9Clisper_gui --toggle\xE2\x80\x9D.");
#endif
  }
  end_card();
  gap();

  // Dictation
  if (begin_card("set_dictation", ImVec2(width, 0.0f))) {
    section_label(ctx.fonts, "Dictation");
    gap(2.0f);
    ImGui::TextUnformatted("Microphone");
    if (state.input_devices.empty()) {
      state.input_devices = AudioCapture::list_input_devices();
    }
    const std::string current = s.input_device.empty() ? "System default" : s.input_device;
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 110.0f);
    if (ImGui::BeginCombo("##mic", current.c_str())) {
      if (ImGui::Selectable("System default", s.input_device.empty())) {
        s.input_device.clear();
        mark_dirty(state);
      }
      for (const auto &dev : state.input_devices) {
        if (ImGui::Selectable(dev.c_str(), dev == s.input_device)) {
          s.input_device = dev;
          mark_dirty(state);
        }
      }
      ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (secondary_button("Refresh", ImVec2(-1.0f, 0))) {
      state.input_devices = AudioCapture::list_input_devices();
    }
    gap(4.0f);
    ImGui::TextUnformatted("When you finish speaking");
    int mode = s.insert_mode == Settings::InsertMode::Paste ? 0 : 1;
    if (segmented("insert_mode", &mode, {"Type it where I am", "Copy to clipboard"}, 380.0f)) {
      s.insert_mode = mode == 0 ? Settings::InsertMode::Paste : Settings::InsertMode::Clipboard;
      mark_dirty(state);
    }
    if (!state.paste_hint.empty() && s.insert_mode == Settings::InsertMode::Paste) {
      ImGui::PushStyleColor(ImGuiCol_Text, p.warning);
      ImGui::TextWrapped("%s", state.paste_hint.c_str());
      ImGui::PopStyleColor();
#ifdef __APPLE__
      if (secondary_button("Open Accessibility settings")) {
        ctx.request_accessibility();
      }
#endif
    }
    gap(6.0f);
    if (setting_row_toggle("Live preview",
                           "See your words appear while you\xE2\x80\x99re still talking.",
                           &s.live_preview)) {
      mark_dirty(state);
    }
    divider();
    if (setting_row_toggle("Show the island",
                           "A small capsule with a live waveform while you dictate.",
                           &s.show_overlay)) {
      mark_dirty(state);
    }
    if (s.show_overlay) {
      int pos = s.island_top ? 0 : 1;
#ifdef __APPLE__
      const char *top_label = "In the notch";
#else
      const char *top_label = "Top of screen";
#endif
      if (segmented("island_pos", &pos, {top_label, "Bottom of screen"}, 380.0f)) {
        s.island_top = pos == 0;
        mark_dirty(state);
      }
      gap(2.0f);
    }
    divider();
    if (setting_row_toggle("Restore clipboard", "Put back what you had copied after inserting.",
                           &s.restore_clipboard)) {
      mark_dirty(state);
    }
    divider();
    if (setting_row_toggle("Sounds", "A soft chime when listening starts and stops.", &s.sounds)) {
      mark_dirty(state);
    }
  }
  end_card();
  gap();

  // Model & language
  if (begin_card("set_model", ImVec2(width, 0.0f))) {
    section_label(ctx.fonts, "Speech model");
    gap(2.0f);
    for (const auto &profile : all_model_profiles()) {
      ImGui::PushID(profile.name.c_str());
      const bool selected = s.model_profile == profile.name && s.model_path.empty();
      const bool installed = !resolve_model_path("", profile.name).empty();
      const ImVec2 row = ImGui::GetCursorScreenPos();
      const float row_w = ImGui::GetContentRegionAvail().x;
      const float row_h = 54.0f;
      if (ImGui::InvisibleButton("row", ImVec2(row_w - 120.0f, row_h)) && installed) {
        s.model_profile = profile.name;
        s.model_path.clear();
        mark_dirty(state);
      }
      const float hv = anim(ImGui::GetItemID(), ImGui::IsItemHovered() ? 1.0f : 0.0f, 14.0f);
      const float sel = anim(ImGui::GetItemID() + 5, selected ? 1.0f : 0.0f, 12.0f);
      ImDrawList *dl = ImGui::GetWindowDrawList();
      dl->AddRectFilled(row, ImVec2(row.x + row_w, row.y + row_h),
                        u32(with_alpha(mix(p.card_hover, p.accent_soft, sel), std::max(hv, sel))),
                        12.0f);
      const ImVec2 rc(row.x + 20.0f, row.y + row_h * 0.5f);
      dl->AddCircle(rc, 8.0f, u32(mix(p.border_strong, p.accent, sel)), 24, 1.6f);
      dl->AddCircleFilled(rc, 4.0f * sel, u32(p.accent), 16);
      std::string name = profile.name;
      name[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
      ImGui::PushFont(ctx.fonts.medium);
      dl->AddText(ImVec2(row.x + 40.0f, row.y + 9.0f), u32(p.text), name.c_str());
      ImGui::PopFont();
      ImGui::PushFont(ctx.fonts.small);
      dl->AddText(ImVec2(row.x + 40.0f, row.y + 30.0f), u32(p.text_muted),
                  profile.description.c_str());
      ImGui::PopFont();
      ImGui::SetCursorScreenPos(ImVec2(row.x + row_w - 108.0f, row.y + 11.0f));
      if (installed) {
        ImGui::SetCursorScreenPos(ImVec2(row.x + row_w - 96.0f, row.y + 15.0f));
        ImGui::PushFont(ctx.fonts.small);
        pill("Installed", p.success, with_alpha(p.success, 0.12f));
        ImGui::PopFont();
      } else if (!state.downloading) {
        if (secondary_button("Download", ImVec2(100.0f, 32.0f))) {
          ctx.download_model(profile.name);
        }
      }
      ImGui::SetCursorScreenPos(ImVec2(row.x, row.y + row_h + 4.0f));
      ImGui::Dummy(ImVec2(row_w, 0.0f));
      ImGui::PopID();
    }
    if (state.downloading || !state.download_status.empty()) {
      caption(ctx.fonts, state.download_status.c_str());
    }
    caption(
        ctx.fonts,
        ("Using " + ctx.dictation.model_status() + "  \xC2\xB7  " + paths::models_dir()).c_str());

    gap(8.0f);
    ImGui::TextUnformatted("Language");
    const char *current_lang = s.language.c_str();
    for (const auto &l : kLanguages) {
      if (s.language == l[0]) {
        current_lang = l[1];
      }
    }
    ImGui::SetNextItemWidth(300.0f);
    if (ImGui::BeginCombo("##language", current_lang)) {
      for (const auto &l : kLanguages) {
        if (ImGui::Selectable(l[1], s.language == l[0])) {
          s.language = l[0];
          mark_dirty(state);
        }
      }
      ImGui::EndCombo();
    }
    gap(4.0f);
    if (setting_row_toggle("Translate to English", "Speak any language, get English.",
                           &s.translate)) {
      mark_dirty(state);
    }
    divider();
    ImGui::TextUnformatted("Run on");
    int device = static_cast<int>(s.device);
    if (segmented("device", &device, {"Automatic", "CPU", "GPU"}, 300.0f)) {
      s.device = static_cast<LisperConfig::Device>(device);
      mark_dirty(state);
    }
  }
  end_card();
  gap();

  // Appearance & behaviour
  if (begin_card("set_app", ImVec2(width, 0.0f))) {
    section_label(ctx.fonts, "Appearance");
    gap(2.0f);
    int theme = s.dark_theme ? 1 : 0;
    if (segmented("theme", &theme, {"Light", "Dark"}, 240.0f)) {
      s.dark_theme = theme == 1;
      apply_theme(s.dark_theme);
      mark_dirty(state);
    }
    gap(6.0f);
    if (setting_row_toggle("Keep running when closed",
                           "Closing the window keeps the shortcut working.",
                           &s.close_to_background)) {
      mark_dirty(state);
    }
  }
  end_card();
  gap();

  // Privacy
  if (begin_card("set_privacy", ImVec2(width, 0.0f))) {
    section_label(ctx.fonts, "Privacy");
    gap(2.0f);
    muted("Your voice is transcribed on this device and never uploaded. Audio is not saved.");
    gap(6.0f);
    if (setting_row_toggle(
            "Keep history",
            "A private, searchable list of what you dictated. Powers the stats on Home.",
            &s.save_history)) {
      mark_dirty(state);
    }
    if (danger_button("Clear history")) {
      history::clear();
      state.history_dirty = true;
      show_toast(state, "History cleared");
    }
    caption(ctx.fonts, paths::data_dir().c_str());
  }
  end_card();
  gap();

  // MCP
  if (begin_card("set_mcp", ImVec2(width, 0.0f))) {
    section_label(ctx.fonts, "Use with AI assistants (MCP)");
    gap(2.0f);
    muted("Let any MCP client transcribe files and take dictation through Lisper, on-device. Add "
          "this to "
          "the client's MCP configuration:");
    const std::string config =
        "{\n  \"mcpServers\": {\n    \"lisper\": {\n      \"command\": \"lisper-mcp\",\n"
        "      \"args\": [\"--allow-microphone\"]\n    }\n  }\n}";
    gap(2.0f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, p.input);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 14));
    ImGui::BeginChild("mcp_config", ImVec2(0, 0),
                      ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PushFont(ctx.fonts.small);
    ImGui::TextUnformatted(config.c_str());
    ImGui::PopFont();
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    gap(2.0f);
    if (secondary_button("Copy config")) {
      ctx.copy_text(config);
      show_toast(state, "Config copied");
    }
  }
  end_card();
  gap(18.0f);
  caption(ctx.fonts, "Lisper " LISPER_VERSION "  \xC2\xB7  whisper.cpp  \xC2\xB7  MIT");
  gap(4.0f);
  if (secondary_button("Quit Lisper")) {
    state.want_quit = true;
  }
  end_page();
}

void render_toast(UiContext &ctx) {
  AppState &state = ctx.state;
  const double now = steady_seconds();
  const float remaining = static_cast<float>(state.toast_until - now);
  if (state.toast.empty() || remaining < -0.3f) {
    return;
  }
  const float total = 2.5f;
  const float in = ease_out_cubic((total - remaining) / 0.28f);
  const float out = remaining < 0.0f ? 1.0f + remaining / 0.3f : 1.0f;
  const float a = std::clamp(std::min(in, out), 0.0f, 1.0f);

  const Palette &p = palette();
  const ImGuiViewport *vp = ImGui::GetMainViewport();
  ImGui::PushFont(ctx.fonts.medium);
  const ImVec2 ts = ImGui::CalcTextSize(state.toast.c_str());
  const ImVec2 size(ts.x + 52.0f, ts.y + 22.0f);
  const ImVec2 pos(vp->Pos.x + kSidebarWidth + (vp->Size.x - kSidebarWidth - size.x) * 0.5f,
                   vp->Pos.y + vp->Size.y - size.y - 30.0f + (1.0f - a) * 18.0f);
  ImDrawList *dl = ImGui::GetForegroundDrawList();
  dl->AddRectFilled(ImVec2(pos.x, pos.y + 3.0f), ImVec2(pos.x + size.x, pos.y + size.y + 4.0f),
                    IM_COL32(0, 0, 0, static_cast<int>(28 * a)), 999.0f);
  dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), u32(with_alpha(p.primary, a)),
                    999.0f);
  draw_icon(dl, Icon::Check, ImVec2(pos.x + 20.0f, pos.y + size.y * 0.5f), 14.0f,
            u32(with_alpha(p.accent, a)));
  dl->AddText(ImVec2(pos.x + 34.0f, pos.y + 11.0f), u32(with_alpha(p.primary_text, a)),
              state.toast.c_str());
  ImGui::PopFont();
}

} // namespace

void render_main_window(UiContext &ctx) {
  const ImGuiViewport *viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->Pos);
  ImGui::SetNextWindowSize(viewport->Size);
  const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                                 ImGuiWindowFlags_NoBringToFrontOnFocus |
                                 ImGuiWindowFlags_NoScrollbar;
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::Begin("Lisper", nullptr, flags);
  ImGui::PopStyleVar();

  render_sidebar(ctx);
  ImGui::SameLine(0.0f, 0.0f);

  switch (ctx.state.page) {
  case Page::Home:
    render_home(ctx);
    break;
  case Page::Transcribe:
    render_transcribe(ctx);
    break;
  case Page::Dictionary:
    render_dictionary(ctx);
    break;
  case Page::Snippets:
    render_snippets(ctx);
    break;
  case Page::Style:
    render_style(ctx);
    break;
  case Page::Settings:
    render_settings(ctx);
    break;
  }

  render_toast(ctx);
  ImGui::End();
}

} // namespace gui
