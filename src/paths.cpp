#include "paths.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>

#ifndef _WIN32
#include <sys/stat.h>
#endif

namespace fs = std::filesystem;

namespace paths {

namespace {

std::string env(const char *name) {
#ifdef _WIN32
  char *value = nullptr;
  size_t len = 0;
  if (_dupenv_s(&value, &len, name) != 0 || value == nullptr) {
    return "";
  }
  std::string out(value);
  free(value);
  return out;
#else
  const char *value = std::getenv(name);
  return value == nullptr ? "" : value;
#endif
}

fs::path home_dir() {
#ifdef _WIN32
  std::string h = env("USERPROFILE");
#else
  std::string h = env("HOME");
#endif
  return h.empty() ? fs::current_path() : fs::path(h);
}

fs::path base_config() {
  const std::string override_home = env("LISPER_HOME");
  if (!override_home.empty()) {
    return fs::path(override_home);
  }
#if defined(_WIN32)
  const std::string appdata = env("APPDATA");
  return (appdata.empty() ? home_dir() / "AppData" / "Roaming" : fs::path(appdata)) / "Lisper";
#elif defined(__APPLE__)
  return home_dir() / "Library" / "Application Support" / "Lisper";
#else
  const std::string xdg = env("XDG_CONFIG_HOME");
  return (xdg.empty() ? home_dir() / ".config" : fs::path(xdg)) / "lisper";
#endif
}

fs::path base_data() {
  const std::string override_home = env("LISPER_HOME");
  if (!override_home.empty()) {
    return fs::path(override_home);
  }
#if defined(_WIN32) || defined(__APPLE__)
  return base_config();
#else
  const std::string xdg = env("XDG_DATA_HOME");
  return (xdg.empty() ? home_dir() / ".local" / "share" : fs::path(xdg)) / "lisper";
#endif
}

} // namespace

std::string config_dir() {
  return base_config().string();
}

std::string data_dir() {
  return base_data().string();
}

std::string models_dir() {
  const std::string override_models = env("LISPER_MODELS_DIR");
  if (!override_models.empty()) {
    return override_models;
  }
  return (base_data() / "models").string();
}

bool ensure_private_dir(const std::string &dir) {
  std::error_code ec;
  fs::create_directories(dir, ec);
  if (ec) {
    return false;
  }
#ifndef _WIN32
  ::chmod(dir.c_str(), 0700);
#endif
  return fs::is_directory(dir, ec);
}

bool write_private_file(const std::string &path, const std::string &content) {
  const fs::path target(path);
  if (target.has_parent_path() && !ensure_private_dir(target.parent_path().string())) {
    return false;
  }

  const fs::path tmp = target.string() + ".tmp";
  {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out) {
      return false;
    }
    out << content;
    if (!out) {
      return false;
    }
  }
#ifndef _WIN32
  ::chmod(tmp.c_str(), 0600);
#endif
  std::error_code ec;
  fs::rename(tmp, target, ec);
  if (ec) {
    fs::remove(tmp, ec);
    return false;
  }
  return true;
}

} // namespace paths
