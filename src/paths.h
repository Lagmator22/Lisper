#pragma once

#include <string>

// Per-user locations for configuration, data and models.
//
//   Linux:   $XDG_CONFIG_HOME/lisper, $XDG_DATA_HOME/lisper
//   macOS:   ~/Library/Application Support/Lisper
//   Windows: %APPDATA%\Lisper
//
// LISPER_HOME overrides the base directory on every platform and
// LISPER_MODELS_DIR overrides only the models directory.
namespace paths {

std::string config_dir();
std::string data_dir();
std::string models_dir();

// Create a directory (and parents) with owner-only permissions where the
// platform supports it. Returns false on failure.
bool ensure_private_dir(const std::string &dir);

// Write `content` to `path` atomically (temp file + rename) with owner-only
// permissions. Returns false on failure.
bool write_private_file(const std::string &path, const std::string &content);

} // namespace paths
