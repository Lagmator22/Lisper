#pragma once

#include <functional>
#include <string>
#include <vector>

namespace watcher {

struct WatchConfig {
  std::string watch_dir;
  std::string output_dir;
  int poll_interval_sec = 3;
};

// watch a directory for new media files and invoke the callback
// for each new file. blocks until interrupted.
void watch_directory(const WatchConfig &config,
                     std::function<void(const std::string &)> on_new_file);

// scan a directory for all media files, returns their paths.
std::vector<std::string> scan_directory(const std::string &dir);

} // namespace watcher
