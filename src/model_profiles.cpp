#include "model_profiles.h"

#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

const std::vector<ModelProfile> &all_model_profiles() {
  static const std::vector<ModelProfile> kProfiles = {
      {"fast", "ggml-small-q5_1.bin", "Small multilingual model for fast drafts on CPU-only runs."},
      {"balanced", "ggml-medium-q5_0.bin", "Medium multilingual model for day-to-day transcripts."},
      {"quality", "ggml-large-v3-turbo-q5_0.bin",
       "Best quality/speed tradeoff for Apple Silicon and Windows CPUs."},
      {"max", "ggml-large-v3.bin",
       "Largest Whisper profile for maximum accuracy when latency is "
       "secondary."},
  };
  return kProfiles;
}

const ModelProfile *find_model_profile(const std::string &profile_name) {
  const auto &all = all_model_profiles();
  auto it = std::find_if(all.begin(), all.end(), [&profile_name](const auto &profile) {
    return profile.name == profile_name;
  });
  return it != all.end() ? &(*it) : nullptr;
}

std::string resolve_model_path(const std::string &explicit_model_path,
                               const std::string &profile_name) {
  if (!explicit_model_path.empty()) {
    return explicit_model_path;
  }

  const ModelProfile *profile = find_model_profile(profile_name);
  if (profile == nullptr) {
    return "";
  }

  const std::vector<fs::path> candidates = {
      fs::path("models") / profile->filename,
      fs::path("third_party/whisper.cpp/models") / profile->filename,
      fs::path(profile->filename),
      fs::path("../models") / profile->filename,
      fs::path("../third_party/whisper.cpp/models") / profile->filename,
  };

  for (const auto &candidate : candidates) {
    std::error_code ec;
    if (fs::exists(candidate, ec) && !ec) {
      return candidate.string();
    }
  }

  return "";
}
