#include "watcher.h"
#include "interrupt.h"
#include "media.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <map>
#include <set>
#include <thread>

namespace fs = std::filesystem;

namespace watcher {

std::vector<std::string> scan_directory(const std::string &dir) {
  std::vector<std::string> files;
  std::error_code ec;
  if (!fs::is_directory(dir, ec))
    return files;

  for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
    const auto &entry = *it;
    std::error_code entry_ec;
    if (entry.is_regular_file(entry_ec) && !entry_ec &&
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

  std::set<std::string> known_files;
  // Candidate files and the size observed on the previous poll.
  std::map<std::string, uintmax_t> pending;

  // index existing files so we only trigger on new ones
  for (const auto &f : scan_directory(config.watch_dir)) {
    known_files.insert(f);
  }

  std::cout << "Watching " << config.watch_dir << " for new media files. Press Ctrl+C to stop.\n";

  const int poll_ticks = std::max(1, config.poll_interval_sec) * 10;
  while (!interrupt_state::is_interrupted()) {
    // Use shorter sleep intervals for more responsive shutdown
    for (int i = 0; i < poll_ticks && !interrupt_state::is_interrupted(); i++) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (interrupt_state::is_interrupted()) {
      break;
    }

    for (const auto &f : scan_directory(config.watch_dir)) {
      if (known_files.count(f) != 0) {
        continue;
      }
      std::error_code ec;
      const uintmax_t size = fs::file_size(f, ec);
      if (ec) {
        continue;
      }
      auto it = pending.find(f);
      if (it == pending.end() || it->second != size || size == 0) {
        pending[f] = size; // still growing (or first sighting); check next poll
        continue;
      }

      pending.erase(it);
      known_files.insert(f);
      std::cout << "New file detected: " << f << "\n";
      on_new_file(f);

      if (interrupt_state::is_interrupted()) {
        break;
      }
    }
  }

  return true;
}

} // namespace watcher
