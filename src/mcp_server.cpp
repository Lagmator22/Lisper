#include "mcp_server.h"

#include "audio_capture.h"
#include "formatter.h"
#include "history.h"
#include "interrupt.h"
#include "lisper.h"
#include "media.h"
#include "model_download.h"
#include "model_profiles.h"
#include "paths.h"
#include "settings.h"
#include "text_cleanup.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <optional>
#include <thread>

namespace fs = std::filesystem;
using nlohmann::json;

#ifndef LISPER_VERSION
#define LISPER_VERSION "0.0.0"
#endif

namespace mcp {

namespace {

constexpr const char *kLatestProtocol = "2025-06-18";
const std::vector<std::string> kSupportedProtocols = {"2025-06-18", "2025-03-26", "2024-11-05"};

// JSON-RPC error codes
constexpr int kParseError = -32700;
constexpr int kInvalidRequest = -32600;
constexpr int kMethodNotFound = -32601;
constexpr int kInvalidParams = -32602;

struct ToolError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

std::string dump(const json &j) {
  // Whisper can emit partial UTF-8 sequences at token boundaries; replace them
  // instead of throwing while serialising.
  return j.dump(-1, ' ', false, json::error_handler_t::replace);
}

json rpc_error(const json &id, int code, const std::string &message) {
  return {{"jsonrpc", "2.0"}, {"id", id}, {"error", {{"code", code}, {"message", message}}}};
}

json rpc_result(const json &id, json result) {
  return {{"jsonrpc", "2.0"}, {"id", id}, {"result", std::move(result)}};
}

json text_content(const std::string &text) {
  return json::array({{{"type", "text"}, {"text", text}}});
}

json tool_failure(const std::string &message) {
  return {{"content", text_content(message)}, {"isError", true}};
}

std::string home_dir() {
#ifdef _WIN32
  const char *h = std::getenv("USERPROFILE");
#else
  const char *h = std::getenv("HOME");
#endif
  return h != nullptr ? h : "";
}

json segments_json(const TranscriptionResult &r) {
  json segs = json::array();
  for (const auto &s : r.segments) {
    segs.push_back({{"start_ms", s.start_ms}, {"end_ms", s.end_ms}, {"text", s.text}});
  }
  return segs;
}

std::string trim(const std::string &s) {
  const auto a = s.find_first_not_of(" \t\r\n");
  if (a == std::string::npos) {
    return "";
  }
  const auto b = s.find_last_not_of(" \t\r\n");
  return s.substr(a, b - a + 1);
}

template <typename T> T arg(const json &args, const char *key, T fallback) {
  auto it = args.find(key);
  if (it == args.end() || it->is_null()) {
    return fallback;
  }
  try {
    return it->get<T>();
  } catch (const json::exception &) {
    throw ToolError(std::string("Invalid type for argument '") + key + "'");
  }
}

} // namespace

// ---------------------------------------------------------------------------
// base64
// ---------------------------------------------------------------------------

std::string base64_decode(const std::string &in, bool &ok) {
  static const auto table = [] {
    std::array<int, 256> t{};
    t.fill(-1);
    const char *chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (int i = 0; i < 64; ++i) {
      t[static_cast<unsigned char>(chars[i])] = i;
    }
    t['-'] = 62; // url-safe alphabet
    t['_'] = 63;
    return t;
  }();

  std::string out;
  out.reserve(in.size() * 3 / 4);
  int val = 0;
  int bits = -8;
  for (unsigned char c : in) {
    if (c == '=' || c == '\n' || c == '\r' || c == ' ') {
      continue;
    }
    const int d = table[c];
    if (d < 0) {
      ok = false;
      return "";
    }
    val = (val << 6) | d;
    bits += 6;
    if (bits >= 0) {
      out.push_back(static_cast<char>((val >> bits) & 0xFF));
      bits -= 8;
    }
  }
  ok = true;
  return out;
}

// ---------------------------------------------------------------------------
// Server implementation
// ---------------------------------------------------------------------------

struct Server::Impl {
  ServerOptions options;
  Settings settings;
  std::vector<fs::path> roots;

  std::mutex engine_mutex;
  std::unique_ptr<Lisper> engine;
  std::string engine_model_path;

