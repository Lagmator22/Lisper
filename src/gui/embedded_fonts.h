#pragma once

#include <cstddef>

// Fonts compiled into the binary (see cmake/embed_fonts.cmake).
// Inter and Source Serif 4 are licensed under the SIL Open Font License 1.1.
extern "C" {
extern const unsigned char lisper_font_inter_regular[];
extern const size_t lisper_font_inter_regular_size;
extern const unsigned char lisper_font_inter_medium[];
extern const size_t lisper_font_inter_medium_size;
extern const unsigned char lisper_font_inter_semibold[];
extern const size_t lisper_font_inter_semibold_size;
extern const unsigned char lisper_font_serif_display[];
extern const size_t lisper_font_serif_display_size;
extern const unsigned char lisper_font_serif_subhead[];
extern const size_t lisper_font_serif_subhead_size;
}
