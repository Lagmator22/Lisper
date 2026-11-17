#include "gui/theme.h"

#include "gui/embedded_fonts.h"

#include "imgui_internal.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>

namespace gui {

namespace {

Palette g_palette;
constexpr float kPi = 3.14159265f;

ImFont *add_font(ImGuiIO &io, const unsigned char *data, size_t size, float px) {
  ImFontConfig cfg;
  cfg.FontDataOwnedByAtlas = false; // compiled into the binary
  cfg.OversampleH = 2;
  cfg.OversampleV = 2;
  return io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char *>(data), static_cast<int>(size),
                                        px, &cfg);
}

ImVec4 hex(unsigned rgb, float a = 1.0f) {
  return ImVec4(static_cast<float>((rgb >> 16) & 0xFF) / 255.0f,
                static_cast<float>((rgb >> 8) & 0xFF) / 255.0f,
                static_cast<float>(rgb & 0xFF) / 255.0f, a);
}

// Warm, paper-like light theme with a single clay accent.
Palette make_light() {
  Palette p;
  p.dark = false;
  p.bg = hex(0xFAF9F5);
  p.sidebar = hex(0xF2F0E8);
  p.card = hex(0xFFFFFF);
  p.card_hover = hex(0xF6F4EE);
  p.border = hex(0xE6E3D8);
  p.border_strong = hex(0xD6D2C4);
  p.text = hex(0x1F1E1B);
  p.text_muted = hex(0x5F5D57);
  p.text_faint = hex(0x9A978E);
  p.primary = hex(0x1F1E1B);
  p.primary_text = hex(0xFAF9F5);
  p.accent = hex(0xD97757);
  p.accent_hover = hex(0xC6613F);
  p.accent_soft = hex(0xF7E7DF);
  p.accent_text = hex(0xB4532F);
  p.success = hex(0x4E8A52);
  p.warning = hex(0xB0741C);
  p.danger = hex(0xB5483B);
  p.input = hex(0xF4F2EB);
  p.stage = hex(0x1F1E1B);
  p.stage_text = hex(0xF5F3EC);
  p.stage_muted = hex(0xA8A499);
  return p;
}

Palette make_dark() {
  Palette p;
  p.dark = true;
  p.bg = hex(0x262624);
  p.sidebar = hex(0x1F1E1D);
  p.card = hex(0x2E2E2B);
  p.card_hover = hex(0x353531);
  p.border = hex(0x3B3A36);
  p.border_strong = hex(0x4A4944);
  p.text = hex(0xF3F1EA);
  p.text_muted = hex(0xBFBCB1);
  p.text_faint = hex(0x8C8A82);
  p.primary = hex(0xF3F1EA);
  p.primary_text = hex(0x1F1E1D);
  p.accent = hex(0xD97757);
  p.accent_hover = hex(0xE38B6D);
  p.accent_soft = hex(0x3E2F28);
  p.accent_text = hex(0xEBA07F);
  p.success = hex(0x7FBF7F);
  p.warning = hex(0xE0B055);
  p.danger = hex(0xE58A7C);
  p.input = hex(0x2A2A27);
  p.stage = hex(0x181816);
  p.stage_text = hex(0xF5F3EC);
  p.stage_muted = hex(0x9C998F);
  return p;
}

ImU32 u32(const ImVec4 &c) {
  return ImGui::GetColorU32(c);
}

bool push_wrap_if_unset() {
  if (ImGui::GetCurrentWindow()->DC.TextWrapPos < 0.0f) {
    ImGui::PushTextWrapPos(0.0f);
    return true;
  }
  return false;
}

// Custom-drawn button with eased hover/press and a slight press-in scale.
bool draw_button(const char *label, const ImVec2 &size_arg, const ImVec4 &bg,
                 const ImVec4 &bg_hover, const ImVec4 &fg, const ImVec4 &border) {
  ImGuiStyle &style = ImGui::GetStyle();
  const ImVec2 text = ImGui::CalcTextSize(label, nullptr, true);
  const ImVec2 size(size_arg.x > 0.0f
                        ? size_arg.x
                        : (size_arg.x < 0.0f ? ImGui::GetContentRegionAvail().x : text.x + 32.0f),
                    size_arg.y > 0.0f ? size_arg.y : text.y + 18.0f);
  const ImVec2 pos = ImGui::GetCursorScreenPos();
  const bool pressed = ImGui::InvisibleButton(label, size);
  const ImGuiID id = ImGui::GetItemID();
  const bool disabled = (ImGui::GetItemFlags() & ImGuiItemFlags_Disabled) != 0;
  const float hover = anim(id, ImGui::IsItemHovered() && !disabled ? 1.0f : 0.0f, 14.0f);
  const float press = anim(id + 1, ImGui::IsItemActive() ? 1.0f : 0.0f, 24.0f);

  const float shrink = press * 1.5f;
  const ImVec2 a(pos.x + shrink, pos.y + shrink);
  const ImVec2 b(pos.x + size.x - shrink, pos.y + size.y - shrink);
  const float alpha = disabled ? style.DisabledAlpha : 1.0f;
  ImDrawList *dl = ImGui::GetWindowDrawList();
  dl->AddRectFilled(a, b, u32(with_alpha(mix(bg, bg_hover, hover), bg.w * alpha)), 10.0f);
  if (border.w > 0.0f) {
    dl->AddRect(a, b, u32(with_alpha(mix(border, g_palette.border_strong, hover), alpha)), 10.0f, 0,
                1.0f);
  }
  const char *end = ImGui::FindRenderedTextEnd(label);
  dl->AddText(ImVec2(pos.x + (size.x - text.x) * 0.5f, pos.y + (size.y - text.y) * 0.5f),
              u32(with_alpha(fg, alpha)), label, end);
  return pressed && !disabled;
}

} // namespace

