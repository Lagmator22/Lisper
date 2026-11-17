#include "gui/theme.h"

#include "imgui_internal.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;

namespace gui {

namespace {

Palette g_palette;
constexpr float kPi = 3.14159265f;

const char *first_existing_font_path(std::initializer_list<const char *> paths) {
  for (const char *path : paths) {
    std::error_code ec;
    if (path != nullptr && fs::exists(path, ec) && !ec) {
      return path;
    }
  }
  return nullptr;
}

ImFont *load_font(ImGuiIO &io, float size_pixels, std::initializer_list<const char *> paths) {
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

Palette make_light() {
  Palette p;
  p.dark = false;
  p.bg = color_v4(250, 250, 249);
  p.sidebar = color_v4(244, 243, 241);
  p.card = color_v4(255, 255, 255);
  p.card_hover = color_v4(247, 246, 244);
  p.border = color_v4(231, 229, 226);
  p.text = color_v4(28, 25, 23);
  p.text_muted = color_v4(98, 94, 90);
  p.text_faint = color_v4(150, 146, 141);
  p.primary = color_v4(28, 25, 23);
  p.primary_text = color_v4(255, 255, 255);
  p.accent = color_v4(108, 92, 231);
  p.accent_soft = color_v4(240, 237, 255);
  p.success = color_v4(22, 163, 74);
  p.warning = color_v4(202, 138, 4);
  p.danger = color_v4(220, 38, 38);
  p.input = color_v4(245, 244, 242);
  return p;
}

Palette make_dark() {
  Palette p;
  p.dark = true;
  p.bg = color_v4(17, 17, 19);
  p.sidebar = color_v4(12, 12, 14);
  p.card = color_v4(26, 26, 29);
  p.card_hover = color_v4(33, 33, 37);
  p.border = color_v4(44, 44, 49);
  p.text = color_v4(240, 239, 237);
  p.text_muted = color_v4(168, 165, 160);
  p.text_faint = color_v4(118, 115, 111);
  p.primary = color_v4(244, 243, 241);
  p.primary_text = color_v4(20, 20, 22);
  p.accent = color_v4(150, 136, 255);
  p.accent_soft = color_v4(39, 34, 66);
  p.success = color_v4(74, 222, 128);
  p.warning = color_v4(250, 204, 21);
  p.danger = color_v4(248, 113, 113);
  p.input = color_v4(33, 33, 37);
  return p;
}

ImU32 u32(const ImVec4 &c) {
  return ImGui::GetColorU32(c);
}

ImVec4 lerp(const ImVec4 &a, const ImVec4 &b, float t) {
  return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t,
                a.w + (b.w - a.w) * t);
}

// Wrap at the enclosing column if one was set, otherwise at the window edge.
bool push_wrap_if_unset() {
  if (ImGui::GetCurrentWindow()->DC.TextWrapPos < 0.0f) {
    ImGui::PushTextWrapPos(0.0f);
    return true;
  }
  return false;
}

bool styled_button(const char *label, const ImVec2 &size, const ImVec4 &bg, const ImVec4 &fg,
                   const ImVec4 &border) {
  const ImVec4 hover =
      g_palette.dark ? lerp(bg, ImVec4(1, 1, 1, 1), 0.08f) : lerp(bg, ImVec4(0, 0, 0, 1), 0.06f);
  const ImVec4 active =
      g_palette.dark ? lerp(bg, ImVec4(1, 1, 1, 1), 0.14f) : lerp(bg, ImVec4(0, 0, 0, 1), 0.12f);
  ImGui::PushStyleColor(ImGuiCol_Button, bg);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hover);
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, active);
  ImGui::PushStyleColor(ImGuiCol_Text, fg);
  ImGui::PushStyleColor(ImGuiCol_Border, border);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, border.w > 0.0f ? 1.0f : 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16.0f, 9.0f));
  const bool pressed = ImGui::Button(label, size);
  ImGui::PopStyleVar(2);
  ImGui::PopStyleColor(5);
  return pressed;
}

} // namespace

ImU32 color_u32(int r, int g, int b, int a) {
  return IM_COL32(r, g, b, a);
}

