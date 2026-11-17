#include "subprocess.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
#endif

namespace proc {

#ifdef _WIN32

namespace {

std::wstring widen(const std::string &s) {
  if (s.empty()) {
    return {};
  }
  const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
  std::wstring out(static_cast<size_t>(n), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), n);
  return out;
}

} // namespace

std::wstring quote_windows_arg(const std::wstring &arg) {
  if (!arg.empty() && arg.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
    return arg;
  }

  std::wstring quoted = L"\"";
  for (auto it = arg.begin();; ++it) {
    size_t backslashes = 0;
    while (it != arg.end() && *it == L'\\') {
      ++it;
      ++backslashes;
    }

    if (it == arg.end()) {
      quoted.append(backslashes * 2, L'\\');
      break;
    }
    if (*it == L'"') {
      quoted.append(backslashes * 2 + 1, L'\\');
      quoted.push_back(*it);
    } else {
      quoted.append(backslashes, L'\\');
      quoted.push_back(*it);
    }
  }
  quoted.push_back(L'"');
  return quoted;
}

int run(const std::vector<std::string> &argv, const RunOptions &options) {
  if (argv.empty()) {
    return -1;
  }

  std::wstring cmdline;
  for (size_t i = 0; i < argv.size(); ++i) {
    if (i > 0) {
      cmdline.push_back(L' ');
    }
    cmdline += quote_windows_arg(widen(argv[i]));
  }

  STARTUPINFOW si{};
  si.cb = sizeof(si);
  HANDLE null_handle = INVALID_HANDLE_VALUE;
  if (options.silence_output) {
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    null_handle = CreateFileW(L"NUL", GENERIC_READ | GENERIC_WRITE,
                              FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);
    if (null_handle != INVALID_HANDLE_VALUE) {
      si.dwFlags |= STARTF_USESTDHANDLES;
      si.hStdInput = null_handle;
      si.hStdOutput = null_handle;
      si.hStdError = null_handle;
    }
  }

  PROCESS_INFORMATION pi{};
  // lpApplicationName is null so that the program is searched on PATH; the
  // command line is fully quoted so no shell interpretation takes place.
  const BOOL ok =
      CreateProcessW(nullptr, cmdline.data(), nullptr, nullptr, null_handle != INVALID_HANDLE_VALUE,
                     CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
  if (null_handle != INVALID_HANDLE_VALUE) {
    CloseHandle(null_handle);
  }
  if (!ok) {
    return -1;
  }

  WaitForSingleObject(pi.hProcess, INFINITE);
  DWORD code = 1;
  GetExitCodeProcess(pi.hProcess, &code);
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  return static_cast<int>(code);
}

#else

int run(const std::vector<std::string> &argv, const RunOptions &options) {
  if (argv.empty()) {
    return -1;
  }

  std::vector<char *> cargv;
  cargv.reserve(argv.size() + 1);
  for (const auto &a : argv) {
    cargv.push_back(const_cast<char *>(a.c_str()));
  }
  cargv.push_back(nullptr);

  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_init(&actions);

  int stdin_pipe[2] = {-1, -1};
  if (options.feed_stdin) {
    if (pipe(stdin_pipe) != 0) {
      posix_spawn_file_actions_destroy(&actions);
      return -1;
    }
    posix_spawn_file_actions_adddup2(&actions, stdin_pipe[0], STDIN_FILENO);
    posix_spawn_file_actions_addclose(&actions, stdin_pipe[0]);
    posix_spawn_file_actions_addclose(&actions, stdin_pipe[1]);
  } else {
    posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
  }

  if (options.silence_output) {
    posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
    posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
  }

  pid_t pid = 0;
  const int rc = posix_spawnp(&pid, cargv[0], &actions, nullptr, cargv.data(), environ);
  posix_spawn_file_actions_destroy(&actions);

  if (options.feed_stdin) {
    close(stdin_pipe[0]);
    if (rc == 0) {
      // Ignore SIGPIPE while writing in case the child exits early.
      struct sigaction ignore {};
      struct sigaction previous {};
      ignore.sa_handler = SIG_IGN;
      sigaction(SIGPIPE, &ignore, &previous);
      const char *data = options.stdin_data.data();
      size_t left = options.stdin_data.size();
      while (left > 0) {
        const ssize_t n = write(stdin_pipe[1], data, left);
        if (n < 0) {
          if (errno == EINTR) {
            continue;
          }
          break;
        }
        data += n;
        left -= static_cast<size_t>(n);
      }
      sigaction(SIGPIPE, &previous, nullptr);
    }
    close(stdin_pipe[1]);
  }

  if (rc != 0) {
    return -1;
  }

  int status = 0;
  while (waitpid(pid, &status, 0) < 0) {
    if (errno != EINTR) {
      return -1;
    }
  }
  if (WIFEXITED(status)) {
    const int code = WEXITSTATUS(status);
    // posix_spawnp reports exec failure via exit code 127 on some libcs.
    return code;
  }
  return -1;
}

#endif

bool program_available(const std::string &program, const std::string &version_flag) {
  const int code = run({program, version_flag});
  return code == 0;
}

} // namespace proc