  explicit Impl(ServerOptions opts) : options(std::move(opts)), settings(settings_store::load()) {
    if (!options.model_profile.empty()) {
      settings.model_profile = options.model_profile;
      settings.model_path.clear();
    }
    if (!options.model_path.empty()) {
      settings.model_path = options.model_path;
    }
    if (!options.language.empty()) {
      settings.language = options.language;
    }
    if (options.threads > 0) {
      settings.threads = options.threads;
    }

    std::vector<std::string> dirs = options.allowed_dirs;
    if (dirs.empty() && !home_dir().empty()) {
      dirs.push_back(home_dir());
    }
    for (const auto &d : dirs) {
      std::error_code ec;
      const fs::path canon = fs::canonical(d, ec);
      if (!ec && fs::is_directory(canon, ec)) {
        roots.push_back(canon);
      } else {
        std::cerr << "lisper-mcp: ignoring allowed directory that does not exist: " << d << "\n";
      }
    }
  }

  // -- helpers --------------------------------------------------------------

  bool path_allowed(const std::string &path, std::string &resolved) const {
    if (path.empty() || path.size() > 4096) {
      return false;
    }
    std::error_code ec;
    const fs::path canon = fs::canonical(path, ec);
    if (ec) {
      return false;
    }
    for (const auto &root : roots) {
      auto r = root.begin();
      auto p = canon.begin();
      for (; r != root.end() && p != canon.end(); ++r, ++p) {
        if (*r != *p) {
          break;
        }
      }
      if (r == root.end()) {
        resolved = canon.string();
        return true;
      }
    }
    return false;
  }

  std::string resolve_model() const {
    if (!settings.model_path.empty()) {
      return settings.model_path;
    }
    return resolve_model_path("", settings.model_profile);
  }

  Lisper &get_engine() {
    const std::string model = resolve_model();
    if (model.empty() || !fs::exists(model)) {
      std::string hint = "No model found for profile '" + settings.model_profile + "'. ";
      hint += options.allow_download ? "Call the download_model tool, or "
                                     : "Run `lisper --model-profile " + settings.model_profile +
                                           " --download-model`, or ";
      hint += "place the model in " + paths::models_dir();
      throw ToolError(hint);
    }
    if (!engine || engine_model_path != model) {
      engine.reset();
      std::cerr << "lisper-mcp: loading model " << model << "\n";
      auto e = std::make_unique<Lisper>(settings.engine_config(model));
      if (!e->is_loaded()) {
        throw ToolError("Failed to load model: " + model);
      }
      engine = std::move(e);
      engine_model_path = model;
    }
    return *engine;
  }

  DecodeOptions decode_options(const json &args) const {
    DecodeOptions o;
    o.language = trim(arg<std::string>(args, "language", ""));
    if (o.language.size() > 16) {
      throw ToolError("Invalid language code");
    }
    if (args.contains("translate")) {
      o.translate = arg<bool>(args, "translate", false) ? 1 : 0;
    }
    return o;
  }

  json result_payload(const TranscriptionResult &r, const std::string &source,
                      const std::string &format, bool cleanup) {
    std::string text = r.full_text;
    if (cleanup) {
      text = text_cleanup::clean(text, settings.cleanup_options());
    } else {
      text = trim(text);
    }

    std::string rendered;
    if (format == "srt") {
      rendered = formatter::format_result(r, formatter::Format::SRT, source);
    } else if (format == "json") {
      rendered = formatter::format_result(r, formatter::Format::JSON, source);
    } else {
      rendered = text;
    }
    if (rendered.empty()) {
      rendered = "(no speech detected)";
    }

    json structured = {{"text", text},
                       {"language", r.detected_language},
                       {"duration_ms", r.duration_ms},
                       {"segments", segments_json(r)}};
    if (!source.empty()) {
      structured["source"] = source;
    }
    return {
        {"content", text_content(rendered)}, {"structuredContent", structured}, {"isError", false}};
  }

  static std::string check_format(const json &args) {
    const std::string format = arg<std::string>(args, "format", "text");
    if (format != "text" && format != "srt" && format != "json") {
      throw ToolError("format must be one of: text, srt, json");
    }
    return format;
  }

  TranscriptionResult run_file(const std::string &wav_or_media, const DecodeOptions &opts) {
    auto prepared = media::prepare_audio(wav_or_media);
    if (!prepared.success) {
      throw ToolError(prepared.error);
    }
    struct Cleanup {
      std::string path;
      bool del;
      ~Cleanup() {
        if (del) {
          std::error_code ec;
          fs::remove(path, ec);
        }
      }
    } guard{prepared.wav_path, prepared.is_temp};

    std::lock_guard<std::mutex> lock(engine_mutex);
    auto r = get_engine().transcribe(prepared.wav_path, opts);
    if (!r.success) {
      throw ToolError(interrupt_state::is_interrupted() ? "Cancelled" : r.error);
    }
    return r;
  }

