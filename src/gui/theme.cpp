#include "gui/theme.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <filesystem>

namespace fs = std::filesystem;

namespace gui {

namespace {

const char *
first_existing_font_path(std::initializer_list<const char *> paths) {
  for (const char *path : paths) {
    std::error_code ec;
    if (path != nullptr && fs::exists(path, ec) && !ec) {
      return path;
    }
  }
  return nullptr;
}

ImFont *load_font(ImGuiIO &io, float size_pixels,
                  std::initializer_list<const char *> paths) {
  const char *path = first_existing_font_path(paths);
  if (path == nullptr) {
    return nullptr;
  }

  ImFontConfig config;
  config.OversampleH = 2;
  config.OversampleV = 2;
  config.PixelSnapH = false;
  return io.Fonts->AddFontFromFileTTF(path, size_pixels, &config);
}

} // namespace

ImU32 color_u32(int r, int g, int b, int a) { return IM_COL32(r, g, b, a); }

ImVec4 color_v4(int r, int g, int b, int a) {
  return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
}

ImVec4 gradient_color(float t) {
  static const std::array<ImVec4, 4> palette = {
      color_v4(47, 129, 247), color_v4(68, 147, 248), color_v4(121, 192, 255),
      color_v4(63, 185, 80)};

  const float clamped = std::clamp(t, 0.0f, 1.0f);
  const float scaled = clamped * static_cast<float>(palette.size() - 1);
  const int index = static_cast<int>(scaled);
  const int next = std::min(index + 1, static_cast<int>(palette.size() - 1));
  const float blend = scaled - static_cast<float>(index);

  const auto mix = [blend](float a, float b) { return a + (b - a) * blend; };
  return ImVec4(mix(palette[index].x, palette[next].x),
                mix(palette[index].y, palette[next].y),
                mix(palette[index].z, palette[next].z), 1.0f);
}

Fonts install_fonts(float content_scale) {
  ImGuiIO &io = ImGui::GetIO();
  io.Fonts->Clear();

  Fonts fonts;
  const float scale = std::max(1.0f, content_scale);
  const auto ui_paths = {
      "/System/Library/Fonts/SFNS.ttf",
      "/System/Library/Fonts/HelveticaNeue.ttc",
      "/System/Library/Fonts/Helvetica.ttc",
      "C:/Windows/Fonts/segoeui.ttf",
      "C:/Windows/Fonts/arial.ttf",
      "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
      "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
  };

  fonts.body = load_font(io, 17.0f * scale, ui_paths);
  fonts.heading = load_font(io, 23.0f * scale, ui_paths);
  fonts.hero = load_font(io, 34.0f * scale, ui_paths);

  if (fonts.body == nullptr) {
    ImFontConfig config;
    config.SizePixels = 17.0f * scale;
    fonts.body = io.Fonts->AddFontDefault(&config);
  }
  if (fonts.heading == nullptr) {
    ImFontConfig config;
    config.SizePixels = 23.0f * scale;
    fonts.heading = io.Fonts->AddFontDefault(&config);
  }
  if (fonts.hero == nullptr) {
    ImFontConfig config;
    config.SizePixels = 34.0f * scale;
    fonts.hero = io.Fonts->AddFontDefault(&config);
  }

  return fonts;
}

void apply_style() {
  ImGuiStyle &style = ImGui::GetStyle();
  style.Alpha = 1.0f;
  style.DisabledAlpha = 0.55f;
  style.WindowPadding = ImVec2(20.0f, 20.0f);
  style.FramePadding = ImVec2(12.0f, 8.0f);
  style.ItemSpacing = ImVec2(12.0f, 10.0f);
  style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
  style.CellPadding = ImVec2(8.0f, 6.0f);
  style.ScrollbarSize = 12.0f;
  style.ScrollbarRounding = 999.0f;
  style.WindowRounding = 0.0f;
  style.ChildRounding = 12.0f;
  style.FrameRounding = 8.0f;
  style.GrabRounding = 999.0f;
  style.TabRounding = 8.0f;
  style.PopupRounding = 12.0f;
  style.WindowBorderSize = 0.0f;
  style.ChildBorderSize = 1.0f;
  style.FrameBorderSize = 1.0f;
  style.TabBorderSize = 1.0f;
  style.PopupBorderSize = 1.0f;
  style.IndentSpacing = 18.0f;

  ImVec4 *colors = style.Colors;
  // Text colors - improved contrast for WCAG AA compliance
  colors[ImGuiCol_Text] =
      color_v4(235, 241, 246); // Slightly brighter for better contrast
  colors[ImGuiCol_TextDisabled] =
      color_v4(160, 168, 177); // Increased brightness for better readability

  // Background colors - maintaining dark theme
  colors[ImGuiCol_WindowBg] = color_v4(0, 0, 0, 0);
  colors[ImGuiCol_ChildBg] = color_v4(13, 17, 23, 246);
  colors[ImGuiCol_PopupBg] = color_v4(13, 17, 23, 252);

  // Borders - slightly brighter for visibility
  colors[ImGuiCol_Border] = color_v4(52, 59, 67); // Increased brightness
  colors[ImGuiCol_BorderShadow] = color_v4(0, 0, 0, 0);

  // Frame backgrounds - improved hover states
  colors[ImGuiCol_FrameBg] = color_v4(22, 27, 34);
  colors[ImGuiCol_FrameBgHovered] = color_v4(34, 42, 53); // More distinct hover
  colors[ImGuiCol_FrameBgActive] = color_v4(38, 48, 61); // More distinct active

  // Buttons - improved contrast and hover effects
  colors[ImGuiCol_Button] = color_v4(31, 111, 235);
  colors[ImGuiCol_ButtonHovered] = color_v4(61, 144, 255); // Brighter hover
  colors[ImGuiCol_ButtonActive] =
      color_v4(23, 89, 198); // Darker active for feedback

  // Headers - better distinction
  colors[ImGuiCol_Header] = color_v4(31, 38, 48);
  colors[ImGuiCol_HeaderHovered] = color_v4(43, 54, 68); // More visible hover
  colors[ImGuiCol_HeaderActive] = color_v4(48, 62, 79);  // Clear active state

  // Separators - improved visibility
  colors[ImGuiCol_Separator] = color_v4(52, 59, 67); // Slightly brighter
  colors[ImGuiCol_SeparatorHovered] = color_v4(105, 117, 130);
  colors[ImGuiCol_SeparatorActive] = color_v4(130, 141, 152);

  // Interactive elements - better feedback
  colors[ImGuiCol_SliderGrab] = color_v4(68, 147, 248);
  colors[ImGuiCol_SliderGrabActive] =
      color_v4(130, 200, 255);                         // Brighter when active
  colors[ImGuiCol_CheckMark] = color_v4(75, 155, 255); // Slightly brighter

  // Tabs - improved distinction
  colors[ImGuiCol_Tab] = color_v4(22, 27, 34);
  colors[ImGuiCol_TabHovered] = color_v4(34, 42, 53);
  colors[ImGuiCol_TabActive] = color_v4(16, 21, 28); // Darker for contrast
  colors[ImGuiCol_TabUnfocused] = color_v4(22, 27, 34, 235);
  colors[ImGuiCol_TabUnfocusedActive] = color_v4(20, 25, 31, 245);

  // Title bars
  colors[ImGuiCol_TitleBg] = color_v4(13, 17, 23);
  colors[ImGuiCol_TitleBgActive] =
      color_v4(16, 21, 28); // Slightly different when active

  // Scrollbars - improved visibility
  colors[ImGuiCol_ScrollbarBg] = color_v4(1, 4, 9, 0);
  colors[ImGuiCol_ScrollbarGrab] = color_v4(82, 89, 97, 190); // More visible
  colors[ImGuiCol_ScrollbarGrabHovered] = color_v4(120, 128, 139, 210);
  colors[ImGuiCol_ScrollbarGrabActive] = color_v4(155, 162, 171, 230);

  // Resize grips - better visibility
  colors[ImGuiCol_ResizeGrip] = color_v4(68, 147, 248, 80);
  colors[ImGuiCol_ResizeGripHovered] = color_v4(68, 147, 248, 140);
  colors[ImGuiCol_ResizeGripActive] = color_v4(68, 147, 248, 190);

  // Selection - improved visibility
  colors[ImGuiCol_TextSelectedBg] =
      color_v4(47, 129, 247, 85); // Slightly more opaque
  colors[ImGuiCol_NavHighlight] = color_v4(68, 147, 248, 190);
}

bool begin_surface(const char *id, const ImVec2 &size, bool border,
                   ImGuiWindowFlags flags) {
  ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 12.0f);
  ImGui::PushStyleColor(ImGuiCol_ChildBg, color_v4(13, 17, 23, 246));
  ImGui::PushStyleColor(ImGuiCol_Border, color_v4(48, 54, 61, 255));
  return ImGui::BeginChild(id, size, border, flags);
}