ImVec4 color_v4(int r, int g, int b, int a) {
  return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
}

ImVec4 with_alpha(const ImVec4 &c, float a) {
  return ImVec4(c.x, c.y, c.z, a);
}

const Palette &palette() {
  return g_palette;
}

Fonts install_fonts(float content_scale) {
  ImGuiIO &io = ImGui::GetIO();
  io.Fonts->Clear();

  Fonts fonts;
  const float scale = std::max(1.0f, content_scale);
  const auto ui_paths = {
      "/System/Library/Fonts/SFNS.ttf",
      "/System/Library/Fonts/HelveticaNeue.ttc",
      "C:/Windows/Fonts/segoeui.ttf",
      "C:/Windows/Fonts/arial.ttf",
      "/usr/share/fonts/truetype/inter/Inter-Regular.ttf",
      "/usr/share/fonts/opentype/inter/Inter-Regular.otf",
      "/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf",
      "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
      "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
  };
  const auto bold_paths = {
      "/System/Library/Fonts/SFNS.ttf",
      "C:/Windows/Fonts/seguisb.ttf",
      "C:/Windows/Fonts/segoeuib.ttf",
      "/usr/share/fonts/truetype/inter/Inter-SemiBold.ttf",
      "/usr/share/fonts/opentype/inter/Inter-SemiBold.otf",
      "/usr/share/fonts/truetype/ubuntu/Ubuntu-M.ttf",
      "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
      "/usr/share/fonts/truetype/liberation2/LiberationSans-Bold.ttf",
  };

  fonts.body = load_font(io, fonts.body_size * scale, ui_paths);
  fonts.small = load_font(io, fonts.small_size * scale, ui_paths);
  fonts.heading = load_font(io, fonts.heading_size * scale, bold_paths);
  fonts.hero = load_font(io, fonts.hero_size * scale, bold_paths);

  auto fallback = [&](ImFont *&font, float size) {
    if (font == nullptr) {
      ImFontConfig config;
      config.SizePixels = size * scale;
      font = io.Fonts->AddFontDefault(&config);
    }
  };
  fallback(fonts.body, fonts.body_size);
  fallback(fonts.small, fonts.small_size);
  fallback(fonts.heading, fonts.heading_size);
  fallback(fonts.hero, fonts.hero_size);
  io.FontDefault = fonts.body;
  return fonts;
}

