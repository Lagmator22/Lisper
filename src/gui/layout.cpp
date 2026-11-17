#include "gui/layout.h"

#include "audio_capture.h"
#include "gui/file_dialog.h"
#include "gui/hotkey.h"
#include "paths.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace gui {

namespace {

constexpr float kSidebarWidth = 228.0f;
constexpr float kContentMaxWidth = 860.0f;
float g_page_x = 0.0f; // left edge of the current page column (window-local)

ImU32 u32(const ImVec4 &c) {
  return ImGui::GetColorU32(c);
}

void mark_dirty(AppState &state) {
  state.settings_dirty = true;
}

std::string format_number(int64_t n) {
  std::string s = std::to_string(n);
  for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3) {
    s.insert(static_cast<size_t>(i), ",");
  }
  return s;
}

std::string day_label(int64_t ts_ms) {
  const std::time_t t = static_cast<std::time_t>(ts_ms / 1000);
  const std::time_t now = std::time(nullptr);
  std::tm a{};
  std::tm b{};
#ifdef _WIN32
  localtime_s(&a, &t);
  localtime_s(&b, &now);
#else
  localtime_r(&t, &a);
  localtime_r(&now, &b);
#endif
  if (a.tm_year == b.tm_year && a.tm_yday == b.tm_yday) {
    return "Today";
  }
  const std::time_t yesterday = now - 86400;
  std::tm y{};
#ifdef _WIN32
  localtime_s(&y, &yesterday);
#else
  localtime_r(&yesterday, &y);
#endif
  if (a.tm_year == y.tm_year && a.tm_yday == y.tm_yday) {
    return "Yesterday";
  }
  char buf[64];
  std::strftime(buf, sizeof(buf), "%A, %b %d", &a);
  return buf;
}

std::string time_label(int64_t ts_ms) {
  const std::time_t t = static_cast<std::time_t>(ts_ms / 1000);
  std::tm a{};
#ifdef _WIN32
  localtime_s(&a, &t);
#else
  localtime_r(&t, &a);
#endif
  char buf[16];
  std::strftime(buf, sizeof(buf), "%H:%M", &a);
  return buf;
}

// Centre page content and cap its width so long lines stay readable.
void begin_page(const char *id) {
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::BeginChild(id, ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_None);
  ImGui::PopStyleVar();
  const float avail = ImGui::GetContentRegionAvail().x;
  const float width = std::min(kContentMaxWidth, avail - 64.0f);
  const float pad = std::max(32.0f, (avail - width) * 0.5f);
  ImGui::SetCursorPos(ImVec2(pad, 36.0f));
  g_page_x = pad;
  ImGui::BeginGroup();
  ImGui::PushItemWidth(width);
  ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width);
  ImGui::Dummy(ImVec2(width, 0.0f));
}

void end_page() {
  ImGui::Dummy(ImVec2(0.0f, 40.0f));
  ImGui::PopTextWrapPos();
  ImGui::PopItemWidth();
  ImGui::EndGroup();
  ImGui::EndChild();
}

float page_width() {
  return ImGui::CalcItemWidth();
}

void page_header(const Fonts &fonts, const char *title, const char *subtitle) {
  hero(fonts, title);
  if (subtitle != nullptr) {
    muted(subtitle);
  }
  ImGui::Dummy(ImVec2(0.0f, 12.0f));
}

// ---------------------------------------------------------------------------
// Sidebar
// ---------------------------------------------------------------------------

void render_sidebar(UiContext &ctx) {
  AppState &state = ctx.state;
  const Palette &p = palette();

  ImGui::PushStyleColor(ImGuiCol_ChildBg, p.sidebar);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 20.0f));
  ImGui::BeginChild("sidebar", ImVec2(kSidebarWidth, 0.0f), ImGuiChildFlags_AlwaysUseWindowPadding,
                    ImGuiWindowFlags_NoScrollbar);

  // Brand
  {
    ImDrawList *dl = ImGui::GetWindowDrawList();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    dl->AddRectFilled(ImVec2(pos.x + 6, pos.y), ImVec2(pos.x + 34, pos.y + 28), u32(p.primary),
                      8.0f);
    const float heights[] = {0.35f, 0.7f, 1.0f, 0.6f};
    for (int i = 0; i < 4; ++i) {
      const float h = 14.0f * heights[i];
      const float x = pos.x + 12.0f + static_cast<float>(i) * 4.6f;
      dl->AddRectFilled(ImVec2(x, pos.y + 14 - h * 0.5f), ImVec2(x + 2.6f, pos.y + 14 + h * 0.5f),
                        u32(p.primary_text), 2.0f);
    }
    ImGui::SetCursorScreenPos(ImVec2(pos.x + 44, pos.y + 1));
    ImGui::PushFont(ctx.fonts.heading);
    ImGui::TextUnformatted("Lisper");
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(0.0f, 14.0f));
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
  for (const auto &e : entries) {
    if (nav_item(ctx.fonts, e.label, e.icon, state.page == e.page)) {
      state.page = e.page;
      state.capturing_hotkey = false;
    }
  }

  // Footer: dictation status + shortcut
  const float footer_h = 118.0f;
  ImGui::SetCursorPosY(
      std::max(ImGui::GetCursorPosY() + 12.0f, ImGui::GetWindowHeight() - footer_h));
  ImGui::Separator();
  ImGui::Spacing();

  const Dictation::Phase phase = ctx.dictation.phase();
  ImVec4 dot = p.text_faint;
  std::string status = ctx.dictation.model_status();
  if (ctx.dictation.model_loading()) {
    dot = p.warning;
  } else if (ctx.dictation.model_ready()) {
    dot = phase == Dictation::Phase::Recording ? p.danger : p.success;
    status = phase == Dictation::Phase::Idle ? "Ready to dictate" : phase_label(phase);
  }
  status_dot(dot);
  ImGui::SameLine();
  ImGui::PushFont(ctx.fonts.small);
  ImGui::PushStyleColor(ImGuiCol_Text, p.text_muted);
  ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + kSidebarWidth - 50.0f);
  ImGui::TextUnformatted(status.c_str());
  ImGui::PopTextWrapPos();
  ImGui::PopStyleColor();
  ImGui::PopFont();

  ImGui::PushFont(ctx.fonts.small);
  keycaps(state.settings.hotkey);
  ImGui::PopFont();
  ImGui::PushFont(ctx.fonts.small);
  ImGui::PushStyleColor(ImGuiCol_Text, p.text_faint);
  ImGui::TextUnformatted(state.settings.hotkey_hold ? "hold to dictate" : "press to start / stop");
  ImGui::PopStyleColor();
  ImGui::PopFont();

  ImGui::EndChild();
  ImGui::PopStyleVar();
  ImGui::PopStyleColor();
}

