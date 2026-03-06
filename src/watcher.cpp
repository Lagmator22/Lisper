#include "watcher.h"
#include "media.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <set>
#include <thread>

namespace fs = std::filesystem;

namespace watcher {

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

void watch_directory(const WatchConfig &config,
                     std::function<void(const std::string &)> on_new_file) {
  if (!fs::is_directory(config.watch_dir)) {
    std::cerr << "Watch directory does not exist: " << config.watch_dir << "\n";
    return;
  }

  std::set<std::string> known_files;

  // index existing files so we only trigger on new ones
  for (const auto &f : scan_directory(config.watch_dir)) {
    known_files.insert(f);
  }

  std::cout << "Watching " << config.watch_dir
            << " for new media files. Press Ctrl+C to stop.\n";

  while (true) {
    std::this_thread::sleep_for(std::chrono::seconds(config.poll_interval_sec));

    auto current = scan_directory(config.watch_dir);
    for (const auto &f : current) {
      if (known_files.find(f) == known_files.end()) {
        known_files.insert(f);
        std::cout << "New file detected: " << f << "\n";
        on_new_file(f);
      }
    }
  }
}

} // namespace watcher
