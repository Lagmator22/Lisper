#pragma once

#include "formatter.h"
#include "history.h"
#include "lisper.h"
#include "model_profiles.h"
#include "settings.h"

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace gui {

constexpr size_t kPathBuffer = 1024;
constexpr size_t kTextBuffer = 128;
constexpr size_t kLongBuffer = 4096;

enum class Page { Home, Transcribe, Dictionary, Snippets, Style, Settings };

struct JobRequest {
  std::string input_path;
  std::string output_path;
  std::string explicit_model_path;
  std::string model_profile;
  std::string language;
  formatter::Format format = formatter::Format::TEXT;
  LisperConfig::Device device = LisperConfig::Device::Auto;
  int threads = 4;
  int gpu_device = 0;
  bool translate = false;
  bool flash_attn = true;
  // Already-loaded engine to reuse when it matches the requested model.
  std::shared_ptr<Lisper> shared_engine;
};

struct AppState {
  // File transcription
  std::array<char, kPathBuffer> input_path{};
  std::array<char, kPathBuffer> output_path{};
  int format_index = 0;

  std::string resolved_model_path;
  std::string preview_text;
  std::string final_output_path;
  std::string status_line;
  std::string detected_language;
  int duration_ms = 0;
  int segment_count = 0;
  bool last_run_success = false;

  // --- Flow-style app state ---
  Page page = Page::Home;
  double page_changed_at = 0.0; // for the page transition
  Settings settings;
  bool settings_dirty = false; // saved (debounced) by the main loop

  std::vector<history::Entry> history;
  history::Stats stats;
  bool history_dirty = true; // reload from disk on next frame
  std::array<char, kTextBuffer> history_search{};

  std::array<char, kTextBuffer> dict_from{};
  std::array<char, kTextBuffer> dict_to{};
  std::array<char, kTextBuffer> vocab_word{};
  std::array<char, kTextBuffer> snippet_trigger{};
  std::array<char, kLongBuffer> snippet_text{};

  bool capturing_hotkey = false;
  bool hotkey_registered = false;
  std::string hotkey_error;
  std::string paste_hint;
  std::vector<std::string> input_devices;

  bool downloading = false;
  std::string download_status;

  std::string toast;
  double toast_until = 0.0;
  bool want_quit = false;
};

void show_toast(AppState &state, const std::string &message, double seconds = 2.5);

void copy_string(std::array<char, kPathBuffer> &target, const std::string &value);
void copy_string(std::array<char, kTextBuffer> &target, const std::string &value);
void copy_string(std::array<char, kLongBuffer> &target, const std::string &value);

std::string trim_copy(const char *value);
bool path_exists(const std::string &path);
std::string filename_for_display(const std::string &path);

std::string format_duration_ms(int duration_ms);

formatter::Format format_from_index(int index);

std::string computed_output_preview(const AppState &state);
std::vector<std::string> validation_issues(const AppState &state);
JobRequest build_request(const AppState &state);

} // namespace gui
