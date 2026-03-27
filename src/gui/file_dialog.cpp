#include "gui/file_dialog.h"

#include "gui/state.h"

#include "tinyfiledialogs.h"

#include <filesystem>

namespace fs = std::filesystem;

namespace gui {

namespace {

std::string dialog_seed_path(const std::string &candidate) {
  if (candidate.empty()) {
    return fs::current_path().string();
  }

  std::error_code ec;
  const fs::path path(candidate);
  if (fs::exists(path, ec) && !ec) {
    if (fs::is_directory(path, ec) && !ec) {
      return path.string();
    }
    return path.string();
  }

  const fs::path parent = path.parent_path();
  if (!parent.empty() && fs::exists(parent, ec) && !ec) {
    return parent.string();
  }

  return fs::current_path().string();
}

std::optional<std::string> normalize_dialog_result(const char *result) {
  if (result == nullptr || *result == '\0') {
    return std::nullopt;
  }

  return fs::path(result).string();
}

} // namespace

std::optional<std::string>
pick_input_media_file(const std::string &initial_path) {
  static const char *patterns[] = {"*.wav", "*.mp3", "*.flac", "*.ogg", "*.m4a",
                                   "*.aac", "*.wma", "*.aiff", "*.aif", "*.mp4",
                                   "*.mkv", "*.avi", "*.webm", "*.mov", "*.flv",
                                   "*.wmv", "*.m4v"};

  const char *result = tinyfd_openFileDialog(
      "Choose media file", dialog_seed_path(initial_path).c_str(),
      static_cast<int>(std::size(patterns)), patterns, "Audio and video files",
      0);
  return normalize_dialog_result(result);
}

std::optional<std::string> pick_output_folder(const std::string &initial_path) {
  const char *result = tinyfd_selectFolderDialog(
      "Choose output folder", dialog_seed_path(initial_path).c_str());
  return normalize_dialog_result(result);
}

std::optional<std::string> pick_output_file(const std::string &suggested_path) {
  static const char *patterns[] = {"*.txt", "*.srt", "*.json"};
  const char *result = tinyfd_saveFileDialog(
      "Choose output file", dialog_seed_path(suggested_path).c_str(),
      static_cast<int>(std::size(patterns)), patterns, "Transcript files");
  return normalize_dialog_result(result);
}

} // namespace gui