  // -- tools ----------------------------------------------------------------

  json tool_transcribe_file(const json &args) {
    const std::string path = arg<std::string>(args, "path", "");
    if (path.empty()) {
      throw ToolError("'path' is required");
    }
    std::string resolved;
    if (!path_allowed(path, resolved)) {
      throw ToolError("Access denied or file not found: " + path +
                      ". The file must exist inside an allowed directory (start lisper-mcp "
                      "with --allow-dir to change this).");
    }
    if (!media::is_media_file(resolved)) {
      throw ToolError("Unsupported file type. Supported: wav mp3 flac ogg m4a aac wma aiff mp4 "
                      "mkv avi webm mov flv wmv m4v");
    }
    const std::string format = check_format(args);
    const bool cleanup = arg<bool>(args, "cleanup", false);
    const auto r = run_file(resolved, decode_options(args));
    return result_payload(r, fs::path(resolved).filename().string(), format, cleanup);
  }

  json tool_transcribe_audio(const json &args) {
    const std::string data = arg<std::string>(args, "data", "");
    if (data.empty()) {
      throw ToolError("'data' (base64 audio) is required");
    }
    if (data.size() / 4 * 3 > options.max_audio_bytes) {
      throw ToolError("Audio payload too large");
    }
    std::string ext = arg<std::string>(args, "extension", "wav");
    if (!ext.empty() && ext[0] == '.') {
      ext.erase(0, 1);
    }
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (!media::is_media_file("x." + ext)) {
      throw ToolError("Unsupported audio extension: " + ext);
    }

    bool ok = false;
    const std::string bytes = base64_decode(data, ok);
    if (!ok || bytes.empty()) {
      throw ToolError("'data' is not valid base64");
    }

    const std::string tmp = media::create_secure_temp_file("", "." + ext);
    if (tmp.empty()) {
      throw ToolError("Could not create a temporary file");
    }
    struct Cleanup {
      std::string path;
      ~Cleanup() {
        std::error_code ec;
        fs::remove(path, ec);
      }
    } guard{tmp};
    {
      std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
      out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
      if (!out) {
        throw ToolError("Could not write the temporary audio file");
      }
    }

    const std::string format = check_format(args);
    const bool cleanup = arg<bool>(args, "cleanup", false);
    const auto r = run_file(tmp, decode_options(args));
    return result_payload(r, "", format, cleanup);
  }

