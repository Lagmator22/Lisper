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

void copy_string(std::array<char, kPathBuffer> &target,
                 const std::string &value) {
  target.fill('\0');
  std::snprintf(target.data(), target.size(), "%s", value.c_str());
}

void copy_string(std::array<char, kTextBuffer> &target,
                 const std::string &value) {
  target.fill('\0');
  std::snprintf(target.data(), target.size(), "%s", value.c_str());
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

namespace {

std::string timestamp_now() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t raw = std::chrono::system_clock::to_time_t(now);
  std::tm local_time{};
#ifdef _WIN32
  localtime_s(&local_time, &raw);
#else
  localtime_r(&raw, &local_time);
#endif

  char buffer[16];
  std::strftime(buffer, sizeof(buffer), "%H:%M:%S", &local_time);
  return buffer;
}

} // namespace

void append_log(AppState &state, const std::string &message) {
  state.logs.push_back(timestamp_now() + "  " + message);
  if (state.logs.size() > 180) {
    state.logs.erase(
        state.logs.begin(),
        state.logs.begin() +
            static_cast<long>(state.logs.size() - static_cast<size_t>(180)));
  }
}

int recommended_threads() {
  const unsigned hc = std::thread::hardware_concurrency();
  if (hc == 0) {
    return 4;
  }
  return static_cast<int>(std::clamp(hc, 4u, 8u));
}

int max_threads_for_slider() {
  const unsigned hc = std::thread::hardware_concurrency();
  return static_cast<int>(std::max(8u, hc == 0 ? 8u : hc));
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

const std::vector<ModelProfile> &profiles() { return all_model_profiles(); }

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

const char *format_label(formatter::Format format) {
  switch (format) {
  case formatter::Format::SRT:
    return "SRT";
  case formatter::Format::JSON:
    return "JSON";
  case formatter::Format::RAG:
    return "RAG";
  default:
    return "Text";
  }
}

LisperConfig::Device device_from_index(int index) {
  switch (index) {
  case 1:
    return LisperConfig::Device::CPU;
  case 2:
    return LisperConfig::Device::GPU;
  default:
    return LisperConfig::Device::Auto;
  }
}

std::string describe_device(LisperConfig::Device device) {
  switch (device) {
  case LisperConfig::Device::CPU:
    return "CPU";
  case LisperConfig::Device::GPU:
    return "GPU";
  default:
    return "AUTO";
  }
}

std::string current_profile_name(int profile_index) {
  const auto &all = profiles();
  if (profile_index < 0 || profile_index >= static_cast<int>(all.size())) {
    return "quality";
  }
  return all[profile_index].name;
}

std::string current_profile_filename(int profile_index) {
  const auto &all = profiles();
  if (profile_index < 0 || profile_index >= static_cast<int>(all.size())) {
    return "ggml-large-v3-turbo-q5_0.bin";
  }
  return all[profile_index].filename;
}

std::string profile_description(int profile_index) {
  const auto &all = profiles();
  if (profile_index < 0 || profile_index >= static_cast<int>(all.size())) {
    return {};
  }
  return all[profile_index].description;
}

void refresh_resolved_model(AppState &state) {
  state.resolved_model_path =
      resolve_model_path(trim_copy(state.manual_model_path.data()),
                         current_profile_name(state.profile_index));
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
    issues.push_back("No model was resolved. Download a profile or enter a manual model path.");
  } else if (!path_exists(state.resolved_model_path)) {
    issues.push_back("The resolved model path is missing on disk.");
  }

  return issues;
}

JobRequest build_request(const AppState &state) {
  JobRequest request;
  request.input_path = trim_copy(state.input_path.data());
  request.output_path = trim_copy(state.output_path.data());
  request.explicit_model_path = trim_copy(state.manual_model_path.data());
  request.model_profile = current_profile_name(state.profile_index);
  request.language = trim_copy(state.language.data());
  if (request.language.empty()) {
    request.language = "en";
  }
  request.format = format_from_index(state.format_index);
  request.device = device_from_index(state.device_index);
  request.threads = state.threads;
  request.gpu_device = state.gpu_device;
  request.translate = state.translate;
  request.flash_attn = state.flash_attn;
  return request;
}

} // namespace gui
