#pragma once

#include <string>
#include <vector>

namespace media {

// supported audio extensions
inline bool is_audio_file(const std::string &path) {
  const std::vector<std::string> exts = {".wav", ".mp3", ".flac", ".ogg",
                                         ".m4a", ".aac", ".wma"};
  for (const auto &ext : exts) {
    if (path.size() >= ext.size() &&
        path.compare(path.size() - ext.size(), ext.size(), ext) == 0) {
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
    if (path.size() >= ext.size() &&
        path.compare(path.size() - ext.size(), ext.size(), ext) == 0) {
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
                          const std::string &tmp_dir = "/tmp");

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
