#pragma once

#include <string>
#include <vector>

struct ModelProfile {
  std::string name;
  std::string filename;
  std::string description;
};

const std::vector<ModelProfile> &all_model_profiles();

const ModelProfile *find_model_profile(const std::string &profile_name);

std::string resolve_model_path(const std::string &explicit_model_path,
                               const std::string &profile_name);