// ---------------------------------------------------------------------------

ImU32 color_u32(int r, int g, int b, int a) {
  return IM_COL32(r, g, b, a);
}

ImVec4 color_v4(int r, int g, int b, int a) {
  return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
}

ImVec4 with_alpha(const ImVec4 &c, float a) {
  return ImVec4(c.x, c.y, c.z, a);
}

ImVec4 mix(const ImVec4 &a, const ImVec4 &b, float t) {
  return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t,
                a.w + (b.w - a.w) * t);
}

const Palette &palette() {
  return g_palette;
}

float ease_out_cubic(float t) {
  t = std::clamp(t, 0.0f, 1.0f);
  const float u = 1.0f - t;
  return 1.0f - u * u * u;
}

float ease_in_out(float t) {
  t = std::clamp(t, 0.0f, 1.0f);
  return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) * 0.5f;
}

float anim(ImGuiID id, float target, float speed) {
  ImGuiStorage *st = ImGui::GetStateStorage();
  const ImGuiID key = id ^ 0x5A17u;
  float v = st->GetFloat(key, target);
  v += (target - v) * std::min(1.0f, ImGui::GetIO().DeltaTime * speed);
  if (std::fabs(target - v) < 0.0005f) {
    v = target;
  }
  st->SetFloat(key, v);
  return v;
}

float spring(ImGuiID id, float target, float stiffness, float damping) {
  ImGuiStorage *st = ImGui::GetStateStorage();
  const ImGuiID kv = id ^ 0x5B1Au;
  const ImGuiID kd = id ^ 0x5B1Bu;
  float v = st->GetFloat(kv, target);
  float vel = st->GetFloat(kd, 0.0f);
  const float dt = std::min(ImGui::GetIO().DeltaTime, 0.05f);
  const int steps = std::max(1, static_cast<int>(std::ceil(dt / 0.004f)));
  const float h = dt / static_cast<float>(steps);
  for (int i = 0; i < steps; ++i) {
    vel += (-stiffness * (v - target) - damping * vel) * h;
    v += vel * h;
  }
  st->SetFloat(kv, v);
  st->SetFloat(kd, vel);
  return v;
}

