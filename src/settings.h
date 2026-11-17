#pragma once

#include "lisper.h"
#include "text_cleanup.h"

#include <string>
#include <vector>

// Persistent user settings shared by the desktop app, dictation and the MCP
// server. Stored as JSON in paths::config_dir()/settings.json (mode 0600).
struct Settings {
  // Model
  std::string model_profile = "quality";
  std::string model_path; // explicit override; empty = use profile
  std::string language = "auto";
  bool translate = false;
  LisperConfig::Device device = LisperConfig::Device::Auto;
  int threads = 0; // 0 = choose automatically
  int gpu_device = 0;
  bool flash_attn = true;

  // Dictation
#ifdef __APPLE__
  std::string hotkey = "Cmd+Option"; // tap ⌘ and ⌥ together
#else
  std::string hotkey = "Ctrl+Shift+Space";
#endif
  bool hotkey_hold = false; // true = push-to-talk, false = toggle
  std::string input_device; // empty = system default microphone
  enum class InsertMode { Paste, Clipboard } insert_mode = InsertMode::Paste;
  bool restore_clipboard = true;
  bool show_overlay = true;
  bool live_preview = true; // show words while speaking
  bool island_top = true;   // floating island at the top (notch) or bottom
  bool sounds = true;
  int max_dictation_seconds = 300;

  // Cleanup
  bool remove_fillers = true;
  bool voice_commands = true;
  text_cleanup::Style style = text_cleanup::Style::Formal;
  std::vector<text_cleanup::Replacement> dictionary;
  std::vector<std::string> vocabulary;
  std::vector<text_cleanup::Snippet> snippets;

  // App
  bool dark_theme = false;
  bool close_to_background = true; // closing the window keeps dictation running

  // Privacy
  bool save_history = true;
  int history_limit = 1000;

  text_cleanup::Options cleanup_options() const;
  LisperConfig engine_config(const std::string &resolved_model_path) const;
};

namespace settings_store {

std::string settings_path();

// Load settings; returns defaults if the file is missing or invalid.
Settings load();
Settings load_from(const std::string &path);

bool save(const Settings &settings);
bool save_to(const Settings &settings, const std::string &path);

std::string to_json(const Settings &settings);
Settings from_json(const std::string &json);

} // namespace settings_store
