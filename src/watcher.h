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
// Returns true if stopped via interrupt_watch(), false on error
bool watch_directory(const WatchConfig &config,
                     std::function<void(const std::string &)> on_new_file);

// Signal the watcher to stop
void interrupt_watch();

// scan a directory for all media files, returns their paths.
std::vector<std::string> scan_directory(const std::string &dir);

} // namespace watcher