Fonts install_fonts(float content_scale) {
  ImGuiIO &io = ImGui::GetIO();
  io.Fonts->Clear();
  const float s = std::max(1.0f, content_scale);

  Fonts f;
  f.body = add_font(io, lisper_font_inter_regular, lisper_font_inter_regular_size, 14.0f * s);
  f.medium = add_font(io, lisper_font_inter_medium, lisper_font_inter_medium_size, 14.0f * s);
  f.small = add_font(io, lisper_font_inter_regular, lisper_font_inter_regular_size, 12.5f * s);
  f.label = add_font(io, lisper_font_inter_semibold, lisper_font_inter_semibold_size, 11.0f * s);
  f.heading = add_font(io, lisper_font_serif_subhead, lisper_font_serif_subhead_size, 20.0f * s);
  f.hero = add_font(io, lisper_font_serif_display, lisper_font_serif_display_size, 38.0f * s);
  f.number = add_font(io, lisper_font_serif_display, lisper_font_serif_display_size, 30.0f * s);
  f.lead = add_font(io, lisper_font_inter_regular, lisper_font_inter_regular_size, 16.0f * s);
  io.FontDefault = f.body;
  return f;
}

void apply_theme(bool dark) {
  g_palette = dark ? make_dark() : make_light();
  const Palette &p = g_palette;

  ImGuiStyle &style = ImGui::GetStyle();
  style.WindowPadding = ImVec2(0.0f, 0.0f);
  style.FramePadding = ImVec2(12.0f, 9.0f);
  style.ItemSpacing = ImVec2(10.0f, 10.0f);
  style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
  style.CellPadding = ImVec2(8.0f, 8.0f);
  style.ScrollbarSize = 8.0f;
  style.ScrollbarRounding = 999.0f;
  style.WindowRounding = 0.0f;
  style.ChildRounding = 16.0f;
  style.FrameRounding = 10.0f;
  style.GrabRounding = 999.0f;
  style.PopupRounding = 12.0f;
  style.WindowBorderSize = 0.0f;
  style.ChildBorderSize = 1.0f;
  style.FrameBorderSize = 0.0f;
  style.PopupBorderSize = 1.0f;
  style.DisabledAlpha = 0.42f;
  style.SelectableTextAlign = ImVec2(0.0f, 0.5f);
  style.PopupBorderSize = 1.0f;

  ImVec4 *c = style.Colors;
  c[ImGuiCol_Text] = p.text;
  c[ImGuiCol_TextDisabled] = p.text_faint;
  c[ImGuiCol_WindowBg] = p.bg;
  c[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
  c[ImGuiCol_PopupBg] = p.card;
  c[ImGuiCol_Border] = p.border;
  c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
  c[ImGuiCol_FrameBg] = p.input;
  c[ImGuiCol_FrameBgHovered] = mix(p.input, p.border, 0.35f);
  c[ImGuiCol_FrameBgActive] = mix(p.input, p.border, 0.5f);
  c[ImGuiCol_Button] = p.input;
  c[ImGuiCol_ButtonHovered] = p.card_hover;
  c[ImGuiCol_ButtonActive] = p.border;
  c[ImGuiCol_Header] = p.accent_soft;
  c[ImGuiCol_HeaderHovered] = p.card_hover;
  c[ImGuiCol_HeaderActive] = p.accent_soft;
  c[ImGuiCol_Separator] = p.border;
  c[ImGuiCol_CheckMark] = p.accent;
  c[ImGuiCol_SliderGrab] = p.accent;
  c[ImGuiCol_SliderGrabActive] = p.accent_hover;
  c[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
  c[ImGuiCol_ScrollbarGrab] = with_alpha(p.text_faint, 0.28f);
  c[ImGuiCol_ScrollbarGrabHovered] = with_alpha(p.text_faint, 0.5f);
  c[ImGuiCol_ScrollbarGrabActive] = with_alpha(p.text_faint, 0.7f);
  c[ImGuiCol_TextSelectedBg] = with_alpha(p.accent, 0.28f);
  c[ImGuiCol_NavCursor] = p.accent;
  c[ImGuiCol_InputTextCursor] = p.accent;
  c[ImGuiCol_ModalWindowDimBg] = ImVec4(0, 0, 0, 0.35f);
}

// --- surfaces ---------------------------------------------------------------

namespace {
struct CardFrame {
  ImGuiID id;
  bool shadow_drawn;
};
std::vector<CardFrame> g_card_stack;

void draw_card_shadow(ImDrawList *dl, const ImVec2 &a, const ImVec2 &b) {
  // Layered soft shadow, warm-tinted so it reads as paper rather than grey.
  for (int i = 1; i <= 5; ++i) {
    const float o = static_cast<float>(i);
    dl->AddRectFilled(ImVec2(a.x - o * 0.6f, a.y + o * 0.6f),
                      ImVec2(b.x + o * 0.6f, b.y + o * 1.5f), IM_COL32(90, 70, 40, 7 - i),
                      16.0f + o);
  }
}
} // namespace

bool begin_card(const char *id, const ImVec2 &size, bool auto_height, float padding,
                const ImVec4 &border) {
  // The card is merged into the parent's draw list, so the shadow has to be
  // drawn first, using the size the card had on the previous frame.
  const ImGuiID sid = ImGui::GetID(id);
  ImGuiStorage *st = ImGui::GetStateStorage();
  const float last_w = st->GetFloat(sid ^ 0xC0DEu, 0.0f);
  const float last_h = st->GetFloat(sid ^ 0xC0DFu, 0.0f);
  const bool shadow = !g_palette.dark && last_w > 0.0f && last_h > 0.0f;
  if (shadow) {
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    draw_card_shadow(ImGui::GetWindowDrawList(), pos, ImVec2(pos.x + last_w, pos.y + last_h));
  }
  g_card_stack.push_back({sid, shadow});

  ImGui::PushStyleColor(ImGuiCol_ChildBg, g_palette.card);
  ImGui::PushStyleColor(ImGuiCol_Border, border.w > 0.0f ? border : g_palette.border);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(padding, padding));
  ImGuiChildFlags flags = ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding;
  if (auto_height) {
    flags |= ImGuiChildFlags_AutoResizeY;
  }
  return ImGui::BeginChild(id, size, flags, ImGuiWindowFlags_NoScrollbar);
}

void end_card(bool shadow) {
  (void)shadow;
  ImGui::EndChild();
  ImGui::PopStyleVar();
  ImGui::PopStyleColor(2);
  if (!g_card_stack.empty()) {
    const CardFrame frame = g_card_stack.back();
    g_card_stack.pop_back();
    const ImVec2 a = ImGui::GetItemRectMin();
    const ImVec2 b = ImGui::GetItemRectMax();
    ImGuiStorage *st = ImGui::GetStateStorage();
    st->SetFloat(frame.id ^ 0xC0DEu, b.x - a.x);
    st->SetFloat(frame.id ^ 0xC0DFu, b.y - a.y);
  }
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
  // Letter-spaced eyebrow label.
  ImGui::PushFont(fonts.label);
  ImDrawList *dl = ImGui::GetWindowDrawList();
  ImVec2 pos = ImGui::GetCursorScreenPos();
  const float start = pos.x;
  for (char ch : upper) {
    const char s[2] = {ch, 0};
    dl->AddText(pos, u32(g_palette.text_faint), s);
    pos.x += ImGui::CalcTextSize(s).x + 0.9f;
  }
  ImGui::Dummy(ImVec2(pos.x - start, ImGui::GetFontSize()));
  ImGui::PopFont();
}

// --- controls ---------------------------------------------------------------

bool primary_button(const char *label, const ImVec2 &size) {
  return draw_button(label, size, g_palette.accent, g_palette.accent_hover, ImVec4(1, 1, 1, 1),
                     ImVec4(0, 0, 0, 0));
}

bool dark_button(const char *label, const ImVec2 &size) {
  return draw_button(label, size, g_palette.primary,
                     mix(g_palette.primary, g_palette.text_muted, 0.25f), g_palette.primary_text,
                     ImVec4(0, 0, 0, 0));
}

bool secondary_button(const char *label, const ImVec2 &size) {
  return draw_button(label, size, g_palette.card, g_palette.card_hover, g_palette.text,
                     g_palette.border);
}

bool danger_button(const char *label, const ImVec2 &size) {
  return draw_button(label, size, g_palette.card, mix(g_palette.card, g_palette.danger, 0.08f),
                     g_palette.danger, g_palette.border);
}

bool ghost_icon_button(const char *id, Icon icon, const char *tooltip) {
  const float sz = 30.0f;
  const ImVec2 pos = ImGui::GetCursorScreenPos();
  const bool pressed = ImGui::InvisibleButton(id, ImVec2(sz, sz));
  const bool hovered = ImGui::IsItemHovered();
  const float h = anim(ImGui::GetItemID(), hovered ? 1.0f : 0.0f, 16.0f);
  ImDrawList *dl = ImGui::GetWindowDrawList();
  dl->AddRectFilled(pos, ImVec2(pos.x + sz, pos.y + sz),
                    u32(with_alpha(g_palette.border, 0.7f * h)), 8.0f);
  if (hovered && tooltip != nullptr) {
    ImGui::SetTooltip("%s", tooltip);
  }
  draw_icon(dl, icon, ImVec2(pos.x + sz * 0.5f, pos.y + sz * 0.5f), 15.0f,
            u32(mix(g_palette.text_faint, g_palette.text, h)));
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
  const ImGuiID iid = ImGui::GetItemID();
  const float t = spring(iid, *value ? 1.0f : 0.0f, 420.0f, 26.0f);
  const float hover = anim(iid + 7, ImGui::IsItemHovered() ? 1.0f : 0.0f, 14.0f);
  const float c = std::clamp(t, 0.0f, 1.0f);

  ImDrawList *dl = ImGui::GetWindowDrawList();
  const ImVec4 off = g_palette.dark ? hex(0x4A4944) : hex(0xD9D5C8);
  dl->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + h), u32(mix(off, g_palette.accent, c)),
                    h * 0.5f);
  const float r = h * 0.5f - 3.0f + hover * 0.6f;
  const ImVec2 knob(pos.x + h * 0.5f + t * (w - h), pos.y + h * 0.5f);
  dl->AddCircleFilled(ImVec2(knob.x, knob.y + 1.0f), r, IM_COL32(0, 0, 0, 30), 24);
  dl->AddCircleFilled(knob, r, IM_COL32(255, 255, 255, 255), 24);
  return pressed;
}

