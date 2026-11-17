// lisper-mcp: Model Context Protocol server for Lisper (stdio transport).
//
// Example client configuration (Claude Desktop / Claude Code / Cursor):
//   {
//     "mcpServers": {
//       "lisper": {
//         "command": "/usr/local/bin/lisper-mcp",
//         "args": ["--allow-dir", "/Users/me/Recordings", "--allow-microphone"]
//       }
//     }
//   }

#include "mcp_server.h"
#include "paths.h"

#include <cstdio>
#include <cstring>
#include <iostream>
#include <mutex>
#include <string>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#else
#include <unistd.h>
#endif

#ifndef LISPER_VERSION
#define LISPER_VERSION "0.0.0"
#endif

namespace {

void print_help() {
  std::cerr << "lisper-mcp " << LISPER_VERSION
            << " - local speech-to-text MCP server (stdio)\n\n"
               "Options:\n"
               "  --allow-dir <dir>      Directory transcribe_file may read (repeatable).\n"
               "                         Default: your home directory.\n"
               "  --allow-microphone     Expose record_and_transcribe (microphone access).\n"
               "  --allow-history        Expose get_dictation_history.\n"
               "  --allow-download       Expose download_model (network access).\n"
               "  --model-profile <name> tiny, fast, balanced, quality, max.\n"
               "  -m, --model <path>     Explicit GGML model file.\n"
               "  -l, --language <code>  Default language (auto to detect).\n"
               "  -t, --threads <n>      Decoder threads.\n"
               "  --version              Print version and exit.\n\n"
               "Settings, dictionary and snippets are read from "
            << paths::config_dir() << "\n";
}

// Keep a private handle to the real stdout for protocol messages and point
// file descriptor 1 at stderr, so a stray printf from a dependency can never
// corrupt the JSON-RPC stream.
FILE *claim_stdout() {
#ifdef _WIN32
  _setmode(_fileno(stdin), _O_BINARY);
  const int fd = _dup(_fileno(stdout));
  if (fd < 0) {
    return stdout;
  }
  _dup2(_fileno(stderr), _fileno(stdout));
  FILE *out = _fdopen(fd, "wb");
#else
  const int fd = dup(STDOUT_FILENO);
  if (fd < 0) {
    return stdout;
  }
  dup2(STDERR_FILENO, STDOUT_FILENO);
  FILE *out = fdopen(fd, "w");
#endif
  return out != nullptr ? out : stdout;
}

} // namespace

int main(int argc, char **argv) {
  mcp::ServerOptions options;

  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto next = [&](const char *flag) -> std::string {
      if (i + 1 >= argc) {
        std::cerr << "Missing value for " << flag << "\n";
        std::exit(2);
      }
      return argv[++i];
    };
    if (a == "-h" || a == "--help") {
      print_help();
      return 0;
    } else if (a == "--version") {
      std::cerr << LISPER_VERSION << "\n";
      return 0;
    } else if (a == "--allow-dir") {
      options.allowed_dirs.push_back(next("--allow-dir"));
    } else if (a == "--allow-microphone") {
      options.allow_microphone = true;
    } else if (a == "--allow-history") {
      options.allow_history = true;
    } else if (a == "--allow-download") {
      options.allow_download = true;
    } else if (a == "--model-profile") {
      options.model_profile = next("--model-profile");
    } else if (a == "-m" || a == "--model") {
      options.model_path = next("--model");
    } else if (a == "-l" || a == "--language") {
      options.language = next("--language");
    } else if (a == "-t" || a == "--threads") {
      try {
        options.threads = std::stoi(next("--threads"));
      } catch (const std::exception &) {
        options.threads = 0;
      }
    } else {
      std::cerr << "Unknown option: " << a << "\n";
      print_help();
      return 2;
    }
  }

  FILE *out = claim_stdout();
  std::mutex write_mutex;
  mcp::Server server(options);
  std::cerr << "lisper-mcp " << LISPER_VERSION << " ready\n";

  return server.run_stdio(std::cin, [&](const std::string &line) {
    std::lock_guard<std::mutex> lock(write_mutex);
    std::fwrite(line.data(), 1, line.size(), out);
    std::fputc('\n', out);
    std::fflush(out);
  });
}
