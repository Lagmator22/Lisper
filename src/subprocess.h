#pragma once

#include <string>
#include <vector>

// Shell-free process execution helpers.
//
// Every external program Lisper launches (ffmpeg, curl, paste helpers) goes
// through this module. Arguments are passed as a vector and never interpreted
// by a shell, so file names containing quotes, `$()`, `;`, `%VAR%` and similar
// metacharacters cannot inject commands.
namespace proc {

struct RunOptions {
  // Discard the child's stdout/stderr (redirect to the null device).
  bool silence_output = true;
  // Optional data written to the child's stdin (POSIX only; ignored elsewhere).
  std::string stdin_data;
  bool feed_stdin = false;
};

// Run `argv[0]` (looked up on PATH) with the given arguments and wait for it.
// Returns the exit code, or -1 if the program could not be started.
int run(const std::vector<std::string> &argv, const RunOptions &options = {});

// Returns true when `program` can be found on PATH and executed.
bool program_available(const std::string &program, const std::string &version_flag = "-version");

#ifdef _WIN32
// Quote one argument following the rules CommandLineToArgvW uses. Exposed for
// unit tests.
std::wstring quote_windows_arg(const std::wstring &arg);
#endif

} // namespace proc
