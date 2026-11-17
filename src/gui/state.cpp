#include "gui/state.h"

#include "media.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <sstream>
#include <thread>

namespace fs = std::filesystem;

namespace gui {

void copy_string(std::array<char, kPathBuffer> &target, const std::string &value) {
  target.fill('\0');
  std::snprintf(target.data(), target.size(), "%s", value.c_str());
}

void copy_string(std::array<char, kTextBuffer> &target, const std::string &value) {
  target.fill('\0');
  std::snprintf(target.data(), target.size(), "%s", value.c_str());
}

void copy_string(std::array<char, kLongBuffer> &target, const std::string &value) {
  target.fill('\0');
  std::snprintf(target.data(), target.size(), "%s", value.c_str());
}

void show_toast(AppState &state, const std::string &message, double seconds) {
  state.toast = message;
  state.toast_until = static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                              std::chrono::steady_clock::now().time_since_epoch())
                                              .count()) /
                          1000.0 +
                      seconds;
}

std::string trim_copy(const char *value) {
  std::string text = value == nullptr ? "" : value;
  const auto start = text.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) {
    return "";
  }

  const auto end = text.find_last_not_of(" \t\r\n");
  return text.substr(start, end - start + 1);
}

bool path_exists(const std::string &path) {
  if (path.empty()) {
    return false;
  }

  std::error_code ec;
  return fs::exists(path, ec) && !ec;
}

std::string filename_for_display(const std::string &path) {
  if (path.empty()) {
    return "No file selected";
  }

  const fs::path input(path);
  const std::string name = input.filename().string();
  return name.empty() ? path : name;
}

std::string format_duration_ms(int duration_ms) {
  if (duration_ms <= 0) {
    return "0s";
  }

  const int total_seconds = duration_ms / 1000;
  const int minutes = total_seconds / 60;
  const int seconds = total_seconds % 60;

  std::ostringstream out;
  if (minutes > 0) {
    out << minutes << "m ";
  }
  out << seconds << "s";
  return out.str();
}

formatter::Format format_from_index(int index) {
  switch (index) {
  case 1:
    return formatter::Format::SRT;
  case 2:
    return formatter::Format::JSON;
  case 3:
    return formatter::Format::RAG;
  default:
    return formatter::Format::TEXT;
  }
}

std::string computed_output_preview(const AppState &state) {
  const std::string output = trim_copy(state.output_path.data());
  const std::string input = trim_copy(state.input_path.data());
  if (output.empty() || input.empty()) {
    return "";
  }

  return formatter::resolve_output_path(output, format_from_index(state.format_index),
                                        fs::path(input).filename().string());
}

std::vector<std::string> validation_issues(const AppState &state) {
  std::vector<std::string> issues;

  const std::string input = trim_copy(state.input_path.data());
  if (input.empty()) {
    issues.push_back("Choose an audio or video file.");
  } else if (!path_exists(input)) {
    issues.push_back("The selected input file does not exist.");
  } else if (!media::is_media_file(input)) {
    issues.push_back("The input does not look like a supported audio/video file.");
  }

  if (state.resolved_model_path.empty()) {
    issues.push_back("The selected speech model is not installed. Download it in Settings.");
  } else if (!path_exists(state.resolved_model_path)) {
    issues.push_back("The resolved model path is missing on disk.");
  }

  return issues;
}

JobRequest build_request(const AppState &state) {
  const Settings &s = state.settings;
  JobRequest request;
  request.input_path = trim_copy(state.input_path.data());
  request.output_path = trim_copy(state.output_path.data());
  request.explicit_model_path = s.model_path;
  request.model_profile = s.model_profile;
  request.language = s.language.empty() ? "auto" : s.language;
  request.format = format_from_index(state.format_index);
  request.device = s.device;
  request.threads = s.engine_config("").threads;
  request.gpu_device = s.gpu_device;
  request.translate = s.translate;
  request.flash_attn = s.flash_attn;
  return request;
}

} // namespace gui
