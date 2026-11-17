#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Dictation history, stored locally as JSON Lines in
// paths::data_dir()/history.jsonl with owner-only permissions.
namespace history {

struct Entry {
  std::string id;
  int64_t timestamp_ms = 0; // unix epoch milliseconds
  std::string text;         // cleaned text that was inserted
  std::string raw_text;     // whisper output before cleanup
  int duration_ms = 0;      // spoken audio length
  int words = 0;
  std::string language;
};

struct Stats {
  int entries = 0;
  int64_t total_words = 0;
  int64_t total_audio_ms = 0;
  double words_per_minute = 0.0;
  int day_streak = 0; // consecutive days (ending today or yesterday) with dictation
};

std::string history_path();

// Newest first. `limit` <= 0 returns everything.
std::vector<Entry> load(int limit = 0, const std::string &path = "");

// Append an entry and trim the file to `max_entries` (0 = unlimited).
bool append(const Entry &entry, int max_entries = 0, const std::string &path = "");

bool remove(const std::string &id, const std::string &path = "");
bool clear(const std::string &path = "");

Stats compute_stats(const std::vector<Entry> &entries, int64_t now_ms);

Entry make_entry(const std::string &text, const std::string &raw_text, int duration_ms,
                 const std::string &language);

int64_t now_ms();

} // namespace history
