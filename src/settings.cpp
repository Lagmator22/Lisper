#include "settings.h"

#include "paths.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>

namespace fs = std::filesystem;
using nlohmann::json;

namespace {

const char *device_name(LisperConfig::Device d) {
  switch (d) {
  case LisperConfig::Device::CPU:
    return "cpu";
  case LisperConfig::Device::GPU:
    return "gpu";
  default:
    return "auto";
  }
}

LisperConfig::Device device_from(const std::string &s) {
  if (s == "cpu") {
    return LisperConfig::Device::CPU;
  }
  if (s == "gpu") {
    return LisperConfig::Device::GPU;
  }
  return LisperConfig::Device::Auto;
}

template <typename T> void read(const json &j, const char *key, T &out) {
  auto it = j.find(key);
  if (it == j.end()) {
    return;
  }
  try {
    out = it->get<T>();
  } catch (const json::exception &) {
    // keep default on type mismatch
  }
}

int auto_threads() {
  // Leave a core for the UI and audio threads so the app stays responsive.
  const unsigned hc = std::thread::hardware_concurrency();
  const unsigned usable = hc > 2 ? hc - 1 : 2u;
  return static_cast<int>(std::clamp(hc == 0 ? 4u : usable, 2u, 8u));
}

} // namespace

text_cleanup::Options Settings::cleanup_options() const {
  text_cleanup::Options o;
  o.remove_fillers = remove_fillers;
  o.voice_commands = voice_commands;
  o.style = style;
  o.dictionary = dictionary;
  o.snippets = snippets;
  return o;
}

LisperConfig Settings::engine_config(const std::string &resolved_model_path) const {
  LisperConfig c;
  c.model_path = resolved_model_path;
  c.language = language.empty() ? "auto" : language;
  c.translate = translate;
  c.device = device;
  c.threads = threads > 0 ? std::min(threads, 256) : auto_threads();
  c.gpu_device = gpu_device;
  c.flash_attn = flash_attn;
  c.initial_prompt = text_cleanup::vocabulary_prompt(dictionary, vocabulary);
  return c;
}

namespace settings_store {

std::string settings_path() {
  return (fs::path(paths::config_dir()) / "settings.json").string();
}

std::string to_json(const Settings &s) {
  json dict = json::array();
  for (const auto &r : s.dictionary) {
    dict.push_back({{"from", r.from}, {"to", r.to}});
  }
  json snippets = json::array();
  for (const auto &sn : s.snippets) {
    snippets.push_back({{"trigger", sn.trigger}, {"text", sn.text}});
  }

  json j = {
      {"version", 1},
      {"model_profile", s.model_profile},
      {"model_path", s.model_path},
      {"language", s.language},
      {"translate", s.translate},
      {"device", device_name(s.device)},
      {"threads", s.threads},
      {"gpu_device", s.gpu_device},
      {"flash_attn", s.flash_attn},
      {"hotkey", s.hotkey},
      {"hotkey_hold", s.hotkey_hold},
      {"input_device", s.input_device},
      {"insert_mode", s.insert_mode == Settings::InsertMode::Paste ? "paste" : "clipboard"},
      {"restore_clipboard", s.restore_clipboard},
      {"show_overlay", s.show_overlay},
      {"live_preview", s.live_preview},
      {"island_top", s.island_top},
      {"sounds", s.sounds},
      {"max_dictation_seconds", s.max_dictation_seconds},
      {"remove_fillers", s.remove_fillers},
      {"voice_commands", s.voice_commands},
      {"style", s.style == text_cleanup::Style::Casual ? "casual" : "formal"},
      {"dictionary", dict},
      {"vocabulary", s.vocabulary},
      {"snippets", snippets},
      {"dark_theme", s.dark_theme},
      {"close_to_background", s.close_to_background},
      {"save_history", s.save_history},
      {"history_limit", s.history_limit},
  };
  return j.dump(2);
}

Settings from_json(const std::string &text) {
  Settings s;
  const json j = json::parse(text, nullptr, false);
  if (j.is_discarded() || !j.is_object()) {
    return s;
  }

  read(j, "model_profile", s.model_profile);
  read(j, "model_path", s.model_path);
  read(j, "language", s.language);
  read(j, "translate", s.translate);
  std::string device = "auto";
  read(j, "device", device);
  s.device = device_from(device);
  read(j, "threads", s.threads);
  read(j, "gpu_device", s.gpu_device);
  read(j, "flash_attn", s.flash_attn);
  read(j, "hotkey", s.hotkey);
  read(j, "hotkey_hold", s.hotkey_hold);
  read(j, "input_device", s.input_device);
  std::string insert = "paste";
  read(j, "insert_mode", insert);
  s.insert_mode =
      insert == "clipboard" ? Settings::InsertMode::Clipboard : Settings::InsertMode::Paste;
  read(j, "restore_clipboard", s.restore_clipboard);
  read(j, "show_overlay", s.show_overlay);
  read(j, "live_preview", s.live_preview);
  read(j, "island_top", s.island_top);
  read(j, "sounds", s.sounds);
  read(j, "max_dictation_seconds", s.max_dictation_seconds);
  read(j, "remove_fillers", s.remove_fillers);
  read(j, "voice_commands", s.voice_commands);
  std::string style = "formal";
  read(j, "style", style);
  s.style = style == "casual" ? text_cleanup::Style::Casual : text_cleanup::Style::Formal;
  read(j, "vocabulary", s.vocabulary);
  read(j, "dark_theme", s.dark_theme);
  read(j, "close_to_background", s.close_to_background);
  read(j, "save_history", s.save_history);
  read(j, "history_limit", s.history_limit);

  if (auto it = j.find("dictionary"); it != j.end() && it->is_array()) {
    for (const auto &e : *it) {
      if (e.is_object() && e.contains("from") && e.contains("to") && e["from"].is_string() &&
          e["to"].is_string()) {
        s.dictionary.push_back({e["from"].get<std::string>(), e["to"].get<std::string>()});
      }
    }
  }
  if (auto it = j.find("snippets"); it != j.end() && it->is_array()) {
    for (const auto &e : *it) {
      if (e.is_object() && e.contains("trigger") && e.contains("text") &&
          e["trigger"].is_string() && e["text"].is_string()) {
        s.snippets.push_back({e["trigger"].get<std::string>(), e["text"].get<std::string>()});
      }
    }
  }

  // Clamp values that come from a user-editable file.
  s.threads = std::clamp(s.threads, 0, 256);
  s.gpu_device = std::max(0, s.gpu_device);
  s.max_dictation_seconds = std::clamp(s.max_dictation_seconds, 5, 3600);
  s.history_limit = std::clamp(s.history_limit, 0, 100000);
  return s;
}

Settings load_from(const std::string &path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return Settings{};
  }
  std::ostringstream ss;
  ss << in.rdbuf();
  return from_json(ss.str());
}

Settings load() {
  return load_from(settings_path());
}

bool save_to(const Settings &settings, const std::string &path) {
  return paths::write_private_file(path, to_json(settings) + "\n");
}

bool save(const Settings &settings) {
  return save_to(settings, settings_path());
}

} // namespace settings_store