// ---------------------------------------------------------------------------
// Home
// ---------------------------------------------------------------------------

void render_dictation_card(UiContext &ctx) {
  const Palette &p = palette();
  AppState &state = ctx.state;
  const Dictation::Phase phase = ctx.dictation.phase();
  const float width = page_width();

  // Dark hero card in both themes, echoing the floating bar.
  ImGui::PushStyleColor(ImGuiCol_ChildBg, p.dark ? color_v4(30, 29, 36) : color_v4(24, 22, 28));
  ImGui::PushStyleColor(ImGuiCol_Border, p.dark ? p.border : color_v4(24, 22, 28));
  ImGui::PushStyleColor(ImGuiCol_Text, color_v4(245, 244, 242));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24.0f, 22.0f));
  ImGui::BeginChild("dictation_card", ImVec2(width, 0.0f),
                    ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY |
                        ImGuiChildFlags_AlwaysUseWindowPadding,
                    ImGuiWindowFlags_NoScrollbar);

  ImDrawList *dl = ImGui::GetWindowDrawList();

  ImGui::PushFont(ctx.fonts.heading);
  const char *title = "Dictate anywhere";
  if (phase == Dictation::Phase::Recording) {
    title = "Listening...";
  } else if (phase == Dictation::Phase::Transcribing) {
    title = "Transcribing...";
  } else if (phase == Dictation::Phase::Done) {
    title = "Inserted";
  }
  ImGui::TextUnformatted(title);
  ImGui::PopFont();

  ImGui::PushStyleColor(ImGuiCol_Text, color_v4(190, 186, 200));
  ImGui::TextUnformatted(state.settings.hotkey_hold ? "Hold" : "Press");
  ImGui::SameLine();
  ImGui::PopStyleColor();
  ImGui::PushStyleColor(ImGuiCol_Text, p.text);
  keycaps(state.settings.hotkey);
  ImGui::PopStyleColor();
  ImGui::SameLine();
  ImGui::PushStyleColor(ImGuiCol_Text, color_v4(190, 186, 200));
  ImGui::TextUnformatted("in any app, speak, and Lisper types it for you.");
  ImGui::PopStyleColor();

  ImGui::Dummy(ImVec2(0.0f, 6.0f));
  const ImVec2 wave_pos = ImGui::GetCursorScreenPos();
  const float wave_w = ImGui::GetContentRegionAvail().x - 170.0f;
  const float wave_h = 44.0f;
  if (phase == Dictation::Phase::Recording) {
    draw_waveform(dl, wave_pos, wave_w, wave_h, ctx.dictation.capture().recent_levels(48),
                  IM_COL32(255, 255, 255, 230));
  } else {
    draw_idle_wave(dl, wave_pos, wave_w, wave_h, static_cast<float>(ImGui::GetTime()),
                   phase == Dictation::Phase::Transcribing ? u32(p.accent)
                                                           : IM_COL32(255, 255, 255, 70));
  }

  ImGui::SetCursorScreenPos(ImVec2(wave_pos.x + wave_w + 20.0f, wave_pos.y + 2.0f));
  const bool model_ok = ctx.dictation.model_ready() || ctx.dictation.model_loading();
  if (!model_ok) {
    ImGui::BeginDisabled();
  }
  ImGui::PushStyleColor(ImGuiCol_Button, phase == Dictation::Phase::Recording
                                             ? color_v4(239, 68, 68)
                                             : color_v4(255, 255, 255));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, phase == Dictation::Phase::Recording
                                                    ? color_v4(248, 98, 98)
                                                    : color_v4(232, 230, 240));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, color_v4(210, 208, 220));
  ImGui::PushStyleColor(ImGuiCol_Text, phase == Dictation::Phase::Recording
                                           ? color_v4(255, 255, 255)
                                           : color_v4(20, 20, 24));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 999.0f);
  const char *label = phase == Dictation::Phase::Recording ? "Stop" : "Start dictating";
  if (phase == Dictation::Phase::Transcribing) {
    ImGui::BeginDisabled();
  }
  if (ImGui::Button(label, ImVec2(150.0f, 40.0f))) {
    ctx.dictation.toggle();
  }
  if (phase == Dictation::Phase::Transcribing) {
    ImGui::EndDisabled();
  }
  ImGui::PopStyleVar();
  ImGui::PopStyleColor(4);
  if (!model_ok) {
    ImGui::EndDisabled();
  }

  ImGui::SetCursorScreenPos(ImVec2(wave_pos.x, wave_pos.y + wave_h + 10.0f));
  const std::string message = ctx.dictation.message();
  if (!model_ok) {
    ImGui::PushStyleColor(ImGuiCol_Text, color_v4(253, 186, 116));
    ImGui::TextWrapped("%s", ctx.dictation.model_status().c_str());
    ImGui::PopStyleColor();
    ImGui::PushStyleColor(ImGuiCol_Text, p.text);
    if (!state.downloading) {
      if (secondary_button("Download model", ImVec2(0, 0))) {
        ctx.download_model(state.settings.model_profile);
      }
    }
    ImGui::PopStyleColor();
    if (state.downloading) {
      ImGui::TextUnformatted(state.download_status.c_str());
    }
  } else if (phase == Dictation::Phase::Error && !message.empty()) {
    ImGui::PushStyleColor(ImGuiCol_Text, color_v4(252, 165, 165));
    ImGui::TextWrapped("%s", message.c_str());
    ImGui::PopStyleColor();
  } else if (!state.paste_hint.empty() &&
             state.settings.insert_mode == Settings::InsertMode::Paste) {
    ImGui::PushStyleColor(ImGuiCol_Text, color_v4(253, 186, 116));
    ImGui::TextWrapped("%s", state.paste_hint.c_str());
    ImGui::PopStyleColor();
  } else if (!state.hotkey_registered && !state.hotkey_error.empty()) {
    ImGui::PushStyleColor(ImGuiCol_Text, color_v4(253, 186, 116));
    ImGui::TextWrapped("Shortcut unavailable: %s", state.hotkey_error.c_str());
    ImGui::PopStyleColor();
  } else {
    ImGui::PushStyleColor(ImGuiCol_Text, color_v4(150, 146, 160));
    ImGui::TextUnformatted("Private by design: speech is transcribed on this device.");
    ImGui::PopStyleColor();
  }

  ImGui::EndChild();
  ImGui::PopStyleVar();
  ImGui::PopStyleColor(3);
}