bool segmented(const char *id, int *value, const std::vector<const char *> &options, float width) {
  ImGui::PushID(id);
  const float total = width > 0.0f ? width : ImGui::GetContentRegionAvail().x;
  const float h = 36.0f;
  const ImVec2 pos = ImGui::GetCursorScreenPos();
  ImDrawList *dl = ImGui::GetWindowDrawList();
  dl->AddRectFilled(pos, ImVec2(pos.x + total, pos.y + h), u32(g_palette.input), 11.0f);
  const float seg_w = total / static_cast<float>(options.size());

  // Sliding selection pill.
  const float x = spring(ImGui::GetID("pill"), static_cast<float>(*value) * seg_w, 340.0f, 30.0f);
  const ImVec2 pa(pos.x + x + 3.0f, pos.y + 3.0f);
  const ImVec2 pb(pos.x + x + seg_w - 3.0f, pos.y + h - 3.0f);
  if (!g_palette.dark) {
    dl->AddRectFilled(ImVec2(pa.x, pa.y + 1.0f), ImVec2(pb.x, pb.y + 1.0f),
                      IM_COL32(60, 45, 30, 14), 8.5f);
  }
  dl->AddRectFilled(pa, pb, u32(g_palette.dark ? g_palette.card_hover : g_palette.card), 8.5f);

  bool changed = false;
  for (size_t i = 0; i < options.size(); ++i) {
    const ImVec2 a(pos.x + seg_w * static_cast<float>(i), pos.y);
    ImGui::SetCursorScreenPos(a);
    ImGui::PushID(static_cast<int>(i));
    if (ImGui::InvisibleButton("seg", ImVec2(seg_w, h))) {
      if (*value != static_cast<int>(i)) {
        *value = static_cast<int>(i);
        changed = true;
      }
    }
    const float hv = anim(ImGui::GetItemID(), ImGui::IsItemHovered() ? 1.0f : 0.0f, 14.0f);
    ImGui::PopID();
    const bool selected = *value == static_cast<int>(i);
    const ImVec2 ts = ImGui::CalcTextSize(options[i]);
    const ImVec4 col = selected ? g_palette.text : mix(g_palette.text_muted, g_palette.text, hv);
    dl->AddText(ImVec2(a.x + (seg_w - ts.x) * 0.5f, a.y + (h - ts.y) * 0.5f), u32(col), options[i]);
  }
  ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + h));
  ImGui::Dummy(ImVec2(total, 0.0f));
  ImGui::PopID();
  return changed;
}

