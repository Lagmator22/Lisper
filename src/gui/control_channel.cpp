#include "gui/control_channel.h"

#include "paths.h"

#include <atomic>
#include <filesystem>
#include <thread>

#ifndef _WIN32
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace gui {

#ifndef _WIN32

namespace {

std::string socket_path() {
  return (fs::path(paths::data_dir()) / "control.sock").string();
}

bool fill_addr(sockaddr_un &addr, const std::string &path) {
  std::memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  if (path.size() >= sizeof(addr.sun_path)) {
    return false;
  }
  std::memcpy(addr.sun_path, path.c_str(), path.size() + 1);
  return true;
}

int make_socket() {
  const int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd >= 0) {
    fcntl(fd, F_SETFD, FD_CLOEXEC); // SOCK_CLOEXEC is not available on macOS
  }
  return fd;
}

bool valid_command(const std::string &c) {
  return c == "toggle" || c == "show" || c == "start" || c == "stop";
}

} // namespace

struct ControlChannel::Impl {
  int fd = -1;
  std::thread thread;
  std::atomic<bool> stop{false};
  std::string path;
};

ControlChannel::ControlChannel() : impl_(std::make_unique<Impl>()) {}

ControlChannel::~ControlChannel() {
  impl_->stop = true;
  if (impl_->thread.joinable()) {
    impl_->thread.join();
  }
  if (impl_->fd >= 0) {
    close(impl_->fd);
    std::error_code ec;
    fs::remove(impl_->path, ec);
  }
}

bool ControlChannel::send(const std::string &command) {
  if (!valid_command(command)) {
    return false;
  }
  sockaddr_un addr;
  if (!fill_addr(addr, socket_path())) {
    return false;
  }
  const int fd = make_socket();
  if (fd < 0) {
    return false;
  }
  bool ok = connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == 0;
  if (ok) {
    const std::string line = command + "\n";
    ok = write(fd, line.data(), line.size()) == static_cast<ssize_t>(line.size());
  }
  close(fd);
  return ok;
}

bool ControlChannel::listen(std::function<void(const std::string &)> on_command) {
  if (!paths::ensure_private_dir(paths::data_dir())) {
    return true; // cannot set up IPC; run without it
  }
  impl_->path = socket_path();
  sockaddr_un addr;
  if (!fill_addr(addr, impl_->path)) {
    return true;
  }

  // If something answers, another instance is running.
  if (send("show")) {
    return false;
  }
  std::error_code ec;
  fs::remove(impl_->path, ec); // stale socket from a crash

  impl_->fd = make_socket();
  if (impl_->fd < 0) {
    return true;
  }
  if (bind(impl_->fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0 ||
      ::listen(impl_->fd, 4) != 0) {
    close(impl_->fd);
    impl_->fd = -1;
    return true;
  }
  fs::permissions(impl_->path, fs::perms::owner_read | fs::perms::owner_write,
                  fs::perm_options::replace, ec);

  impl_->thread = std::thread([this, on_command] {
    while (!impl_->stop) {
      pollfd pfd{impl_->fd, POLLIN, 0};
      if (poll(&pfd, 1, 200) <= 0) {
        continue;
      }
      const int client = accept(impl_->fd, nullptr, nullptr);
      if (client < 0) {
        continue;
      }
      char buf[32] = {};
      pollfd cfd{client, POLLIN, 0};
      if (poll(&cfd, 1, 500) > 0) {
        const ssize_t n = read(client, buf, sizeof(buf) - 1);
        if (n > 0) {
          std::string cmd(buf, static_cast<size_t>(n));
          while (!cmd.empty() && (cmd.back() == '\n' || cmd.back() == '\r')) {
            cmd.pop_back();
          }
          if (valid_command(cmd)) {
            on_command(cmd);
          }
        }
      }
      close(client);
    }
  });
  return true;
}

#else

struct ControlChannel::Impl {};
ControlChannel::ControlChannel() : impl_(std::make_unique<Impl>()) {}
ControlChannel::~ControlChannel() = default;
bool ControlChannel::send(const std::string &) {
  return false;
}
bool ControlChannel::listen(std::function<void(const std::string &)>) {
  return true;
}

#endif

} // namespace gui
