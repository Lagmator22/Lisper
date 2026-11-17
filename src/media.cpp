#include "media.h"

#include "process.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <share.h>
#include <sys/stat.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace media {

namespace {

std::string random_hex(size_t bytes) {
  static const char *kHex = "0123456789abcdef";
  std::random_device rd;
  std::string out;
  out.reserve(bytes * 2);
  for (size_t i = 0; i < bytes; ++i) {
    const unsigned v = rd() & 0xFFu;
    out.push_back(kHex[v >> 4]);
    out.push_back(kHex[v & 0xF]);
  }
  return out;
}

// Atomically create a new, empty file readable only by the current user.
// Fails instead of following a pre-planted file or symlink.
bool create_exclusive_file(const std::string &path) {
#ifdef _WIN32
  int fd = -1;
  if (_sopen_s(&fd, path.c_str(), _O_CREAT | _O_EXCL | _O_WRONLY | _O_BINARY, _SH_DENYNO,
               _S_IREAD | _S_IWRITE) != 0) {
    return false;
  }
  _close(fd);
  return true;
#else
  const int fd = ::open(path.c_str(), O_CREAT | O_EXCL | O_WRONLY | O_NOFOLLOW | O_CLOEXEC, 0600);
  if (fd < 0) {
    return false;
  }
  ::close(fd);
  return true;
#endif
}

} // namespace

std::string create_secure_temp_file(const std::string &tmp_dir, const std::string &suffix) {
  std::error_code ec;
  const fs::path root = tmp_dir.empty() ? fs::temp_directory_path(ec) : fs::path(tmp_dir);
  if (ec) {
    return "";
  }
  fs::create_directories(root, ec);
  if (ec) {
    return "";
  }

  for (int attempt = 0; attempt < 16; ++attempt) {
    const std::string candidate = (root / ("lisper_" + random_hex(16) + suffix)).string();
    if (create_exclusive_file(candidate)) {
      return candidate;
    }
  }
  return "";
}

std::string ffmpeg_input_arg(const std::string &input_path) {
  // Prefixing with the file: protocol stops ffmpeg from interpreting names such
  // as "http://...", "concat:a|b", "subfile:..." or "-" (stdin) as anything other
  // than a plain local path.
  std::error_code ec;
  fs::path abs = fs::absolute(input_path, ec);
  if (ec) {
    abs = fs::path(input_path);
  }
  return "file:" + abs.string();
}

bool probe_wav(const std::string &path, WavInfo &info) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return false;
  }

  char riff[12];
  file.read(riff, 12);
  if (!file || std::memcmp(riff, "RIFF", 4) != 0 || std::memcmp(riff + 8, "WAVE", 4) != 0) {
    return false;
  }

  for (int i = 0; i < 64 && file; ++i) {
    char id[4];
    uint32_t size = 0;
    file.read(id, 4);
    file.read(reinterpret_cast<char *>(&size), 4);
    if (!file) {
      return false;
    }
    if (std::memcmp(id, "fmt ", 4) == 0) {
      if (size < 16) {
        return false;
      }
      uint16_t fmt = 0;
      uint16_t channels = 0;
      uint32_t rate = 0;
      uint32_t byte_rate = 0;
      uint16_t align = 0;
      uint16_t bits = 0;
      file.read(reinterpret_cast<char *>(&fmt), 2);
      file.read(reinterpret_cast<char *>(&channels), 2);
      file.read(reinterpret_cast<char *>(&rate), 4);
      file.read(reinterpret_cast<char *>(&byte_rate), 4);
      file.read(reinterpret_cast<char *>(&align), 2);
      file.read(reinterpret_cast<char *>(&bits), 2);
      if (!file) {
        return false;
      }
      info.audio_format = fmt;
      info.channels = channels;
      info.sample_rate = static_cast<int>(rate);
      info.bits_per_sample = bits;
      return true;
    }
    file.seekg(static_cast<std::streamoff>(size) + (size % 2), std::ios::cur);
  }
  return false;
}

bool wav_is_whisper_ready(const WavInfo &info) {
  return info.audio_format == 1 && info.bits_per_sample == 16 && info.sample_rate == 16000 &&
         (info.channels == 1 || info.channels == 2);
}

std::string extract_audio(const std::string &input_path, const std::string &tmp_dir) {
  if (!proc::program_available("ffmpeg")) {
    std::cerr << "ffmpeg not found. Install FFmpeg and ensure `ffmpeg` is in "
                 "your PATH.\n";
    return "";
  }

  std::error_code ec;
  if (!fs::is_regular_file(input_path, ec)) {
    std::cerr << "File not found: " << input_path << "\n";
    return "";
  }

  const std::string out_path = create_secure_temp_file(tmp_dir, ".wav");
  if (out_path.empty()) {
    std::cerr << "Failed to create a temporary file for audio extraction\n";
    return "";
  }

  // Convert to 16kHz mono 16-bit PCM WAV. Arguments are passed directly to the
  // ffmpeg process, never through a shell.
  const std::vector<std::string> argv = {
      "ffmpeg",
      "-nostdin",
      "-hide_banner",
      "-loglevel",
      "error",
      "-y",
      "-protocol_whitelist",
      "file",
      "-i",
      ffmpeg_input_arg(input_path),
      "-vn",
      "-ar",
      "16000",
      "-ac",
      "1",
      "-sample_fmt",
      "s16",
      "-f",
      "wav",
      "file:" + out_path,
  };

  const int ret = proc::run(argv);
  if (ret != 0) {
    std::cerr << "ffmpeg extraction failed for: " << input_path << "\n";
    fs::remove(out_path, ec);
    return "";
  }

  return out_path;
}

PreparedAudio prepare_audio(const std::string &input_path) {
  PreparedAudio result;

  std::error_code ec;
  if (!fs::exists(input_path, ec)) {
    result.error = "File not found: " + input_path;
    return result;
  }
  if (!fs::is_regular_file(input_path, ec)) {
    result.error = "Not a regular file: " + input_path;
    return result;
  }

  std::string ext = fs::path(input_path).extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

  if (!is_media_file(input_path)) {
    result.error = "Unsupported file format: " + ext;
    return result;
  }

  // Use WAV files directly only when they already match what whisper expects;
  // anything else (44.1kHz, float, 24-bit, ...) is resampled through ffmpeg.
  if (ext == ".wav") {
    WavInfo info;
    if (probe_wav(input_path, info) && wav_is_whisper_ready(info)) {
      result.wav_path = input_path;
      result.is_temp = false;
      result.success = true;
      return result;
    }
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
