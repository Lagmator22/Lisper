#pragma once

#include "imgui.h"

#include <string>
#include <vector>

namespace gui {

struct Fonts {
  ImFont *body = nullptr;    // Inter Regular 14
  ImFont *medium = nullptr;  // Inter Medium 14
  ImFont *small = nullptr;   // Inter Regular 12.5
  ImFont *label = nullptr;   // Inter SemiBold 11 (eyebrow labels)
  ImFont *heading = nullptr; // Source Serif Subhead SemiBold 20
  ImFont *hero = nullptr;    // Source Serif Display 38
  ImFont *number = nullptr;  // Source Serif Display 30 (stats)
  ImFont *lead = nullptr;    // Inter Regular 16 (live transcript)
};

// Colour tokens for the current theme.
struct Palette {
  ImVec4 bg, sidebar, card, card_hover, border, border_strong;
  ImVec4 text, text_muted, text_faint;
  ImVec4 primary, primary_text; // high-contrast neutral button
  ImVec4 accent, accent_hover, accent_soft, accent_text;
  ImVec4 success, warning, danger;
  ImVec4 input;
  ImVec4 stage, stage_text, stage_muted; // the dark dictation stage
  bool dark = false;
};

enum class Icon { Home, Mic, Book, Bolt, Brush, Gear, File, Copy, Trash, Check, Stop };

ImU32 color_u32(int r, int g, int b, int a = 255);
ImVec4 color_v4(int r, int g, int b, int a = 255);
ImVec4 with_alpha(const ImVec4 &c, float a);
ImVec4 mix(const ImVec4 &a, const ImVec4 &b, float t);

Fonts install_fonts(float content_scale = 1.0f);
void apply_theme(bool dark);
const Palette &palette();

// --- motion ---------------------------------------------------------------
// Frame-rate independent easing toward `target`, state kept per ImGui id.
float anim(ImGuiID id, float target, float speed = 12.0f);
// Spring toward `target` (value/velocity kept per id). Slight overshoot.
float spring(ImGuiID id, float target, float stiffness = 260.0f, float damping = 24.0f);
float ease_out_cubic(float t);
float ease_in_out(float t);

// --- surfaces -------------------------------------------------------------
// `border` overrides the hairline colour when its alpha is non-zero.
bool begin_card(const char *id, const ImVec2 &size = ImVec2(0, 0), bool auto_height = true,
                float padding = 22.0f, const ImVec4 &border = ImVec4(0, 0, 0, 0));
void end_card(bool shadow = true);

// --- text -----------------------------------------------------------------
void heading(const Fonts &fonts, const char *text);
void hero(const Fonts &fonts, const char *text);
void muted(const char *text, bool wrap = true);
void caption(const Fonts &fonts, const char *text);
void section_label(const Fonts &fonts, const char *text); // small caps eyebrow

// --- controls -------------------------------------------------------------
bool primary_button(const char *label, const ImVec2 &size = ImVec2(0, 0)); // clay
bool dark_button(const char *label, const ImVec2 &size = ImVec2(0, 0));    // ink
bool secondary_button(const char *label, const ImVec2 &size = ImVec2(0, 0));
bool danger_button(const char *label, const ImVec2 &size = ImVec2(0, 0));
bool ghost_icon_button(const char *id, Icon icon, const char *tooltip);
bool toggle_switch(const char *id, bool *value);
bool segmented(const char *id, int *value, const std::vector<const char *> &options,
               float width = 0.0f);
bool nav_item(const Fonts &fonts, const char *label, Icon icon, bool selected);
bool setting_row_toggle(const char *title, const char *description, bool *value);
void setting_row_begin(const char *title, const char *description);
void setting_row_end();
void divider();

// --- decorations ----------------------------------------------------------
void draw_icon(ImDrawList *dl, Icon icon, const ImVec2 &center, float size, ImU32 color);
void keycaps(const std::string &shortcut, bool on_dark = false);
void pill(const std::string &text, const ImVec4 &text_color, const ImVec4 &bg_color);
void status_dot(const ImVec4 &color, float radius = 4.0f, bool pulse = false);
void stat_card(const Fonts &fonts, const char *id, double value, const char *suffix,
               const char *label, Icon icon, float width);
void draw_brand_mark(ImDrawList *dl, const ImVec2 &pos, float size, float time);

} // namespace gui
