#include "media.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <random>
#include <sstream>

namespace fs = std::filesystem;

namespace media {

static std::string generate_tmp_name(const std::string &tmp_dir) {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<int> dist(10000, 99999);
  return (fs::path(tmp_dir) / ("lisper_" + std::to_string(dist(gen)) + ".wav"))
      .string();
}

static std::string shell_quote(const std::string &path) {
#ifdef _WIN32
  std::string quoted = "\"";
  for (const char c : path) {
    if (c == '"') {
      quoted += "\\\"";
    } else {
      quoted += c;
    }
  }
  quoted += "\"";
  return quoted;
#else
  std::string quoted = "'";
  for (const char c : path) {
    if (c == '\'') {
      quoted += "'\\''";
    } else {
      quoted += c;
    }
  }
  quoted += "'";
  return quoted;
#endif
}

static std::string null_device() {
#ifdef _WIN32
  return "NUL";
#else
  return "/dev/null";
#endif
}

static bool ffmpeg_available() {
  const std::string cmd = "ffmpeg -version > " + null_device() + " 2>&1";
  int ret = std::system(cmd.c_str());
  return ret == 0;
}

std::string extract_audio(const std::string &input_path,
                          const std::string &tmp_dir) {
  if (!ffmpeg_available()) {
    std::cerr << "ffmpeg not found. Install FFmpeg and ensure `ffmpeg` is in "
                 "your PATH.\n";
    return "";
  }

  if (!fs::exists(input_path)) {
    std::cerr << "File not found: " << input_path << "\n";
    return "";
  }

  std::error_code ec;
  const fs::path tmp_root =
      tmp_dir.empty() ? fs::temp_directory_path(ec) : fs::path(tmp_dir);
  if (ec) {
    std::cerr << "Failed to resolve temporary directory: " << ec.message()
              << "\n";
    return "";
  }

  fs::create_directories(tmp_root, ec);
  if (ec) {
    std::cerr << "Failed to create temporary directory: " << ec.message()
              << "\n";
    return "";
  }

  std::string out_path = generate_tmp_name(tmp_root.string());
  std::string safe_input = shell_quote(input_path);
  std::string safe_output = shell_quote(out_path);

  // convert to 16kHz mono 16-bit PCM WAV
  std::ostringstream cmd;
  cmd << "ffmpeg -nostdin -y -i " << safe_input << " "
      << "-ar 16000 -ac 1 -sample_fmt s16 "
      << "-f wav " << safe_output << " "
      << "-loglevel error > " << null_device() << " 2>&1";

  int ret = std::system(cmd.str().c_str());
  if (ret != 0) {
    std::cerr << "ffmpeg extraction failed for: " << input_path << "\n";
    return "";
  }

  return out_path;
}

PreparedAudio prepare_audio(const std::string &input_path) {
  PreparedAudio result;

  if (!fs::exists(input_path)) {
    result.error = "File not found: " + input_path;
    return result;
  }

  std::string ext = fs::path(input_path).extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });

  // if already a wav, use it directly
  if (ext == ".wav") {
    result.wav_path = input_path;
    result.is_temp = false;
    result.success = true;
    return result;
  }

  // for everything else, extract audio via ffmpeg
  if (!is_media_file(input_path)) {
    result.error = "Unsupported file format: " + ext;
    return result;
  }

  std::string wav_path = extract_audio(input_path);
  if (wav_path.empty()) {
    result.error = "Failed to extract audio from: " + input_path;
    return result;
  }

  result.wav_path = wav_path;
  result.is_temp = true;
  result.success = true;
  return result;
}

} // namespace media
