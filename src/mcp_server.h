#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

// Model Context Protocol server exposing Lisper's local speech-to-text to MCP
// clients (Claude Desktop, Claude Code, Cursor, VS Code, ...). Transport is
// newline-delimited JSON-RPC 2.0 over stdio.
namespace mcp {

struct ServerOptions {
  // Directories transcribe_file may read from. Paths are canonicalised
  // (symlinks and ".." resolved) before the check. Empty = the user's home.
  std::vector<std::string> allowed_dirs;
  // Opt-in capabilities that touch private data.
  bool allow_microphone = false;
  bool allow_history = false;
  bool allow_download = false;
  // Overrides for the persisted settings.
  std::string model_profile;
  std::string model_path;
  std::string language;
  int threads = 0;
  // Upper bound for transcribe_audio payloads (decoded bytes).
  size_t max_audio_bytes = 50u * 1024u * 1024u;
};

class Server {
public:
  explicit Server(ServerOptions options);
  ~Server();

  // Handle one JSON-RPC message (a single object or a batch array). Returns
  // the serialized response, or an empty string for notifications.
  std::string handle(const std::string &message);

  // Run the stdio loop until EOF. Responses are written with `write_line`.
  int run_stdio(std::istream &in, const std::function<void(const std::string &)> &write_line);

  // True when `path` resolves inside one of the allowed directories.
  bool path_allowed(const std::string &path, std::string &resolved) const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

std::string base64_decode(const std::string &in, bool &ok);

} // namespace mcp