void apply_theme(bool dark) {
  g_palette = dark ? make_dark() : make_light();
  const Palette &p = g_palette;

  ImGuiStyle &style = ImGui::GetStyle();
  style.WindowPadding = ImVec2(0.0f, 0.0f);
  style.FramePadding = ImVec2(12.0f, 8.0f);
  style.ItemSpacing = ImVec2(10.0f, 10.0f);
  style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
  style.CellPadding = ImVec2(8.0f, 8.0f);
  style.ScrollbarSize = 10.0f;
  style.ScrollbarRounding = 999.0f;
  style.WindowRounding = 0.0f;
  style.ChildRounding = 14.0f;
  style.FrameRounding = 9.0f;
  style.GrabRounding = 999.0f;
  style.PopupRounding = 12.0f;
  style.TabRounding = 8.0f;
  style.WindowBorderSize = 0.0f;
  style.ChildBorderSize = 1.0f;
  style.FrameBorderSize = 0.0f;
  style.PopupBorderSize = 1.0f;
  style.DisabledAlpha = 0.45f;
  style.SelectableTextAlign = ImVec2(0.0f, 0.5f);

  ImVec4 *c = style.Colors;
  c[ImGuiCol_Text] = p.text;
  c[ImGuiCol_TextDisabled] = p.text_faint;
  c[ImGuiCol_WindowBg] = p.bg;
  c[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
  c[ImGuiCol_PopupBg] = p.card;
  c[ImGuiCol_Border] = p.border;
  c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
  c[ImGuiCol_FrameBg] = p.input;
  c[ImGuiCol_FrameBgHovered] = p.card_hover;
  c[ImGuiCol_FrameBgActive] = p.card_hover;
  c[ImGuiCol_Button] = p.input;
  c[ImGuiCol_ButtonHovered] = p.card_hover;
  c[ImGuiCol_ButtonActive] = p.border;
  c[ImGuiCol_Header] = p.accent_soft;
  c[ImGuiCol_HeaderHovered] = p.card_hover;
  c[ImGuiCol_HeaderActive] = p.accent_soft;
  c[ImGuiCol_Separator] = p.border;
  c[ImGuiCol_CheckMark] = p.accent;
  c[ImGuiCol_SliderGrab] = p.accent;
  c[ImGuiCol_SliderGrabActive] = p.accent;
  c[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
  c[ImGuiCol_ScrollbarGrab] = with_alpha(p.text_faint, 0.35f);
  c[ImGuiCol_ScrollbarGrabHovered] = with_alpha(p.text_faint, 0.55f);
  c[ImGuiCol_ScrollbarGrabActive] = with_alpha(p.text_faint, 0.75f);
  c[ImGuiCol_TextSelectedBg] = with_alpha(p.accent, 0.25f);
  c[ImGuiCol_NavCursor] = p.accent;
  c[ImGuiCol_Tab] = p.input;
  c[ImGuiCol_TabHovered] = p.card_hover;
  c[ImGuiCol_TabSelected] = p.card;
  c[ImGuiCol_ModalWindowDimBg] = ImVec4(0, 0, 0, 0.35f);
}

// --- surfaces ---------------------------------------------------------------

bool begin_card(const char *id, const ImVec2 &size, bool auto_height, float padding,
                const ImVec4 &border) {
  ImGui::PushStyleColor(ImGuiCol_ChildBg, g_palette.card);
  ImGui::PushStyleColor(ImGuiCol_Border, border.w > 0.0f ? border : g_palette.border);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(padding, padding));
  ImGuiChildFlags flags = ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding;
  if (auto_height) {
    flags |= ImGuiChildFlags_AutoResizeY;
  }
  return ImGui::BeginChild(id, size, flags, ImGuiWindowFlags_NoScrollbar);
}

void end_card() {
  ImGui::EndChild();
  ImGui::PopStyleVar();
  ImGui::PopStyleColor(2);
}

// --- text -------------------------------------------------------------------

void heading(const Fonts &fonts, const char *text) {
  ImGui::PushFont(fonts.heading);
  ImGui::TextUnformatted(text);
  ImGui::PopFont();
}

void hero(const Fonts &fonts, const char *text) {
  ImGui::PushFont(fonts.hero);
  ImGui::TextUnformatted(text);
  ImGui::PopFont();
}

void muted(const char *text, bool wrap) {
  ImGui::PushStyleColor(ImGuiCol_Text, g_palette.text_muted);
  const bool pushed = wrap && push_wrap_if_unset();
  if (!wrap) {
    ImGui::PushTextWrapPos(-1.0f);
  }
  ImGui::TextUnformatted(text);
  if (pushed || !wrap) {
    ImGui::PopTextWrapPos();
  }
  ImGui::PopStyleColor();
}

void caption(const Fonts &fonts, const char *text) {
  ImGui::PushFont(fonts.small);
  ImGui::PushStyleColor(ImGuiCol_Text, g_palette.text_faint);
  const bool pushed = push_wrap_if_unset();
  ImGui::TextUnformatted(text);
  if (pushed) {
    ImGui::PopTextWrapPos();
  }
  ImGui::PopStyleColor();
  ImGui::PopFont();
}

void section_label(const Fonts &fonts, const char *text) {
  std::string upper(text);
  std::transform(upper.begin(), upper.end(), upper.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
  ImGui::PushFont(fonts.small);
  ImGui::PushStyleColor(ImGuiCol_Text, g_palette.text_faint);
  ImGui::TextUnformatted(upper.c_str());
  ImGui::PopStyleColor();
  ImGui::PopFont();
}

// --- controls ---------------------------------------------------------------

bool primary_button(const char *label, const ImVec2 &size) {
  return styled_button(label, size, g_palette.primary, g_palette.primary_text, ImVec4(0, 0, 0, 0));
}

bool secondary_button(const char *label, const ImVec2 &size) {
  return styled_button(label, size, g_palette.card, g_palette.text, g_palette.border);
}

bool danger_button(const char *label, const ImVec2 &size) {
  return styled_button(label, size, g_palette.card, g_palette.danger, g_palette.border);
}

bool ghost_icon_button(const char *id, Icon icon, const char *tooltip) {
  const float sz = ImGui::GetFrameHeight();
  const ImVec2 pos = ImGui::GetCursorScreenPos();
  const bool pressed = ImGui::InvisibleButton(id, ImVec2(sz, sz));
  const bool hovered = ImGui::IsItemHovered();
  ImDrawList *dl = ImGui::GetWindowDrawList();
  if (hovered) {
    dl->AddRectFilled(pos, ImVec2(pos.x + sz, pos.y + sz), u32(g_palette.card_hover), 8.0f);
    if (tooltip != nullptr) {
      ImGui::SetTooltip("%s", tooltip);
    }
  }
  draw_icon(dl, icon, ImVec2(pos.x + sz * 0.5f, pos.y + sz * 0.5f), sz * 0.48f,
            u32(hovered ? g_palette.text : g_palette.text_muted));
  return pressed;
}

bool toggle_switch(const char *id, bool *value) {
  const float h = 22.0f;
  const float w = 38.0f;
  const ImVec2 pos = ImGui::GetCursorScreenPos();
  const bool pressed = ImGui::InvisibleButton(id, ImVec2(w, h));
  if (pressed) {
    *value = !*value;
  }
  ImGuiStorage *storage = ImGui::GetStateStorage();
  const ImGuiID anim_id = ImGui::GetItemID();
  float t = storage->GetFloat(anim_id, *value ? 1.0f : 0.0f);
  const float target = *value ? 1.0f : 0.0f;
  t += (target - t) * std::min(1.0f, ImGui::GetIO().DeltaTime * 14.0f);
  storage->SetFloat(anim_id, t);

  ImDrawList *dl = ImGui::GetWindowDrawList();
  const ImVec4 off = g_palette.dark ? color_v4(60, 60, 66) : color_v4(214, 211, 207);
  dl->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + h), u32(lerp(off, g_palette.accent, t)),
                    h * 0.5f);
  const float r = h * 0.5f - 3.0f;
  dl->AddCircleFilled(ImVec2(pos.x + h * 0.5f + t * (w - h), pos.y + h * 0.5f), r,
                      IM_COL32(255, 255, 255, 255), 24);
  return pressed;
}