void render_stats(UiContext &ctx) {
  const auto &st = ctx.state.stats;
  const float spacing = ImGui::GetStyle().ItemSpacing.x;
  const float w = (page_width() - spacing * 3.0f) / 4.0f;

  // Assume ~40 wpm typing speed for the time-saved estimate.
  const double typing_minutes = static_cast<double>(st.total_words) / 40.0;
  const double speaking_minutes = static_cast<double>(st.total_audio_ms) / 60000.0;
  const int saved = static_cast<int>(std::max(0.0, typing_minutes - speaking_minutes));
  const std::string saved_text =
      saved >= 60 ? std::to_string(saved / 60) + "h " + std::to_string(saved % 60) + "m"
                  : std::to_string(saved) + " min";

  const std::string words = format_number(st.total_words);
  const std::string wpm = std::to_string(static_cast<int>(std::lround(st.words_per_minute)));
  const std::string streak =
      std::to_string(st.day_streak) + (st.day_streak == 1 ? " day" : " days");

  stat_card(ctx.fonts, "stat_words", words.c_str(), "Words dictated", Icon::Mic, w);
  ImGui::SameLine();
  stat_card(ctx.fonts, "stat_wpm", wpm.c_str(), "Words per minute", Icon::Bolt, w);
  ImGui::SameLine();
  stat_card(ctx.fonts, "stat_streak", streak.c_str(), "Daily streak", Icon::Check, w);
  ImGui::SameLine();
  stat_card(ctx.fonts, "stat_saved", saved_text.c_str(), "Time saved", Icon::Home, w);
}

void render_history(UiContext &ctx) {
  AppState &state = ctx.state;
  const Palette &p = palette();

  heading(ctx.fonts, "Recent dictations");
  const float search_w = 220.0f;
  ImGui::SameLine(page_width() - search_w); // offset is relative to the group start
  ImGui::SetNextItemWidth(search_w);
  ImGui::InputTextWithHint("##history_search", "Search", state.history_search.data(),
                           state.history_search.size());

  if (!state.settings.save_history) {
    muted("History is turned off in Settings > Privacy.");
    return;
  }
  if (state.history.empty()) {
    if (begin_card("history_empty", ImVec2(page_width(), 0.0f))) {
      muted("Your dictations will appear here. Try it now: put your cursor in any text field, "
            "press the shortcut, and start talking.");
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
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
      }
      current_day = day;
      section_label(ctx.fonts, day.c_str());
      begin_card(("day_" + day).c_str(), ImVec2(page_width(), 0.0f), true, 6.0f);
    }

    ImGui::PushID(entry.id.c_str());
    const ImVec2 row_start = ImGui::GetCursorScreenPos();
    const float row_w = ImGui::GetContentRegionAvail().x;
    ImDrawList *dl = ImGui::GetWindowDrawList();
    dl->ChannelsSplit(2);
    dl->ChannelsSetCurrent(1);

    ImGui::SetCursorScreenPos(ImVec2(row_start.x + 12.0f, row_start.y + 10.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, p.text_faint);
    ImGui::TextUnformatted(time_label(entry.timestamp_ms).c_str());
    ImGui::PopStyleColor();
    ImGui::SameLine(80.0f);
    const float text_w = row_w - 80.0f - 84.0f;
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + text_w);
    ImGui::TextUnformatted(entry.text.c_str());
    ImGui::PopTextWrapPos();
    const float row_end_y = ImGui::GetCursorScreenPos().y + 4.0f;
    const ImVec2 row_end(row_start.x + row_w, row_end_y);
    const bool hovered = ImGui::IsMouseHoveringRect(row_start, row_end);

    if (hovered) {
      ImGui::SetCursorScreenPos(ImVec2(row_start.x + row_w - 80.0f, row_start.y + 4.0f));
      if (ghost_icon_button("copy", Icon::Copy, "Copy")) {
        ctx.copy_text(entry.text);
        show_toast(state, "Copied to clipboard");
      }
      ImGui::SameLine(0.0f, 4.0f);
      if (ghost_icon_button("delete", Icon::Trash, "Delete")) {
        pending_delete = entry.id;
      }
    }
    dl->ChannelsSetCurrent(0);
    if (hovered) {
      dl->AddRectFilled(row_start, row_end, u32(p.card_hover), 8.0f);
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
    muted("No dictations match your search.");
  }
  if (!pending_delete.empty()) {
    history::remove(pending_delete);
    state.history_dirty = true;
  }
}