bool nav_item(const Fonts &fonts, const char *label, Icon icon, bool selected) {
  const float h = 36.0f;
  const float w = ImGui::GetContentRegionAvail().x;
  const ImVec2 pos = ImGui::GetCursorScreenPos();
  const bool pressed = ImGui::InvisibleButton(label, ImVec2(w, h));
  const float hover =
      anim(ImGui::GetItemID(), ImGui::IsItemHovered() && !selected ? 1.0f : 0.0f, 14.0f);
  const float sel = anim(ImGui::GetItemID() + 3, selected ? 1.0f : 0.0f, 14.0f);
  ImDrawList *dl = ImGui::GetWindowDrawList();
  if (hover > 0.01f) {
    dl->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + h),
                      u32(with_alpha(g_palette.border, 0.45f * hover)), 9.0f);
  }
  const ImVec4 col = mix(g_palette.text_muted, g_palette.text, std::max(sel, hover * 0.6f));
  draw_icon(dl, icon, ImVec2(pos.x + 21.0f + hover * 1.5f, pos.y + h * 0.5f), 15.0f,
            u32(mix(col, g_palette.accent, sel)));
  ImGui::PushFont(selected ? fonts.medium : fonts.body);
  const ImVec2 ts = ImGui::CalcTextSize(label);
  dl->AddText(ImVec2(pos.x + 40.0f + hover * 1.5f, pos.y + (h - ts.y) * 0.5f), u32(col), label);
  ImGui::PopFont();
  return pressed;
}

