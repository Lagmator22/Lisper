#pragma once

#include "imgui.h"

#include <string>

namespace gui {

struct Fonts {
  ImFont *body = nullptr;
  ImFont *heading = nullptr;
  ImFont *hero = nullptr;
};

ImU32 color_u32(int r, int g, int b, int a = 255);
ImVec4 color_v4(int r, int g, int b, int a = 255);
ImVec4 gradient_color(float t);

Fonts install_fonts(float content_scale = 1.0f);
void apply_style();

bool begin_surface(const char *id, const ImVec2 &size, bool border = true,
                   ImGuiWindowFlags flags = 0);
void end_surface();

void draw_background(ImDrawList *draw_list, const ImVec2 &pos,
                     const ImVec2 &size, float time_seconds);
void draw_gradient_label(ImDrawList *draw_list, ImFont *font, float font_size,
                         ImVec2 position, const std::string &text,
                         float shimmer_offset);
void draw_processing_wave(ImDrawList *draw_list, const ImVec2 &origin,
                          float width, float height, float time_seconds,
                          bool active);

void draw_pill(const std::string &text, const ImVec4 &text_color,
               const ImVec4 &bg_color);
bool choice_button(const char *label, bool selected,
                   const ImVec2 &size = ImVec2(0, 0));
void section_title(const Fonts &fonts, const std::string &title,
                   const std::string &subtitle = {});
void label_text(const char *label);

} // namespace gui
