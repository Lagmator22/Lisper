#pragma once

#include "model_profiles.h"

#include <string>

namespace model_download {

// Official whisper.cpp GGML model location for a profile file name.
std::string model_url(const std::string &filename);

// True when the file starts with a GGML magic number. Used to reject HTML
// error pages or truncated downloads before whisper tries to parse them.
bool looks_like_ggml_model(const std::string &path);

// Download a profile's model into `dest_dir` (defaults to paths::models_dir()).
// Uses curl over HTTPS only, writes to a temporary file and renames it into
// place after validation. Returns the final path, or an empty string and fills
// `error` on failure.
std::string download_profile(const ModelProfile &profile, std::string &error,
                             const std::string &dest_dir = "");

} // namespace model_download