void setting_row_begin(const char *title, const char *description) {
  ImGui::BeginGroup();
  ImGui::TextUnformatted(title);
  if (description != nullptr && *description != '\0') {
    ImGui::PushStyleColor(ImGuiCol_Text, g_palette.text_muted);
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x * 0.7f);
    ImGui::TextUnformatted(description);
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
  }
  ImGui::EndGroup();
}

void setting_row_end() {
  ImGui::Dummy(ImVec2(0.0f, 2.0f));
}

void divider() {
  const ImVec2 pos = ImGui::GetCursorScreenPos();
  const float w = ImGui::GetContentRegionAvail().x;
  ImGui::GetWindowDrawList()->AddLine(ImVec2(pos.x, pos.y + 6.0f), ImVec2(pos.x + w, pos.y + 6.0f),
                                      u32(g_palette.border), 1.0f);
  ImGui::Dummy(ImVec2(w, 13.0f));
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
  // Outline icons on a 24-unit grid with 1.6px strokes.
  const float u = size / 24.0f;
  const float th = std::max(1.4f, 1.6f * u * 1.1f);
  auto P = [&](float x, float y) { return ImVec2(c.x + (x - 12.0f) * u, c.y + (y - 12.0f) * u); };
  switch (icon) {
  case Icon::Home:
    dl->PathLineTo(P(3, 10));
    dl->PathLineTo(P(12, 3));
    dl->PathLineTo(P(21, 10));
    dl->PathStroke(col, 0, th);
    dl->AddRect(P(5.5f, 9), P(18.5f, 21), col, 2.5f * u, 0, th);
    break;
  case Icon::Mic:
    dl->AddRect(P(9, 2.5f), P(15, 14.5f), col, 3.0f * u, 0, th);
    dl->PathArcTo(P(12, 11), 7.0f * u, 0.0f, kPi, 18);
    dl->PathStroke(col, 0, th);
    dl->AddLine(P(12, 18), P(12, 21.5f), col, th);
    break;
  case Icon::Book:
    dl->AddRect(P(4.5f, 3), P(19.5f, 21), col, 2.5f * u, 0, th);
    dl->AddLine(P(8.5f, 3), P(8.5f, 21), col, th);
    dl->AddLine(P(11.5f, 8), P(16, 8), col, th);
    break;
  case Icon::Bolt:
    dl->PathLineTo(P(13, 2.5f));
    dl->PathLineTo(P(5, 13.5f));
    dl->PathLineTo(P(12, 13.5f));
    dl->PathLineTo(P(11, 21.5f));
    dl->PathLineTo(P(19, 10.5f));
    dl->PathLineTo(P(12, 10.5f));
    dl->PathStroke(col, ImDrawFlags_Closed, th);
    break;
  case Icon::Brush:
    dl->AddLine(P(19.5f, 4.5f), P(10.5f, 13.5f), col, th);
    dl->PathArcTo(P(8, 16.5f), 3.2f * u, 0.0f, 2.0f * kPi, 18);
    dl->PathStroke(col, 0, th);
    break;
  case Icon::Gear:
    dl->AddCircle(c, 3.2f * u, col, 16, th);
    for (int i = 0; i < 8; ++i) {
      const float a = static_cast<float>(i) * kPi / 4.0f;
      dl->AddLine(ImVec2(c.x + std::cos(a) * 6.6f * u, c.y + std::sin(a) * 6.6f * u),
                  ImVec2(c.x + std::cos(a) * 9.4f * u, c.y + std::sin(a) * 9.4f * u), col,
                  th * 1.35f);
    }
    dl->AddCircle(c, 6.6f * u, col, 24, th);
    break;
  case Icon::File:
    dl->PathLineTo(P(14, 2.5f));
    dl->PathLineTo(P(5.5f, 2.5f));
    dl->PathLineTo(P(5.5f, 21.5f));
    dl->PathLineTo(P(18.5f, 21.5f));
    dl->PathLineTo(P(18.5f, 7));
    dl->PathLineTo(P(14, 2.5f));
    dl->PathLineTo(P(14, 7));
    dl->PathLineTo(P(18.5f, 7));
    dl->PathStroke(col, 0, th);
    dl->AddLine(P(9, 13), P(15, 13), col, th);
    dl->AddLine(P(9, 17), P(13, 17), col, th);
    break;
  case Icon::Copy:
    dl->AddRect(P(8.5f, 8.5f), P(20.5f, 20.5f), col, 2.5f * u, 0, th);
    dl->PathLineTo(P(15.5f, 8.5f));
    dl->PathLineTo(P(15.5f, 3.5f));
    dl->PathLineTo(P(3.5f, 3.5f));
    dl->PathLineTo(P(3.5f, 15.5f));
    dl->PathLineTo(P(8.5f, 15.5f));
    dl->PathStroke(col, 0, th);
    break;
  case Icon::Trash:
    dl->AddLine(P(3.5f, 6), P(20.5f, 6), col, th);
    dl->AddLine(P(9, 6), P(9.5f, 3), col, th);
    dl->AddLine(P(9.5f, 3), P(14.5f, 3), col, th);
    dl->AddLine(P(14.5f, 3), P(15, 6), col, th);
    dl->PathLineTo(P(5.5f, 6));
    dl->PathLineTo(P(6.5f, 21));
    dl->PathLineTo(P(17.5f, 21));
    dl->PathLineTo(P(18.5f, 6));
    dl->PathStroke(col, 0, th);
    break;
  case Icon::Check:
    dl->PathLineTo(P(4.5f, 12.5f));
    dl->PathLineTo(P(9.5f, 17.5f));
    dl->PathLineTo(P(19.5f, 6.5f));
    dl->PathStroke(col, 0, th * 1.15f);
    break;
  case Icon::Stop:
    dl->AddRectFilled(P(6.5f, 6.5f), P(17.5f, 17.5f), col, 2.5f * u);
    break;
  }
}