void render_home(UiContext &ctx) {
  begin_page("page_home");
  page_header(ctx.fonts, "Welcome back",
              "Speak naturally. Lisper writes it down, cleans it up and "
              "types it where your cursor is.");
  render_dictation_card(ctx);
  ImGui::Dummy(ImVec2(0.0f, 8.0f));
  render_stats(ctx);
  ImGui::Dummy(ImVec2(0.0f, 12.0f));
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
  begin_page("page_transcribe");
  page_header(ctx.fonts, "Transcribe files",
              "Turn recordings, meetings and videos into text, subtitles or RAG-ready chunks.");

  const float width = page_width();
  if (begin_card("drop_zone", ImVec2(width, 0.0f))) {
    const std::string input = trim_copy(state.input_path.data());
    ImDrawList *dl = ImGui::GetWindowDrawList();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float zone_w = ImGui::GetContentRegionAvail().x;
    const float zone_h = 110.0f;
    dl->AddRectFilled(pos, ImVec2(pos.x + zone_w, pos.y + zone_h), u32(p.input), 12.0f);
    // dashed outline
    const float dash = 8.0f;
    for (float x = pos.x + 12.0f; x < pos.x + zone_w - 12.0f; x += dash * 2) {
      dl->AddLine(ImVec2(x, pos.y), ImVec2(std::min(x + dash, pos.x + zone_w - 12.0f), pos.y),
                  u32(p.border), 1.5f);
      dl->AddLine(ImVec2(x, pos.y + zone_h),
                  ImVec2(std::min(x + dash, pos.x + zone_w - 12.0f), pos.y + zone_h), u32(p.border),
                  1.5f);
    }
    draw_icon(dl, Icon::File, ImVec2(pos.x + zone_w * 0.5f, pos.y + 34.0f), 22.0f,
              u32(p.text_muted));
    const std::string headline_s = input.empty() ? std::string("Drop an audio or video file here")
                                                 : filename_for_display(input);
    const ImVec2 ts = ImGui::CalcTextSize(headline_s.c_str());
    dl->AddText(ImVec2(pos.x + (zone_w - ts.x) * 0.5f, pos.y + 56.0f), u32(p.text),
                headline_s.c_str());
    const char *sub = "WAV, MP3, M4A, FLAC, OGG, MP4, MOV, MKV, WEBM and more";
    const ImVec2 ss = ImGui::CalcTextSize(sub);
    dl->AddText(ImVec2(pos.x + (zone_w - ss.x) * 0.5f, pos.y + 80.0f), u32(p.text_faint), sub);
    if (ImGui::InvisibleButton("drop_click", ImVec2(zone_w, zone_h))) {
      if (const auto selected = pick_input_media_file(input)) {
        copy_string(state.input_path, *selected);
      }
    }
    ImGui::Spacing();
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##input_path", "...or paste a file path", state.input_path.data(),
                             state.input_path.size());
    static const char *kSamples[] = {"third_party/whisper.cpp/samples/jfk.wav",
                                     "../third_party/whisper.cpp/samples/jfk.wav"};
    for (const char *sample : kSamples) {
      if (path_exists(sample)) {
        if (secondary_button("Use sample clip")) {
          copy_string(state.input_path, fs::absolute(sample).string());
        }
        break;
      }
    }
  }
  end_card();

  ImGui::Dummy(ImVec2(0.0f, 4.0f));
  if (begin_card("transcribe_options", ImVec2(width, 0.0f))) {
    section_label(ctx.fonts, "Output format");
    segmented("format", &state.format_index, {"Text", "Subtitles (SRT)", "JSON", "RAG chunks"});
    ImGui::Spacing();
    section_label(ctx.fonts, "Save to");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 220.0f);
    ImGui::InputTextWithHint("##output_path", "Preview only (leave empty), or a folder / file path",
                             state.output_path.data(), state.output_path.size());
    ImGui::SameLine();
    if (secondary_button("Folder...", ImVec2(100.0f, 0))) {
      if (const auto folder = pick_output_folder(trim_copy(state.output_path.data()))) {
        copy_string(state.output_path, *folder);
      }
    }
    ImGui::SameLine();
    if (secondary_button("File...", ImVec2(100.0f, 0))) {
      const std::string preview = computed_output_preview(state);
      if (const auto file =
              pick_output_file(preview.empty() ? trim_copy(state.output_path.data()) : preview)) {
        copy_string(state.output_path, *file);
      }
    }
    const std::string preview = computed_output_preview(state);
    if (!preview.empty()) {
      caption(ctx.fonts, ("Will write " + preview).c_str());
    }

    ImGui::Spacing();
    // Validate against the model chosen in Settings (shared with dictation).
    state.resolved_model_path = state.settings.model_path.empty()
                                    ? resolve_model_path("", state.settings.model_profile)
                                    : state.settings.model_path;
    const auto issues = validation_issues(state);
    const bool can_start = !job.running() && issues.empty();
    if (job.running()) {
      const ImVec2 wpos = ImGui::GetCursorScreenPos();
      draw_idle_wave(ImGui::GetWindowDrawList(), wpos, 180.0f, 36.0f,
                     static_cast<float>(ImGui::GetTime()) * 2.0f, u32(p.accent));
      ImGui::Dummy(ImVec2(180.0f, 36.0f));
      ImGui::SameLine();
      ImGui::TextUnformatted(job.status().c_str());
      ImGui::SameLine();
      ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - 120.0f);
      if (danger_button("Cancel", ImVec2(120.0f, 0))) {
        job.cancel();
      }
    } else {
      if (!can_start) {
        ImGui::BeginDisabled();
      }
      if (primary_button("Transcribe", ImVec2(160.0f, 40.0f))) {
        auto request = build_request(state);
        request.shared_engine = ctx.dictation.engine();
        state.status_line = "Transcribing " + filename_for_display(request.input_path) + "...";
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

  if (!state.preview_text.empty() || !state.status_line.empty()) {
    ImGui::Dummy(ImVec2(0.0f, 4.0f));
    if (begin_card("transcript_result", ImVec2(width, 0.0f))) {
      heading(ctx.fonts, "Result");
      ImGui::PushStyleColor(ImGuiCol_Text, state.last_run_success ? p.success : p.text_muted);
      ImGui::TextWrapped("%s", state.status_line.c_str());
      ImGui::PopStyleColor();
      if (!state.preview_text.empty()) {
        if (state.duration_ms > 0) {
          caption(ctx.fonts, (format_duration_ms(state.duration_ms) + " of audio  ·  " +
                              std::to_string(state.segment_count) + " segments  ·  language " +
                              (state.detected_language.empty() ? "?" : state.detected_language))
                                 .c_str());
        }
        if (secondary_button("Copy transcript")) {
          ctx.copy_text(state.preview_text);
          show_toast(state, "Transcript copied");
        }
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_ChildBg, p.input);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14, 12));
        ImGui::BeginChild("transcript_text", ImVec2(0, 320.0f),
                          ImGuiChildFlags_AlwaysUseWindowPadding);
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(state.preview_text.c_str());
        ImGui::PopTextWrapPos();
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
      }
    }
    end_card();
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
  begin_page("page_dictionary");
  page_header(ctx.fonts, "Dictionary",
              "Teach Lisper names, jargon and spellings. Replacements fix words it mishears; "
              "vocabulary nudges the model toward the right spelling in the first place.");

  const float width = page_width();
  if (begin_card("dict_add", ImVec2(width, 0.0f))) {
    section_label(ctx.fonts, "Replacements");
    const float field_w = (ImGui::GetContentRegionAvail().x - 150.0f) * 0.5f;
    ImGui::SetNextItemWidth(field_w);
    ImGui::InputTextWithHint("##dict_from", "When I say... (e.g. lisa per)", state.dict_from.data(),
                             state.dict_from.size());
    ImGui::SameLine();
    ImGui::TextUnformatted("->");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(field_w);
    const bool enter =
        ImGui::InputTextWithHint("##dict_to", "Write (e.g. Lisper)", state.dict_to.data(),
                                 state.dict_to.size(), ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    const std::string from = trim_copy(state.dict_from.data());
    const std::string to = trim_copy(state.dict_to.data());
    if ((primary_button("Add", ImVec2(-1.0f, 0)) || enter) && !from.empty() && !to.empty()) {
      s.dictionary.push_back({from, to});
      copy_string(state.dict_from, "");
      copy_string(state.dict_to, "");
      mark_dirty(state);
    }

    ImGui::Spacing();
    if (s.dictionary.empty()) {
      caption(ctx.fonts, "No replacements yet.");
    }
    int remove_index = -1;
    for (size_t i = 0; i < s.dictionary.size(); ++i) {
      ImGui::PushID(static_cast<int>(i));
      ImGui::Separator();
      ImGui::AlignTextToFramePadding();
      ImGui::PushStyleColor(ImGuiCol_Text, p.text_muted);
      ImGui::TextUnformatted(s.dictionary[i].from.c_str());
      ImGui::PopStyleColor();
      ImGui::SameLine(field_w + 16.0f);
      ImGui::TextUnformatted(s.dictionary[i].to.c_str());
      ImGui::SameLine(ImGui::GetContentRegionMax().x - ImGui::GetFrameHeight());
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

  ImGui::Dummy(ImVec2(0.0f, 4.0f));
  if (begin_card("vocab", ImVec2(width, 0.0f))) {
    section_label(ctx.fonts, "Vocabulary");
    muted("Words Lisper should expect, such as product names, people and acronyms.");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 90.0f);
    const bool enter =
        ImGui::InputTextWithHint("##vocab", "Add a word", state.vocab_word.data(),
                                 state.vocab_word.size(), ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    const std::string word = trim_copy(state.vocab_word.data());
    if ((primary_button("Add", ImVec2(-1.0f, 0)) || enter) && !word.empty()) {
      s.vocabulary.push_back(word);
      copy_string(state.vocab_word, "");
      mark_dirty(state);
    }
    ImGui::Spacing();
    int remove_index = -1;
    const float right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    for (size_t i = 0; i < s.vocabulary.size(); ++i) {
      ImGui::PushID(static_cast<int>(i));
      const std::string label = s.vocabulary[i] + "  x";
      const float w = ImGui::CalcTextSize(label.c_str()).x + 24.0f;
      if (i > 0 && ImGui::GetItemRectMax().x + 8.0f + w < right) {
        ImGui::SameLine(0.0f, 8.0f);
      }
      ImGui::PushStyleColor(ImGuiCol_Button, p.accent_soft);
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, with_alpha(p.danger, 0.15f));
      ImGui::PushStyleColor(ImGuiCol_Text, p.accent);
      ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 999.0f);
      if (ImGui::Button(label.c_str())) {
        remove_index = static_cast<int>(i);
      }
      ImGui::PopStyleVar();
      ImGui::PopStyleColor(3);
      ImGui::PopID();
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
  begin_page("page_snippets");
  page_header(ctx.fonts, "Snippets",
              "Say a short cue and Lisper types the full text: links, addresses, sign-offs, "
              "boilerplate replies.");

  const float width = page_width();
  if (begin_card("snip_add", ImVec2(width, 0.0f))) {
    section_label(ctx.fonts, "New snippet");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##snip_trigger", "When I say... (e.g. my calendar link)",
                             state.snippet_trigger.data(), state.snippet_trigger.size());
    ImGui::InputTextMultiline("##snip_text", state.snippet_text.data(), state.snippet_text.size(),
                              ImVec2(-1.0f, 90.0f));
    const std::string trigger = trim_copy(state.snippet_trigger.data());
    const std::string text = state.snippet_text.data();
    if (trigger.empty() || text.empty()) {
      ImGui::BeginDisabled();
    }
    if (primary_button("Add snippet")) {
      s.snippets.push_back({trigger, text});
      copy_string(state.snippet_trigger, "");
      copy_string(state.snippet_text, "");
      mark_dirty(state);
    }
    if (trigger.empty() || text.empty()) {
      ImGui::EndDisabled();
    }
  }
  end_card();

  ImGui::Dummy(ImVec2(0.0f, 4.0f));
  int remove_index = -1;
  for (size_t i = 0; i < s.snippets.size(); ++i) {
    ImGui::PushID(static_cast<int>(i));
    if (begin_card("snippet", ImVec2(width, 0.0f), true, 16.0f)) {
      ImGui::PushStyleColor(ImGuiCol_Text, p.accent);
      ImGui::Text("\"%s\"", s.snippets[i].trigger.c_str());
      ImGui::PopStyleColor();
      ImGui::SameLine(ImGui::GetContentRegionMax().x - ImGui::GetFrameHeight());
      if (ghost_icon_button("del", Icon::Trash, "Remove")) {
        remove_index = static_cast<int>(i);
      }
      muted(s.snippets[i].text.c_str());
    }
    end_card();
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
  ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, selected ? 2.0f : 1.0f);
  if (begin_card(id, ImVec2(width, 0.0f), true, 20.0f, selected ? p.accent : p.border)) {
    heading(ctx.fonts, title);
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, p.input);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 10));
    ImGui::BeginChild("sample", ImVec2(0, 0),
                      ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(sample);
    ImGui::PopTextWrapPos();
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    const ImVec2 min = ImGui::GetWindowPos();
    const ImVec2 max(min.x + ImGui::GetWindowWidth(), min.y + ImGui::GetWindowHeight());
    if (ImGui::IsMouseHoveringRect(min, max) && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
      s.style = value;
      mark_dirty(ctx.state);
    }
  }
  end_card();
  ImGui::PopStyleVar();
}