void end_surface() {
  ImGui::EndChild();
  ImGui::PopStyleColor(2);
  ImGui::PopStyleVar();
}

void draw_background(ImDrawList *draw_list, const ImVec2 &pos,
                     const ImVec2 &size, float time_seconds) {
  const ImVec2 max(pos.x + size.x, pos.y + size.y);
  draw_list->AddRectFilledMultiColor(pos, max, color_u32(1, 4, 9),
                                     color_u32(4, 8, 15), color_u32(11, 16, 24),
                                     color_u32(7, 10, 15));
  draw_list->AddRectFilledMultiColor(
      pos, ImVec2(max.x, pos.y + 3.0f), color_u32(31, 111, 235, 255),
      color_u32(68, 147, 248, 255), color_u32(31, 111, 235, 255),
      color_u32(9, 105, 218, 255));

  const float pulse = std::sin(time_seconds * 0.45f) * 0.5f + 0.5f;
  draw_list->AddCircleFilled(ImVec2(pos.x + size.x - 180.0f, pos.y + 88.0f),
                             150.0f + pulse * 14.0f,
                             color_u32(31, 111, 235, 22));
  draw_list->AddCircleFilled(ImVec2(pos.x + 180.0f, pos.y + size.y - 120.0f),
                             190.0f, color_u32(56, 139, 253, 11));

  for (int i = 0; i < 5; ++i) {
    const float y = pos.y + 140.0f + static_cast<float>(i) * 160.0f;
    draw_list->AddLine(ImVec2(pos.x + 28.0f, y), ImVec2(max.x - 28.0f, y),
                       color_u32(255, 255, 255, 8), 1.0f);
  }
}