void keycaps(const std::string &shortcut, bool on_dark) {
  std::stringstream ss(shortcut);
  std::string part;
  bool first = true;
  ImDrawList *dl = ImGui::GetWindowDrawList();
  const ImVec4 face = on_dark ? hex(0x34332F) : g_palette.card;
  const ImVec4 edge = on_dark ? hex(0x4A4843) : g_palette.border_strong;
  const ImVec4 text = on_dark ? hex(0xF3F1EA) : g_palette.text;
  while (std::getline(ss, part, '+')) {
    if (!first) {
      ImGui::SameLine(0.0f, 4.0f);
    }
    first = false;
    const ImVec2 ts = ImGui::CalcTextSize(part.c_str());
    const ImVec2 size(std::max(ts.x + 14.0f, 24.0f), ts.y + 8.0f);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    dl->AddRectFilled(ImVec2(pos.x, pos.y + 1.5f), ImVec2(pos.x + size.x, pos.y + size.y + 1.5f),
                      u32(edge), 6.0f);
    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), u32(face), 6.0f);
    dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), u32(with_alpha(edge, 0.8f)), 6.0f);
    dl->AddText(ImVec2(pos.x + (size.x - ts.x) * 0.5f, pos.y + 4.0f), u32(text), part.c_str());
    ImGui::Dummy(ImVec2(size.x, size.y + 1.5f));
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