bool segmented(const char *id, int *value, const std::vector<const char *> &options, float width) {
  ImGui::PushID(id);
  const float total = width > 0.0f ? width : ImGui::GetContentRegionAvail().x;
  const float h = ImGui::GetFrameHeight();
  const ImVec2 pos = ImGui::GetCursorScreenPos();
  ImDrawList *dl = ImGui::GetWindowDrawList();
  dl->AddRectFilled(pos, ImVec2(pos.x + total, pos.y + h), u32(g_palette.input), 10.0f);
  const float seg_w = total / static_cast<float>(options.size());
  bool changed = false;
  for (size_t i = 0; i < options.size(); ++i) {
    const ImVec2 a(pos.x + seg_w * static_cast<float>(i), pos.y);
    ImGui::SetCursorScreenPos(a);
    ImGui::PushID(static_cast<int>(i));
    if (ImGui::InvisibleButton("seg", ImVec2(seg_w, h))) {
      *value = static_cast<int>(i);
      changed = true;
    }
    const bool hovered = ImGui::IsItemHovered();
    ImGui::PopID();
    const bool selected = *value == static_cast<int>(i);
    if (selected) {
      dl->AddRectFilled(ImVec2(a.x + 3, a.y + 3), ImVec2(a.x + seg_w - 3, a.y + h - 3),
                        u32(g_palette.card), 8.0f);
      if (!g_palette.dark) {
        dl->AddRect(ImVec2(a.x + 3, a.y + 3), ImVec2(a.x + seg_w - 3, a.y + h - 3),
                    u32(g_palette.border), 8.0f);
      }
    }
    const ImVec2 ts = ImGui::CalcTextSize(options[i]);
    dl->AddText(ImVec2(a.x + (seg_w - ts.x) * 0.5f, a.y + (h - ts.y) * 0.5f),
                u32(selected || hovered ? g_palette.text : g_palette.text_muted), options[i]);
  }
  ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + h));
  ImGui::Dummy(ImVec2(total, 0.0f));
  ImGui::PopID();
  return changed;
}