void draw_gradient_label(ImDrawList *draw_list, ImFont *font, float font_size,
                         ImVec2 position, const std::string &text,
                         float shimmer_offset) {
  if (font == nullptr || text.empty()) {
    return;
  }

  float cursor_x = position.x;
  const float width = std::max(1.0f, static_cast<float>(text.size() - 1));

  for (size_t i = 0; i < text.size(); ++i) {
    const std::string glyph(1, text[i]);
    ImVec4 color = gradient_color(static_cast<float>(i) / width);
    const float distance = std::fabs(static_cast<float>(i) - shimmer_offset);
    if (distance < 1.4f) {
      color.x = std::min(1.0f, color.x + 0.12f);
      color.y = std::min(1.0f, color.y + 0.12f);
      color.z = std::min(1.0f, color.z + 0.12f);
    }

    draw_list->AddText(font, font_size, ImVec2(cursor_x, position.y),
                       ImGui::GetColorU32(color), glyph.c_str());
    cursor_x += font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, glyph.c_str()).x;
  }
}

void draw_processing_wave(ImDrawList *draw_list, const ImVec2 &origin,
                          float width, float height, float time_seconds,
                          bool active) {
  const int bars = 40;
  const float bar_spacing = width / static_cast<float>(bars);
  const float center_y = origin.y + height * 0.5f;

  for (int i = 0; i < bars; ++i) {
    const float x = origin.x + static_cast<float>(i) * bar_spacing;
    const float phase = static_cast<float>(i) * 0.24f + time_seconds * 3.3f;
    float amplitude = std::sin(phase) * 0.5f + 0.5f;
    amplitude *= std::sin((static_cast<float>(i) / static_cast<float>(bars)) *
                          3.1415926f) *
                 0.85f;
    const float bar_height = active ? (6.0f + amplitude * height)
                                    : (4.0f + amplitude * height * 0.16f);
    const ImVec4 color =
        active
            ? gradient_color(static_cast<float>(i) / static_cast<float>(bars))
            : color_v4(110, 118, 129);
    draw_list->AddRectFilled(
        ImVec2(x, center_y - bar_height * 0.5f),
        ImVec2(x + bar_spacing * 0.54f, center_y + bar_height * 0.5f),
        ImGui::GetColorU32(
            ImVec4(color.x, color.y, color.z, active ? 0.95f : 0.42f)),
        999.0f);
  }
}

