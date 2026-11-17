#pragma once

#include <functional>
#include <memory>
#include <string>

namespace gui {

// Single-instance control channel. The running app listens on a socket inside
// the private (0700) data directory; `lisper_gui --toggle` / `--show` connect
// to it instead of launching a second copy. This is also how Wayland users
// bind dictation to a compositor shortcut. POSIX only; on Windows `send`
// returns false and the global hotkey is used instead.
class ControlChannel {
public:
  ControlChannel();
  ~ControlChannel();

  // Start listening. Returns false if another instance already owns the
  // socket (in which case the caller should forward its command and exit).
  bool listen(std::function<void(const std::string &command)> on_command);

  // Send a command ("toggle", "show", "start", "stop") to a running instance.
  static bool send(const std::string &command);

  struct Impl;

private:
  std::unique_ptr<Impl> impl_;
};

} // namespace gui
