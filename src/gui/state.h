#pragma once

#include "formatter.h"
#include "lisper.h"
#include "model_profiles.h"

#include <array>
#include <string>
#include <vector>

namespace gui {

constexpr size_t kPathBuffer = 1024;
constexpr size_t kTextBuffer = 128;

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
};

struct AppState {
  std::array<char, kPathBuffer> input_path{};
  std::array<char, kPathBuffer> output_path{};
  std::array<char, kPathBuffer> manual_model_path{};
  std::array<char, kTextBuffer> language{};

  int profile_index = 2;
  int format_index = 0;
  int device_index = 0;
  int threads = 4;
  int gpu_device = 0;
  bool translate = false;
  bool flash_attn = true;
  bool auto_scroll_log = true;

  std::string resolved_model_path;
  std::string preview_text;
  std::string final_output_path;
  std::string status_line = "Ready for a local transcription run.";
  std::string detected_language;
  int duration_ms = 0;
  int segment_count = 0;
  bool last_run_success = false;
  std::vector<std::string> logs;
};

void copy_string(std::array<char, kPathBuffer> &target,
                 const std::string &value);
void copy_string(std::array<char, kTextBuffer> &target,
                 const std::string &value);

std::string trim_copy(const char *value);
bool path_exists(const std::string &path);
std::string filename_for_display(const std::string &path);
void append_log(AppState &state, const std::string &message);

int recommended_threads();
int max_threads_for_slider();
std::string format_duration_ms(int duration_ms);

const std::vector<ModelProfile> &profiles();
formatter::Format format_from_index(int index);
const char *format_label(formatter::Format format);
LisperConfig::Device device_from_index(int index);
std::string describe_device(LisperConfig::Device device);

std::string current_profile_name(int profile_index);
std::string current_profile_filename(int profile_index);
std::string profile_description(int profile_index);

void refresh_resolved_model(AppState &state);
std::string computed_output_preview(const AppState &state);
std::vector<std::string> validation_issues(const AppState &state);
JobRequest build_request(const AppState &state);

} // namespace gui