void status_dot(const ImVec4 &color, float radius, bool pulse) {
  const ImVec2 pos = ImGui::GetCursorScreenPos();
  const float h = ImGui::GetTextLineHeight();
  const ImVec2 c(pos.x + radius + 2.0f, pos.y + h * 0.5f);
  ImDrawList *dl = ImGui::GetWindowDrawList();
  if (pulse) {
    const float t = std::fmod(static_cast<float>(ImGui::GetTime()), 1.6f) / 1.6f;
    dl->AddCircleFilled(c, radius + t * 6.0f, u32(with_alpha(color, 0.35f * (1.0f - t))), 20);
  }
  dl->AddCircleFilled(c, radius, u32(color), 16);
  ImGui::Dummy(ImVec2(radius * 2.0f + 4.0f, h));
}

void stat_card(const Fonts &fonts, const char *id, double value, const char *suffix,
               const char *label, Icon icon, float width) {
  if (begin_card(id, ImVec2(width, 0.0f), true, 18.0f)) {
    ImDrawList *dl = ImGui::GetWindowDrawList();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float right = pos.x + ImGui::GetContentRegionAvail().x;
    draw_icon(dl, icon, ImVec2(right - 9.0f, pos.y + 10.0f), 15.0f, u32(g_palette.text_faint));

    // Count up to the real value.
    const float shown = anim(ImGui::GetID("count"), static_cast<float>(value), 6.0f);
    char buf[64];
    if (value >= 1000.0) {
      std::snprintf(buf, sizeof(buf), "%.1fk%s", shown / 1000.0f, suffix);
    } else {
      std::snprintf(buf, sizeof(buf), "%d%s", static_cast<int>(std::lround(shown)), suffix);
    }
    ImGui::PushFont(fonts.number);
    ImGui::TextUnformatted(buf);
    ImGui::PopFont();
    ImGui::PushStyleColor(ImGuiCol_Text, g_palette.text_muted);
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();
  }
  end_card();
}

void draw_brand_mark(ImDrawList *dl, const ImVec2 &pos, float size, float time) {
  // A rounded clay tile with five voice bars that idle gently.
  dl->AddRectFilled(pos, ImVec2(pos.x + size, pos.y + size), u32(g_palette.accent), size * 0.3f);
  const float heights[] = {0.28f, 0.55f, 0.82f, 0.5f, 0.3f};
  const float bar = size * 0.085f;
  const float gap = size * 0.07f;
  const float total = 5.0f * bar + 4.0f * gap;
  const float x0 = pos.x + (size - total) * 0.5f;
  const float cy = pos.y + size * 0.5f;
  for (int i = 0; i < 5; ++i) {
    const float wobble = 0.08f * std::sin(time * 2.4f + static_cast<float>(i) * 1.3f);
    const float bh = size * std::clamp(heights[i] + wobble, 0.2f, 0.9f) * 0.72f;
    const float x = x0 + static_cast<float>(i) * (bar + gap);
    dl->AddRectFilled(ImVec2(x, cy - bh * 0.5f), ImVec2(x + bar, cy + bh * 0.5f),
                      IM_COL32(255, 250, 244, 255), bar * 0.5f);
  }
}

} // namespace gui