  json tool_record(const json &args) {
    const int seconds = arg<int>(args, "seconds", 5);
    if (seconds < 1 || seconds > 120) {
      throw ToolError("seconds must be between 1 and 120");
    }
    const bool cleanup = arg<bool>(args, "cleanup", true);

    AudioCapture capture;
    std::string error;
    if (!capture.start(settings.input_device, seconds + 1, error)) {
      throw ToolError(error);
    }
    std::cerr << "lisper-mcp: recording microphone for " << seconds << "s\n";
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    while (std::chrono::steady_clock::now() < deadline && !interrupt_state::is_interrupted()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    const auto pcm = capture.stop();
    if (interrupt_state::is_interrupted()) {
      throw ToolError("Cancelled");
    }
    if (pcm.size() < 1600) {
      throw ToolError("No audio was captured from the microphone");
    }

    std::lock_guard<std::mutex> lock(engine_mutex);
    auto r = get_engine().transcribe_pcm(pcm, decode_options(args));
    if (!r.success) {
      throw ToolError(r.error);
    }
    return result_payload(r, "microphone", "text", cleanup);
  }

  json tool_clean_text(const json &args) {
    const std::string text = arg<std::string>(args, "text", "");
    if (text.size() > 1024 * 1024) {
      throw ToolError("text too long");
    }
    const std::string cleaned = text_cleanup::clean(text, settings.cleanup_options());
    return {{"content", text_content(cleaned)},
            {"structuredContent", {{"text", cleaned}}},
            {"isError", false}};
  }

  json tool_list_models(const json &) {
    json models = json::array();
    const std::string active = resolve_model();
    for (const auto &p : all_model_profiles()) {
      const std::string path = resolve_model_path("", p.name);
      models.push_back({{"profile", p.name},
                        {"filename", p.filename},
                        {"description", p.description},
                        {"installed", !path.empty()},
                        {"path", path}});
    }
    json structured = {{"models", models},
                       {"active_profile", settings.model_profile},
                       {"active_model_path", active},
                       {"models_dir", paths::models_dir()}};
    std::string text = "Models directory: " + paths::models_dir() + "\n";
    for (const auto &m : models) {
      text += "- " + m["profile"].get<std::string>() + " (" + m["filename"].get<std::string>() +
              "): " + (m["installed"].get<bool>() ? "installed" : "not installed") + "\n";
    }
    text += "Active: " + (active.empty() ? std::string("none") : active);
    return {{"content", text_content(text)}, {"structuredContent", structured}, {"isError", false}};
  }

  json tool_download(const json &args) {
    const std::string profile_name = arg<std::string>(args, "profile", settings.model_profile);
    const ModelProfile *profile = find_model_profile(profile_name);
    if (profile == nullptr) {
      throw ToolError("Unknown profile: " + profile_name);
    }
    if (!resolve_model_path("", profile_name).empty()) {
      return {{"content", text_content("Model already installed for profile " + profile_name)},
              {"isError", false}};
    }
    std::string error;
    const std::string path = model_download::download_profile(*profile, error);
    if (path.empty()) {
      throw ToolError(error);
    }
    return {{"content", text_content("Downloaded " + path)}, {"isError", false}};
  }

  json tool_history(const json &args) {
    const int limit = std::clamp(arg<int>(args, "limit", 20), 1, 200);
    std::string query = arg<std::string>(args, "query", "");
    std::transform(query.begin(), query.end(), query.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    json items = json::array();
    std::string text;
    for (const auto &e : history::load(0)) {
      if (!query.empty()) {
        std::string lower = e.text;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (lower.find(query) == std::string::npos) {
          continue;
        }
      }
      items.push_back({{"id", e.id},
                       {"timestamp_ms", e.timestamp_ms},
                       {"text", e.text},
                       {"duration_ms", e.duration_ms},
                       {"words", e.words}});
      text += "- " + e.text + "\n";
      if (static_cast<int>(items.size()) >= limit) {
        break;
      }
    }
    if (items.empty()) {
      text = "No matching dictation history.";
    }
    return {{"content", text_content(text)},
            {"structuredContent", {{"entries", items}}},
            {"isError", false}};
  }

  // -- tool registry --------------------------------------------------------

  json tool_list() const {
    const json lang = {{"type", "string"},
                       {"description", "Language code such as en, de, ja, or auto to detect. "
                                       "Defaults to the user's setting."}};
    const json translate = {{"type", "boolean"},
                            {"description", "Translate the speech into English."}};
    const json format = {{"type", "string"},
                         {"enum", {"text", "srt", "json"}},
                         {"description", "Output format for the text content (default text)."}};
    const json cleanup = {{"type", "boolean"},
                          {"description", "Apply dictation cleanup: remove filler words, apply the "
                                          "user's dictionary and snippets."}};
    const json read_only = {{"readOnlyHint", true}, {"openWorldHint", false}};

    json tools = json::array();
    tools.push_back(
        {{"name", "transcribe_file"},
         {"title", "Transcribe audio/video file"},
         {"description", "Transcribe a local audio or video file on this machine with Whisper. "
                         "Runs fully offline. Returns the transcript plus timestamped segments."},
         {"inputSchema",
          {{"type", "object"},
           {"properties",
            {{"path", {{"type", "string"}, {"description", "Absolute path to the media file."}}},
             {"language", lang},
             {"translate", translate},
             {"format", format},
             {"cleanup", cleanup}}},
           {"required", {"path"}}}},
         {"annotations", read_only}});
    tools.push_back(
        {{"name", "transcribe_audio"},
         {"title", "Transcribe audio data"},
         {"description", "Transcribe base64-encoded audio sent with the request (for audio that "
                         "is not on this machine's disk). Runs fully offline."},
         {"inputSchema",
          {{"type", "object"},
           {"properties",
            {{"data", {{"type", "string"}, {"description", "Base64-encoded audio bytes."}}},
             {"extension",
              {{"type", "string"},
               {"description",
                "Container/codec extension, e.g. wav, mp3, m4a, webm (default wav)."}}},
             {"language", lang},
             {"translate", translate},
             {"format", format},
             {"cleanup", cleanup}}},
           {"required", {"data"}}}},
         {"annotations", read_only}});
    if (options.allow_microphone && AudioCapture::supported()) {
      tools.push_back(
          {{"name", "record_and_transcribe"},
           {"title", "Dictate from microphone"},
           {"description", "Record from the user's microphone for a number of seconds, then "
                           "return the cleaned-up transcript. Only call this when the user asks "
                           "to dictate."},
           {"inputSchema",
            {{"type", "object"},
             {"properties",
              {{"seconds",
                {{"type", "integer"},
                 {"minimum", 1},
                 {"maximum", 120},
                 {"description", "Recording length in seconds (default 5)."}}},
               {"language", lang},
               {"cleanup", cleanup}}}}},
           {"annotations", {{"readOnlyHint", true}, {"openWorldHint", false}}}});
    }
    tools.push_back(
        {{"name", "clean_text"},
         {"title", "Clean up dictated text"},
         {"description", "Apply Lisper's dictation cleanup (filler-word removal, spoken "
                         "'new line' commands, personal dictionary and snippets) to text."},
         {"inputSchema",
          {{"type", "object"},
           {"properties", {{"text", {{"type", "string"}}}}},
           {"required", {"text"}}}},
         {"annotations", read_only}});
    tools.push_back({{"name", "list_models"},
                     {"title", "List speech models"},
                     {"description", "List Whisper model profiles, which are installed, and "
                                     "which one is active."},
                     {"inputSchema", {{"type", "object"}, {"properties", json::object()}}},
                     {"annotations", read_only}});
    if (options.allow_download) {
      tools.push_back(
          {{"name", "download_model"},
           {"title", "Download speech model"},
           {"description", "Download the Whisper model for a profile from Hugging Face."},
           {"inputSchema",
            {{"type", "object"},
             {"properties",
              {{"profile",
                {{"type", "string"}, {"enum", {"tiny", "fast", "balanced", "quality", "max"}}}}}}}},
           {"annotations", {{"readOnlyHint", false}, {"openWorldHint", true}}}});
    }
    if (options.allow_history) {
      tools.push_back(
          {{"name", "get_dictation_history"},
           {"title", "Search dictation history"},
           {"description", "Return recent dictations the user made with Lisper, newest first."},
           {"inputSchema",
            {{"type", "object"},
             {"properties",
              {{"limit", {{"type", "integer"}, {"minimum", 1}, {"maximum", 200}}},
               {"query",
                {{"type", "string"}, {"description", "Case-insensitive substring filter."}}}}}}},
           {"annotations", read_only}});
    }
    return tools;
  }

  json call_tool(const std::string &name, const json &args) {
    try {
      if (name == "transcribe_file") {
        return tool_transcribe_file(args);
      }
      if (name == "transcribe_audio") {
        return tool_transcribe_audio(args);
      }
      if (name == "record_and_transcribe" && options.allow_microphone) {
        return tool_record(args);
      }
      if (name == "clean_text") {
        return tool_clean_text(args);
      }
      if (name == "list_models") {
        return tool_list_models(args);
      }
      if (name == "download_model" && options.allow_download) {
        return tool_download(args);
      }
      if (name == "get_dictation_history" && options.allow_history) {
        return tool_history(args);
      }
    } catch (const ToolError &e) {
      return tool_failure(e.what());
    } catch (const std::exception &e) {
      return tool_failure(std::string("Internal error: ") + e.what());
    }
    throw std::out_of_range(name);
  }

  // -- dispatch -------------------------------------------------------------

  std::optional<json> dispatch(const json &msg) {
    if (!msg.is_object() || msg.value("jsonrpc", "") != "2.0") {
      return rpc_error(msg.is_object() && msg.contains("id") ? msg["id"] : json(nullptr),
                       kInvalidRequest, "Invalid JSON-RPC 2.0 message");
    }
    const bool is_request = msg.contains("id") && !msg["id"].is_null();
    if (!msg.contains("method") || !msg["method"].is_string()) {
      // A response from the client (we never send requests) or garbage.
      return is_request
                 ? std::optional<json>(rpc_error(msg["id"], kInvalidRequest, "Missing method"))
                 : std::nullopt;
    }

    const std::string method = msg["method"];
    const json params =
        msg.contains("params") && msg["params"].is_object() ? msg["params"] : json::object();
    const json id = is_request ? msg["id"] : json(nullptr);

    if (!is_request) {
      return std::nullopt; // notifications/initialized, notifications/cancelled, ...
    }

    if (method == "initialize") {
      std::string version = params.value("protocolVersion", kLatestProtocol);
      if (std::find(kSupportedProtocols.begin(), kSupportedProtocols.end(), version) ==
          kSupportedProtocols.end()) {
        version = kLatestProtocol;
      }
      return rpc_result(
          id,
          {{"protocolVersion", version},
           {"capabilities", {{"tools", {{"listChanged", false}}}}},
           {"serverInfo", {{"name", "lisper"}, {"title", "Lisper"}, {"version", LISPER_VERSION}}},
           {"instructions",
            "Lisper provides private, on-device speech-to-text. Use transcribe_file for "
            "audio/video files on disk, transcribe_audio for base64 audio, and "
            "record_and_transcribe (when enabled) to let the user dictate."}});
    }
    if (method == "ping") {
      return rpc_result(id, json::object());
    }
    if (method == "tools/list") {
      return rpc_result(id, {{"tools", tool_list()}});
    }
    if (method == "tools/call") {
      if (!params.contains("name") || !params["name"].is_string()) {
        return rpc_error(id, kInvalidParams, "tools/call requires a tool name");
      }
      const json args = params.contains("arguments") && params["arguments"].is_object()
                            ? params["arguments"]
                            : json::object();
      try {
        return rpc_result(id, call_tool(params["name"].get<std::string>(), args));
      } catch (const std::out_of_range &) {
        return rpc_error(id, kInvalidParams, "Unknown tool: " + params["name"].get<std::string>());
      }
    }
    return rpc_error(id, kMethodNotFound, "Method not found: " + method);
  }
};

Server::Server(ServerOptions options) : impl_(std::make_unique<Impl>(std::move(options))) {}
Server::~Server() = default;

bool Server::path_allowed(const std::string &path, std::string &resolved) const {
  return impl_->path_allowed(path, resolved);
}

std::string Server::handle(const std::string &message) {
  const json msg = json::parse(message, nullptr, false);
  if (msg.is_discarded()) {
    return dump(rpc_error(nullptr, kParseError, "Parse error"));
  }
  if (msg.is_array()) {
    if (msg.empty()) {
      return dump(rpc_error(nullptr, kInvalidRequest, "Empty batch"));
    }
    json responses = json::array();
    for (const auto &m : msg) {
      if (auto r = impl_->dispatch(m)) {
        responses.push_back(*r);
      }
    }
    return responses.empty() ? "" : dump(responses);
  }
  auto r = impl_->dispatch(msg);
  return r ? dump(*r) : "";
}

int Server::run_stdio(std::istream &in,
                      const std::function<void(const std::string &)> &write_line) {
  // Requests run on a worker so the reader can still see cancellations while
  // a long transcription is in progress.
  std::mutex mutex;
  std::condition_variable cv;
  std::deque<std::string> queue;
  bool eof = false;
  json running_id;        // id of the request currently executing
  bool cancelled = false; // the running request was cancelled by the client

  std::thread worker([&] {
    for (;;) {
      std::string line;
      {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [&] { return eof || !queue.empty(); });
        if (queue.empty()) {
          return;
        }
        line = std::move(queue.front());
        queue.pop_front();
        const json peek = json::parse(line, nullptr, false);
        running_id = peek.is_object() && peek.contains("id") ? peek["id"] : json(nullptr);
        cancelled = false;
        interrupt_state::reset();
      }
      const std::string response = handle(line);
      bool drop = false;
      {
        std::lock_guard<std::mutex> lock(mutex);
        running_id = nullptr;
        drop = cancelled; // the spec says not to answer cancelled requests
      }
      if (!response.empty() && !drop) {
        write_line(response);
      }
    }
  });

  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (trim(line).empty()) {
      continue;
    }

    const json peek = json::parse(line, nullptr, false);
    if (!peek.is_discarded() && peek.is_object() &&
        peek.value("method", "") == "notifications/cancelled") {
      const json params = peek.value("params", json::object());
      std::lock_guard<std::mutex> lock(mutex);
      if (params.contains("requestId") && !running_id.is_null() &&
          params["requestId"] == running_id) {
        cancelled = true;
        interrupt_state::request_stop();
      }
      // Drop a queued request that was cancelled before it started.
      queue.erase(std::remove_if(queue.begin(), queue.end(),
                                 [&](const std::string &queued) {
                                   const json q = json::parse(queued, nullptr, false);
                                   return q.is_object() && params.contains("requestId") &&
                                          q.value("id", json(nullptr)) == params["requestId"];
                                 }),
                  queue.end());
      continue;
    }

    {
      std::lock_guard<std::mutex> lock(mutex);
      queue.push_back(std::move(line));
    }
    cv.notify_one();
  }

  {
    std::lock_guard<std::mutex> lock(mutex);
    eof = true;
  }
  cv.notify_one();
  worker.join();
  return 0;
}

} // namespace mcp
