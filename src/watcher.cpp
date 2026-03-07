#include "watcher.h"
#include "interrupt.h"
#include "media.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <set>
#include <thread>

namespace fs = std::filesystem;

namespace watcher {

static std::atomic<bool> watch_interrupted(false);

void interrupt_watch() {
  watch_interrupted = true;
}

std::vector<std::string> scan_directory(const std::string &dir) {
  std::vector<std::string> files;
  if (!fs::is_directory(dir))
    return files;

  for (const auto &entry : fs::directory_iterator(dir)) {
    if (entry.is_regular_file() &&
        media::is_media_file(entry.path().string())) {
      files.push_back(entry.path().string());
    }
  }

  std::sort(files.begin(), files.end());
  return files;
}

bool watch_directory(const WatchConfig &config,
                     std::function<void(const std::string &)> on_new_file) {
  if (!fs::is_directory(config.watch_dir)) {
    std::cerr << "Watch directory does not exist: " << config.watch_dir << "\n";
    return false;
  }

  // Reset the interrupt flag
  watch_interrupted = false;

  std::set<std::string> known_files;

  // index existing files so we only trigger on new ones
  for (const auto &f : scan_directory(config.watch_dir)) {
    known_files.insert(f);
  }

  std::cout << "Watching " << config.watch_dir
            << " for new media files. Press Ctrl+C to stop.\n";

  while (!watch_interrupted && !interrupt_state::is_interrupted()) {
    // Use shorter sleep intervals for more responsive shutdown
    for (int i = 0;
         i < config.poll_interval_sec * 10 && !watch_interrupted &&
         !interrupt_state::is_interrupted();
         i++) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (watch_interrupted || interrupt_state::is_interrupted()) {
      break;
    }

    auto current = scan_directory(config.watch_dir);
    for (const auto &f : current) {
      if (known_files.find(f) == known_files.end()) {
        known_files.insert(f);
        std::cout << "New file detected: " << f << "\n";
        on_new_file(f);

        if (watch_interrupted || interrupt_state::is_interrupted()) {
          break;
        }
      }
    }
  }

  return true;
}

} // namespace watcher
