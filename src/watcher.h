#pragma once

#include <functional>
#include <string>
#include <vector>

namespace watcher {

struct WatchConfig {
  std::string watch_dir;
  int poll_interval_sec = 3;
};

// Watch a directory for new media files and invoke the callback for each new
// file once its size has stopped changing between two polls (so files that
// are still being copied are not transcribed half-written). Blocks until
// interrupt_state::request_stop() is called. Returns false if the directory
// does not exist.
bool watch_directory(const WatchConfig &config,
                     std::function<void(const std::string &)> on_new_file);

// scan a directory for all media files, returns their paths.
std::vector<std::string> scan_directory(const std::string &dir);

} // namespace watcher