void draw_pill(const std::string &text, const ImVec4 &text_color,
               const ImVec4 &bg_color) {
  ImDrawList *draw_list = ImGui::GetWindowDrawList();
  const ImVec2 pos = ImGui::GetCursorScreenPos();
  const ImVec2 text_size = ImGui::CalcTextSize(text.c_str());
  const ImVec2 pill_size(text_size.x + 20.0f, text_size.y + 10.0f);

  draw_list->AddRectFilled(pos,
                           ImVec2(pos.x + pill_size.x, pos.y + pill_size.y),
                           ImGui::GetColorU32(bg_color), 999.0f);
  draw_list->AddRect(pos, ImVec2(pos.x + pill_size.x, pos.y + pill_size.y),
                     ImGui::GetColorU32(ImVec4(0.48f, 0.51f, 0.56f, 0.85f)),
                     999.0f, 0, 1.0f);
  draw_list->AddText(ImVec2(pos.x + 10.0f, pos.y + 5.0f),
                     ImGui::GetColorU32(text_color), text.c_str());
  ImGui::Dummy(pill_size);
}

bool choice_button(const char *label, bool selected, const ImVec2 &size) {
  const ImVec4 button = selected ? color_v4(24, 52, 92) : color_v4(22, 27, 34);
  const ImVec4 hover = selected ? color_v4(28, 63, 112) : color_v4(31, 38, 48);
  const ImVec4 active = selected ? color_v4(21, 48, 85) : color_v4(33, 43, 55);
  const ImVec4 text =
      selected ? color_v4(121, 192, 255) : color_v4(230, 237, 243);
  const ImVec4 border =
      selected ? color_v4(47, 129, 247) : color_v4(48, 54, 61);

  ImGui::PushStyleColor(ImGuiCol_Button, button);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hover);
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, active);
  ImGui::PushStyleColor(ImGuiCol_Text, text);
  ImGui::PushStyleColor(ImGuiCol_Border, border);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
  const bool pressed = ImGui::Button(label, size);
  ImGui::PopStyleVar();
  ImGui::PopStyleColor(5);
  return pressed;
}

void section_title(const Fonts &fonts, const std::string &title,
                   const std::string &subtitle) {
  if (fonts.heading != nullptr) {
    ImGui::PushFont(fonts.heading);
  }
  ImGui::TextUnformatted(title.c_str());
  if (fonts.heading != nullptr) {
    ImGui::PopFont();
  }

  if (!subtitle.empty()) {
    ImGui::PushStyleColor(ImGuiCol_Text, color_v4(145, 152, 161));
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(subtitle.c_str());
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
  }
}

void label_text(const char *label) {
  ImGui::PushStyleColor(ImGuiCol_Text, color_v4(174, 183, 194));
  ImGui::TextUnformatted(label);
  ImGui::PopStyleColor();
}

} // namespace gui
