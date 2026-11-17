#include "model_download.h"

#include "paths.h"
#include "process.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace model_download {

namespace {

bool safe_filename(const std::string &name) {
  if (name.empty() || name.size() > 128) {
    return false;
  }
  return std::all_of(name.begin(), name.end(),
                     [](char c) {
                       return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                              (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.';
                     }) &&
         name.find("..") == std::string::npos;
}

} // namespace

std::string model_url(const std::string &filename) {
  return "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/" + filename;
}

bool looks_like_ggml_model(const std::string &path) {
  std::ifstream in(path, std::ios::binary);
  uint32_t magic = 0;
  in.read(reinterpret_cast<char *>(&magic), sizeof(magic));
  if (!in) {
    return false;
  }
  // 'ggml' little-endian (0x67676d6c) as written by whisper.cpp's converter.
  return magic == 0x67676d6cu;
}

std::string download_profile(const ModelProfile &profile, std::string &error,
                             const std::string &dest_dir) {
  if (!safe_filename(profile.filename)) {
    error = "Refusing to download model with unsafe file name";
    return "";
  }

  const std::string dir = dest_dir.empty() ? paths::models_dir() : dest_dir;
  if (!paths::ensure_private_dir(dir)) {
    error = "Could not create models directory: " + dir;
    return "";
  }

  if (!proc::program_available("curl", "--version")) {
    error = "curl is required to download models. Install curl or download " +
            model_url(profile.filename) + " manually into " + dir;
    return "";
  }

  const fs::path final_path = fs::path(dir) / profile.filename;
  const fs::path part_path = fs::path(dir) / (profile.filename + ".part");
  std::error_code ec;
  fs::remove(part_path, ec);

  const std::vector<std::string> argv = {
      "curl",
      "--fail",
      "--location",
      "--proto",
      "=https",
      "--proto-redir",
      "=https",
      "--tlsv1.2",
      "--retry",
      "3",
      "--progress-bar",
      "--output",
      part_path.string(),
      model_url(profile.filename),
  };

  proc::RunOptions options;
  options.silence_output = false; // show curl's progress bar
  const int code = proc::run(argv, options);
  if (code != 0) {
    fs::remove(part_path, ec);
    error = "Download failed (curl exit code " + std::to_string(code) + ")";
    return "";
  }

  if (!looks_like_ggml_model(part_path.string())) {
    fs::remove(part_path, ec);
    error = "Downloaded file is not a valid GGML model";
    return "";
  }

  fs::rename(part_path, final_path, ec);
  if (ec) {
    fs::remove(part_path, ec);
    error = "Could not move model into place: " + ec.message();
    return "";
  }
  return final_path.string();
}

} // namespace model_download