void render_style(UiContext &ctx) {
  AppState &state = ctx.state;
  Settings &s = state.settings;
  begin_page("page_style");
  page_header(ctx.fonts, "Style", "Choose how your dictation reads once it lands on the page.");

  const float w = (page_width() - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
  style_choice(ctx, "style_formal", "Formal",
               "Hey Sam, thanks for the update. Let's ship it on Friday.",
               text_cleanup::Style::Formal, w);
  ImGui::SameLine();
  style_choice(ctx, "style_casual", "Casual",
               "hey Sam, thanks for the update. let's ship it on Friday",
               text_cleanup::Style::Casual, w);

  ImGui::Dummy(ImVec2(0.0f, 4.0f));
  if (begin_card("cleanup", ImVec2(page_width(), 0.0f))) {
    section_label(ctx.fonts, "Cleanup");
    if (setting_row_toggle("Remove filler words", "Drop \"um\", \"uh\", \"erm\" and similar.",
                           &s.remove_fillers)) {
      mark_dirty(state);
    }
    ImGui::Separator();
    ImGui::Spacing();
    if (setting_row_toggle("Voice commands",
                           "Say \"new line\" or \"new paragraph\" to format as you speak.",
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
  begin_page("page_settings");
  page_header(ctx.fonts, "Settings", nullptr);
  const float width = page_width();

  // Shortcut
  if (begin_card("set_shortcut", ImVec2(width, 0.0f))) {
    section_label(ctx.fonts, "Dictation shortcut");
    if (state.capturing_hotkey) {
      ImGui::PushStyleColor(ImGuiCol_Text, p.accent);
      ImGui::TextUnformatted("Press the new shortcut now... (Esc to cancel)");
      ImGui::PopStyleColor();
    } else {
      keycaps(s.hotkey);
      ImGui::SameLine(0.0f, 16.0f);
      if (secondary_button("Change")) {
        state.capturing_hotkey = true;
      }
    }
    if (!state.hotkey_registered && !state.hotkey_error.empty()) {
      ImGui::PushStyleColor(ImGuiCol_Text, p.warning);
      ImGui::TextWrapped("%s", state.hotkey_error.c_str());
      ImGui::PopStyleColor();
    }
    ImGui::Spacing();
    int mode = s.hotkey_hold ? 1 : 0;
    if (segmented("hotkey_mode", &mode, {"Press to start / stop", "Hold to talk"}, 360.0f)) {
      s.hotkey_hold = mode == 1;
      mark_dirty(state);
    }
#if !defined(_WIN32) && !defined(__APPLE__)
    caption(ctx.fonts, "Wayland: bind a system shortcut to `lisper_gui --toggle` if the "
                       "built-in shortcut is unavailable.");
#endif
  }
  end_card();
  ImGui::Dummy(ImVec2(0.0f, 4.0f));

  // Microphone & insertion
  if (begin_card("set_input", ImVec2(width, 0.0f))) {
    section_label(ctx.fonts, "Microphone");
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
    ImGui::Spacing();
    section_label(ctx.fonts, "Insert text");
    int mode = s.insert_mode == Settings::InsertMode::Paste ? 0 : 1;
    if (segmented("insert_mode", &mode, {"Type into the active app", "Copy to clipboard only"},
                  420.0f)) {
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
    ImGui::Spacing();
    if (setting_row_toggle("Restore clipboard",
                           "Put back what was on your clipboard after pasting.",
                           &s.restore_clipboard)) {
      mark_dirty(state);
    }
    ImGui::Separator();
    ImGui::Spacing();
    if (setting_row_toggle("Show floating bar", "A small waveform pill appears while you dictate.",
                           &s.show_overlay)) {
      mark_dirty(state);
    }
    ImGui::Separator();
    ImGui::Spacing();
    if (setting_row_toggle("Sounds", "Play a soft chime when dictation starts and stops.",
                           &s.sounds)) {
      mark_dirty(state);
    }
  }
  end_card();
  ImGui::Dummy(ImVec2(0.0f, 4.0f));

  // Model & language
  if (begin_card("set_model", ImVec2(width, 0.0f))) {
    section_label(ctx.fonts, "Speech model");
    const auto &all = all_model_profiles();
    for (const auto &profile : all) {
      ImGui::PushID(profile.name.c_str());
      const bool selected = s.model_profile == profile.name && s.model_path.empty();
      const bool installed = !resolve_model_path("", profile.name).empty();
      const ImVec2 row = ImGui::GetCursorScreenPos();
      const float row_w = ImGui::GetContentRegionAvail().x;
      if (ImGui::InvisibleButton("row", ImVec2(row_w, 46.0f)) && installed) {
        s.model_profile = profile.name;
        s.model_path.clear();
        mark_dirty(state);
      }
      const bool hovered = ImGui::IsItemHovered();
      ImDrawList *dl = ImGui::GetWindowDrawList();
      if (selected || hovered) {
        dl->AddRectFilled(row, ImVec2(row.x + row_w, row.y + 46.0f),
                          u32(selected ? p.accent_soft : p.card_hover), 10.0f);
      }
      dl->AddCircle(ImVec2(row.x + 18.0f, row.y + 23.0f), 7.0f, u32(selected ? p.accent : p.border),
                    16, 2.0f);
      if (selected) {
        dl->AddCircleFilled(ImVec2(row.x + 18.0f, row.y + 23.0f), 3.5f, u32(p.accent), 12);
      }
      std::string name = profile.name;
      name[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
      dl->AddText(ImVec2(row.x + 36.0f, row.y + 5.0f), u32(p.text), name.c_str());
      dl->AddText(ctx.fonts.small, ctx.fonts.small_size, ImVec2(row.x + 36.0f, row.y + 25.0f),
                  u32(p.text_muted), profile.description.c_str());
      ImGui::SetCursorScreenPos(ImVec2(row.x + row_w - 110.0f, row.y + 8.0f));
      if (installed) {
        ImGui::SetCursorScreenPos(ImVec2(row.x + row_w - 90.0f, row.y + 12.0f));
        pill("Installed", p.success, with_alpha(p.success, 0.12f));
      } else if (!state.downloading) {
        if (secondary_button("Download", ImVec2(100.0f, 0))) {
          ctx.download_model(profile.name);
        }
      }
      ImGui::SetCursorScreenPos(ImVec2(row.x, row.y + 50.0f));
      ImGui::Dummy(ImVec2(row_w, 0.0f));
      ImGui::PopID();
    }
    if (state.downloading || !state.download_status.empty()) {
      caption(ctx.fonts, state.download_status.c_str());
    }
    caption(ctx.fonts, ("Models folder: " + paths::models_dir()).c_str());
    caption(ctx.fonts, ("Active: " + ctx.dictation.model_status()).c_str());

    ImGui::Spacing();
    section_label(ctx.fonts, "Language");
    const char *current_lang = s.language.c_str();
    for (const auto &l : kLanguages) {
      if (s.language == l[0]) {
        current_lang = l[1];
      }
    }
    ImGui::SetNextItemWidth(280.0f);
    if (ImGui::BeginCombo("##language", current_lang)) {
      for (const auto &l : kLanguages) {
        if (ImGui::Selectable(l[1], s.language == l[0])) {
          s.language = l[0];
          mark_dirty(state);
        }
      }
      ImGui::EndCombo();
    }
    ImGui::Spacing();
    if (setting_row_toggle("Translate to English", "Speak any language, get English text.",
                           &s.translate)) {
      mark_dirty(state);
    }
    ImGui::Separator();
    ImGui::Spacing();
    section_label(ctx.fonts, "Compute");
    int device = static_cast<int>(s.device);
    if (segmented("device", &device, {"Auto", "CPU", "GPU"}, 300.0f)) {
      s.device = static_cast<LisperConfig::Device>(device);
      mark_dirty(state);
    }
  }
  end_card();
  ImGui::Dummy(ImVec2(0.0f, 4.0f));

  // Appearance & behaviour
  if (begin_card("set_app", ImVec2(width, 0.0f))) {
    section_label(ctx.fonts, "Appearance");
    int theme = s.dark_theme ? 1 : 0;
    if (segmented("theme", &theme, {"Light", "Dark"}, 240.0f)) {
      s.dark_theme = theme == 1;
      apply_theme(s.dark_theme);
      mark_dirty(state);
    }
    ImGui::Spacing();
    if (setting_row_toggle("Keep running when closed",
                           "Closing the window minimizes Lisper so the shortcut keeps working.",
                           &s.close_to_background)) {
      mark_dirty(state);
    }
  }
  end_card();
  ImGui::Dummy(ImVec2(0.0f, 4.0f));

  // Privacy
  if (begin_card("set_privacy", ImVec2(width, 0.0f))) {
    section_label(ctx.fonts, "Privacy");
    muted("Audio is processed on this device and never uploaded. Recordings are not stored.");
    ImGui::Spacing();
    if (setting_row_toggle("Save dictation history",
                           "Keep a local, searchable list of what you "
                           "dictated, used for stats on Home.",
                           &s.save_history)) {
      mark_dirty(state);
    }
    if (danger_button("Clear history")) {
      history::clear();
      state.history_dirty = true;
      show_toast(state, "History cleared");
    }
    caption(ctx.fonts, ("Data folder: " + paths::data_dir()).c_str());
  }
  end_card();
  ImGui::Dummy(ImVec2(0.0f, 4.0f));

  // MCP
  if (begin_card("set_mcp", ImVec2(width, 0.0f))) {
    section_label(ctx.fonts, "Connect AI assistants (MCP)");
    muted("Let Claude, Cursor or any MCP client transcribe files and take dictation through "
          "Lisper, fully on-device. Add this to your client's MCP configuration:");
    const std::string config =
        "{\n  \"mcpServers\": {\n    \"lisper\": {\n      \"command\": \"lisper-mcp\",\n"
        "      \"args\": [\"--allow-microphone\"]\n    }\n  }\n}";
    ImGui::PushStyleColor(ImGuiCol_ChildBg, p.input);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14, 12));
    ImGui::BeginChild("mcp_config", ImVec2(0, 0),
                      ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::TextUnformatted(config.c_str());
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    if (secondary_button("Copy config")) {
      ctx.copy_text(config);
      show_toast(state, "MCP config copied");
    }
    ImGui::SameLine();
    if (secondary_button("Copy Claude Code command")) {
      ctx.copy_text("claude mcp add lisper -- lisper-mcp --allow-microphone");
      show_toast(state, "Command copied");
    }
  }
  end_card();
  ImGui::Dummy(ImVec2(0.0f, 8.0f));
  caption(ctx.fonts, "Lisper " LISPER_VERSION " · whisper.cpp · MIT licensed");
  if (secondary_button("Quit Lisper")) {
    state.want_quit = true;
  }
  end_page();
}

void render_toast(AppState &state) {
  const double now = static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                             std::chrono::steady_clock::now().time_since_epoch())
                                             .count()) /
                     1000.0;
  if (state.toast.empty() || now > state.toast_until) {
    return;
  }
  const Palette &p = palette();
  const ImGuiViewport *vp = ImGui::GetMainViewport();
  const ImVec2 ts = ImGui::CalcTextSize(state.toast.c_str());
  const ImVec2 size(ts.x + 36.0f, ts.y + 20.0f);
  const ImVec2 pos(vp->Pos.x + kSidebarWidth + (vp->Size.x - kSidebarWidth - size.x) * 0.5f,
                   vp->Pos.y + vp->Size.y - size.y - 28.0f);
  ImDrawList *dl = ImGui::GetForegroundDrawList();
  dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), u32(p.primary), 999.0f);
  dl->AddText(ImVec2(pos.x + 18.0f, pos.y + 10.0f), u32(p.primary_text), state.toast.c_str());
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

  render_toast(ctx.state);
  ImGui::End();
}

} // namespace gui
