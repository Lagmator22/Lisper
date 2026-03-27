#pragma once

#include <cctype>
#include <string>
#include <vector>

namespace media {

// Helper function for case-insensitive string comparison
inline bool ends_with_ignore_case(const std::string &str,
                                  const std::string &suffix) {
  if (str.size() < suffix.size()) {
    return false;
  }

  auto str_end = str.end();
  auto suffix_end = suffix.end();

  for (size_t i = 0; i < suffix.size(); ++i) {
    char c1 = static_cast<char>(
        std::tolower(static_cast<unsigned char>(*(str_end - 1 - i))));
    char c2 = static_cast<char>(
        std::tolower(static_cast<unsigned char>(*(suffix_end - 1 - i))));
    if (c1 != c2) {
      return false;
    }
  }
  return true;
}

// supported audio extensions
inline bool is_audio_file(const std::string &path) {
  const std::vector<std::string> exts = {
      ".wav", ".mp3", ".flac", ".ogg", ".m4a", ".aac", ".wma", ".aiff", ".aif"};
  for (const auto &ext : exts) {
    if (ends_with_ignore_case(path, ext)) {
      return true;
    }
  }
  return false;
}

// supported video extensions
inline bool is_video_file(const std::string &path) {
  const std::vector<std::string> exts = {".mp4", ".mkv", ".avi", ".webm",
                                         ".mov", ".flv", ".wmv", ".m4v"};
  for (const auto &ext : exts) {
    if (ends_with_ignore_case(path, ext)) {
      return true;
    }
  }
  return false;
}

inline bool is_media_file(const std::string &path) {
  return is_audio_file(path) || is_video_file(path);
}

// extract audio from a media file to a 16kHz mono WAV using ffmpeg.
// returns the path to the temporary wav file, or empty string on failure.
std::string extract_audio(const std::string &input_path,
                          const std::string &tmp_dir = "");

// prepare an input file for transcription. if it's already a wav, returns
// the original path. if it's a video or non-wav audio, extracts to a temp wav.
// the caller should delete the temp file when done.
struct PreparedAudio {
  std::string wav_path;
  bool is_temp = false;
  bool success = false;
  std::string error;
};

PreparedAudio prepare_audio(const std::string &input_path);

} // namespace media