bool nav_item(const Fonts &fonts, const char *label, Icon icon, bool selected) {
  (void)fonts;
  const float h = 36.0f;
  const float w = ImGui::GetContentRegionAvail().x;
  const ImVec2 pos = ImGui::GetCursorScreenPos();
  const bool pressed = ImGui::InvisibleButton(label, ImVec2(w, h));
  const bool hovered = ImGui::IsItemHovered();
  ImDrawList *dl = ImGui::GetWindowDrawList();
  if (selected || hovered) {
    const ImVec4 bg = selected ? (g_palette.dark ? color_v4(36, 36, 40) : color_v4(255, 255, 255))
                               : with_alpha(g_palette.card_hover, 0.8f);
    dl->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + h), u32(bg), 9.0f);
    if (selected && !g_palette.dark) {
      dl->AddRect(pos, ImVec2(pos.x + w, pos.y + h), u32(g_palette.border), 9.0f);
    }
  }
  const ImU32 col = u32(selected ? g_palette.text : g_palette.text_muted);
  draw_icon(dl, icon, ImVec2(pos.x + 20.0f, pos.y + h * 0.5f), 15.0f, col);
  const ImVec2 ts = ImGui::CalcTextSize(label);
  dl->AddText(ImVec2(pos.x + 38.0f, pos.y + (h - ts.y) * 0.5f), col, label);
  return pressed;
}

void setting_row_begin(const char *title, const char *description) {
  ImGui::BeginGroup();
  ImGui::TextUnformatted(title);
  if (description != nullptr && *description != '\0') {
    ImGui::PushStyleColor(ImGuiCol_Text, g_palette.text_muted);
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x * 0.62f);
    ImGui::TextUnformatted(description);
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
  }
  ImGui::EndGroup();
}

void setting_row_end() {
  ImGui::Spacing();
}

bool setting_row_toggle(const char *title, const char *description, bool *value) {
  const float right = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
  const float y = ImGui::GetCursorPosY();
  setting_row_begin(title, description);
  const float after_y = ImGui::GetCursorPosY();
  ImGui::SetCursorPos(ImVec2(right - 38.0f, y + 2.0f));
  const bool changed = toggle_switch(title, value);
  ImGui::SetCursorPosY(std::max(after_y, ImGui::GetCursorPosY()));
  setting_row_end();
  return changed;
}

// --- decorations ------------------------------------------------------------

