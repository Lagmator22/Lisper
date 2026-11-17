#pragma once

#include "imgui.h"

#include <string>
#include <vector>

namespace gui {

struct Fonts {
  ImFont *body = nullptr;
  ImFont *small = nullptr;
  ImFont *heading = nullptr;
  ImFont *hero = nullptr;
  float body_size = 15.0f;
  float small_size = 13.0f;
  float heading_size = 20.0f;
  float hero_size = 30.0f;
};

// Colour tokens for the current theme (light or dark).
struct Palette {
  ImVec4 bg;           // window background
  ImVec4 sidebar;      // navigation rail
  ImVec4 card;         // raised surfaces
  ImVec4 card_hover;   // hovered rows
  ImVec4 border;       // hairlines
  ImVec4 text;         // primary text
  ImVec4 text_muted;   // secondary text
  ImVec4 text_faint;   // captions
  ImVec4 primary;      // primary button fill (near-black / near-white)
  ImVec4 primary_text; // text on primary
  ImVec4 accent;       // brand accent
  ImVec4 accent_soft;  // tinted accent background
  ImVec4 success;
  ImVec4 warning;
  ImVec4 danger;
  ImVec4 input; // input field fill
  bool dark = false;
};

enum class Icon { Home, Mic, Book, Bolt, Brush, Gear, File, Copy, Trash, Check, Plug, Download };

ImU32 color_u32(int r, int g, int b, int a = 255);
ImVec4 color_v4(int r, int g, int b, int a = 255);
ImVec4 with_alpha(const ImVec4 &c, float a);

Fonts install_fonts(float content_scale = 1.0f);
void apply_theme(bool dark);
const Palette &palette();

// --- surfaces -------------------------------------------------------------
bool begin_card(const char *id, const ImVec2 &size = ImVec2(0, 0), bool auto_height = true,
                float padding = 20.0f);
void end_card();

// --- text -----------------------------------------------------------------
void heading(const Fonts &fonts, const char *text);
void hero(const Fonts &fonts, const char *text);
void muted(const char *text, bool wrap = true);
void caption(const Fonts &fonts, const char *text);
void section_label(const Fonts &fonts, const char *text); // small caps-style label

// --- controls -------------------------------------------------------------
bool primary_button(const char *label, const ImVec2 &size = ImVec2(0, 0));
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

// --- decorations ----------------------------------------------------------
void draw_icon(ImDrawList *dl, Icon icon, const ImVec2 &center, float size, ImU32 color);
void keycaps(const std::string &shortcut);
void pill(const std::string &text, const ImVec4 &text_color, const ImVec4 &bg_color);
void status_dot(const ImVec4 &color, float radius = 4.0f);
void stat_card(const Fonts &fonts, const char *id, const char *value, const char *label, Icon icon,
               float width);
void draw_waveform(ImDrawList *dl, const ImVec2 &origin, float width, float height,
                   const std::vector<float> &levels, ImU32 color);
void draw_idle_wave(ImDrawList *dl, const ImVec2 &origin, float width, float height, float time,
                    ImU32 color);

} // namespace gui