void draw_icon(ImDrawList *dl, Icon icon, const ImVec2 &c, float size, ImU32 col) {
  const float s = size * 0.5f;
  const float th = std::max(1.4f, size / 10.0f);
  switch (icon) {
  case Icon::Home:
    dl->AddLine(ImVec2(c.x - s, c.y - s * 0.05f), ImVec2(c.x, c.y - s * 0.9f), col, th);
    dl->AddLine(ImVec2(c.x, c.y - s * 0.9f), ImVec2(c.x + s, c.y - s * 0.05f), col, th);
    dl->AddRect(ImVec2(c.x - s * 0.7f, c.y - s * 0.25f), ImVec2(c.x + s * 0.7f, c.y + s * 0.85f),
                col, 2.0f, 0, th);
    break;
  case Icon::Mic:
    dl->AddRect(ImVec2(c.x - s * 0.35f, c.y - s * 0.95f), ImVec2(c.x + s * 0.35f, c.y + s * 0.25f),
                col, s * 0.35f, 0, th);
    dl->PathArcTo(ImVec2(c.x, c.y - s * 0.05f), s * 0.68f, 0.0f, kPi, 16);
    dl->PathStroke(col, 0, th);
    dl->AddLine(ImVec2(c.x, c.y + s * 0.63f), ImVec2(c.x, c.y + s * 0.95f), col, th);
    break;
  case Icon::Book:
    dl->AddRect(ImVec2(c.x - s * 0.75f, c.y - s * 0.85f), ImVec2(c.x + s * 0.75f, c.y + s * 0.85f),
                col, 2.0f, 0, th);
    dl->AddLine(ImVec2(c.x - s * 0.35f, c.y - s * 0.85f), ImVec2(c.x - s * 0.35f, c.y + s * 0.85f),
                col, th);
    break;
  case Icon::Bolt:
    dl->AddLine(ImVec2(c.x + s * 0.25f, c.y - s), ImVec2(c.x - s * 0.5f, c.y + s * 0.1f), col, th);
    dl->AddLine(ImVec2(c.x - s * 0.5f, c.y + s * 0.1f), ImVec2(c.x + s * 0.5f, c.y - s * 0.1f), col,
                th);
    dl->AddLine(ImVec2(c.x + s * 0.5f, c.y - s * 0.1f), ImVec2(c.x - s * 0.25f, c.y + s), col, th);
    break;
  case Icon::Brush:
    dl->AddLine(ImVec2(c.x + s * 0.85f, c.y - s * 0.85f), ImVec2(c.x - s * 0.1f, c.y + s * 0.1f),
                col, th * 1.4f);
    dl->AddCircleFilled(ImVec2(c.x - s * 0.45f, c.y + s * 0.5f), s * 0.38f, col, 12);
    break;
  case Icon::Gear:
    dl->AddCircle(c, s * 0.42f, col, 16, th);
    for (int i = 0; i < 8; ++i) {
      const float a = static_cast<float>(i) * kPi / 4.0f;
      dl->AddLine(ImVec2(c.x + std::cos(a) * s * 0.62f, c.y + std::sin(a) * s * 0.62f),
                  ImVec2(c.x + std::cos(a) * s * 0.95f, c.y + std::sin(a) * s * 0.95f), col,
                  th * 1.3f);
    }
    break;
  case Icon::File:
    dl->AddRect(ImVec2(c.x - s * 0.65f, c.y - s * 0.9f), ImVec2(c.x + s * 0.65f, c.y + s * 0.9f),
                col, 2.0f, 0, th);
    dl->AddLine(ImVec2(c.x - s * 0.3f, c.y - s * 0.3f), ImVec2(c.x + s * 0.3f, c.y - s * 0.3f), col,
                th);
    dl->AddLine(ImVec2(c.x - s * 0.3f, c.y + s * 0.1f), ImVec2(c.x + s * 0.3f, c.y + s * 0.1f), col,
                th);
    dl->AddLine(ImVec2(c.x - s * 0.3f, c.y + s * 0.5f), ImVec2(c.x + s * 0.1f, c.y + s * 0.5f), col,
                th);
    break;
  case Icon::Copy:
    dl->AddRect(ImVec2(c.x - s * 0.8f, c.y - s * 0.4f), ImVec2(c.x + s * 0.4f, c.y + s * 0.8f), col,
                2.0f, 0, th);
    dl->AddRect(ImVec2(c.x - s * 0.4f, c.y - s * 0.8f), ImVec2(c.x + s * 0.8f, c.y + s * 0.4f), col,
                2.0f, 0, th);
    break;
  case Icon::Trash:
    dl->AddLine(ImVec2(c.x - s * 0.8f, c.y - s * 0.55f), ImVec2(c.x + s * 0.8f, c.y - s * 0.55f),
                col, th);
    dl->AddLine(ImVec2(c.x - s * 0.25f, c.y - s * 0.85f), ImVec2(c.x + s * 0.25f, c.y - s * 0.85f),
                col, th);
    dl->AddRect(ImVec2(c.x - s * 0.55f, c.y - s * 0.55f), ImVec2(c.x + s * 0.55f, c.y + s * 0.9f),
                col, 2.0f, 0, th);
    break;
  case Icon::Check:
    dl->AddLine(ImVec2(c.x - s * 0.7f, c.y), ImVec2(c.x - s * 0.15f, c.y + s * 0.55f), col,
                th * 1.3f);
    dl->AddLine(ImVec2(c.x - s * 0.15f, c.y + s * 0.55f), ImVec2(c.x + s * 0.75f, c.y - s * 0.55f),
                col, th * 1.3f);
    break;
  }
}

void keycaps(const std::string &shortcut) {
  std::stringstream ss(shortcut);
  std::string part;
  bool first = true;
  ImDrawList *dl = ImGui::GetWindowDrawList();
  while (std::getline(ss, part, '+')) {
    if (!first) {
      ImGui::SameLine(0.0f, 5.0f);
    }
    first = false;
    const ImVec2 ts = ImGui::CalcTextSize(part.c_str());
    const ImVec2 size(std::max(ts.x + 14.0f, 26.0f), ts.y + 8.0f);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    dl->AddRectFilled(ImVec2(pos.x, pos.y + 2.0f), ImVec2(pos.x + size.x, pos.y + size.y + 2.0f),
                      u32(g_palette.border), 6.0f);
    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), u32(g_palette.card), 6.0f);
    dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), u32(g_palette.border), 6.0f);
    dl->AddText(ImVec2(pos.x + (size.x - ts.x) * 0.5f, pos.y + 4.0f), u32(g_palette.text),
                part.c_str());
    ImGui::Dummy(ImVec2(size.x, size.y + 2.0f));
  }
}

void pill(const std::string &text, const ImVec4 &text_color, const ImVec4 &bg_color) {
  ImDrawList *dl = ImGui::GetWindowDrawList();
  const ImVec2 pos = ImGui::GetCursorScreenPos();
  const ImVec2 ts = ImGui::CalcTextSize(text.c_str());
  const ImVec2 size(ts.x + 20.0f, ts.y + 8.0f);
  dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), u32(bg_color), 999.0f);
  dl->AddText(ImVec2(pos.x + 10.0f, pos.y + 4.0f), u32(text_color), text.c_str());
  ImGui::Dummy(size);
}

void status_dot(const ImVec4 &color, float radius) {
  const ImVec2 pos = ImGui::GetCursorScreenPos();
  const float h = ImGui::GetTextLineHeight();
  ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(pos.x + radius, pos.y + h * 0.5f), radius,
                                              u32(color), 12);
  ImGui::Dummy(ImVec2(radius * 2.0f, h));
}

void stat_card(const Fonts &fonts, const char *id, const char *value, const char *label, Icon icon,
               float width) {
  if (begin_card(id, ImVec2(width, 0.0f), true, 16.0f)) {
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList *dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(pos, ImVec2(pos.x + 30.0f, pos.y + 30.0f), u32(g_palette.accent_soft), 8.0f);
    draw_icon(dl, icon, ImVec2(pos.x + 15.0f, pos.y + 15.0f), 14.0f, u32(g_palette.accent));
    ImGui::Dummy(ImVec2(30.0f, 30.0f));
    ImGui::PushFont(fonts.heading);
    ImGui::TextUnformatted(value);
    ImGui::PopFont();
    ImGui::PushStyleColor(ImGuiCol_Text, g_palette.text_muted);
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();
  }
  end_card();
}

void draw_waveform(ImDrawList *dl, const ImVec2 &origin, float width, float height,
                   const std::vector<float> &levels, ImU32 color) {
  if (levels.empty()) {
    return;
  }
  const float step = width / static_cast<float>(levels.size());
  const float cy = origin.y + height * 0.5f;
  for (size_t i = 0; i < levels.size(); ++i) {
    const float bh = std::max(3.0f, levels[i] * height);
    const float x = origin.x + step * static_cast<float>(i);
    dl->AddRectFilled(ImVec2(x + step * 0.2f, cy - bh * 0.5f),
                      ImVec2(x + step * 0.8f, cy + bh * 0.5f), color, 999.0f);
  }
}

void draw_idle_wave(ImDrawList *dl, const ImVec2 &origin, float width, float height, float time,
                    ImU32 color) {
  const int bars = 32;
  std::vector<float> levels(bars);
  for (int i = 0; i < bars; ++i) {
    const float k = static_cast<float>(i) / bars;
    levels[static_cast<size_t>(i)] =
        0.08f + 0.22f * (0.5f + 0.5f * std::sin(time * 2.2f + k * 9.0f)) * std::sin(k * kPi);
  }
  draw_waveform(dl, origin, width, height, levels, color);
}

} // namespace gui
