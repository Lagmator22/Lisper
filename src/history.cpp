#include "history.h"

#include "paths.h"
#include "text_cleanup.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <random>
#include <set>
#include <sstream>

namespace fs = std::filesystem;
using nlohmann::json;

namespace history {

namespace {

std::string resolve(const std::string &path) {
  return path.empty() ? history_path() : path;
}

std::vector<Entry> load_all(const std::string &path) {
  std::vector<Entry> out;
  std::ifstream in(path, std::ios::binary);
  std::string line;
  while (std::getline(in, line)) {
    const json j = json::parse(line, nullptr, false);
    if (j.is_discarded() || !j.is_object()) {
      continue; // skip corrupt lines instead of failing the whole history
    }
    Entry e;
    e.id = j.value("id", "");
    e.timestamp_ms = j.value("ts", static_cast<int64_t>(0));
    e.text = j.value("text", "");
    e.raw_text = j.value("raw", "");
    e.duration_ms = j.value("duration_ms", 0);
    e.words = j.value("words", 0);
    e.language = j.value("lang", "");
    out.push_back(std::move(e));
  }
  return out;
}

std::string serialize(const Entry &e) {
  json j = {{"id", e.id},        {"ts", e.timestamp_ms},         {"text", e.text},
            {"raw", e.raw_text}, {"duration_ms", e.duration_ms}, {"words", e.words},
            {"lang", e.language}};
  return j.dump();
}

bool write_all(const std::vector<Entry> &oldest_first, const std::string &path) {
  std::string content;
  for (const auto &e : oldest_first) {
    content += serialize(e);
    content += '\n';
  }
  return paths::write_private_file(path, content);
}

int64_t local_day(int64_t ms) {
  const std::time_t t = static_cast<std::time_t>(ms / 1000);
  std::tm tm{};
#ifdef _WIN32
  localtime_s(&tm, &t);
#else
  localtime_r(&t, &tm);
#endif
  // Days since 1970-01-01 for the local calendar date (Howard Hinnant's
  // days_from_civil), so "yesterday" works across month and year boundaries.
  int64_t y = static_cast<int64_t>(tm.tm_year) + 1900;
  const int64_t m = tm.tm_mon + 1;
  const int64_t d = tm.tm_mday;
  y -= m <= 2 ? 1 : 0;
  const int64_t era = (y >= 0 ? y : y - 399) / 400;
  const int64_t yoe = y - era * 400;
  const int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

} // namespace

std::string history_path() {
  return (fs::path(paths::data_dir()) / "history.jsonl").string();
}

int64_t now_ms() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

Entry make_entry(const std::string &text, const std::string &raw_text, int duration_ms,
                 const std::string &language) {
  static const char *kHex = "0123456789abcdef";
  std::random_device rd;
  Entry e;
  for (int i = 0; i < 16; ++i) {
    e.id.push_back(kHex[rd() & 0xF]);
  }
  e.timestamp_ms = now_ms();
  e.text = text;
  e.raw_text = raw_text;
  e.duration_ms = duration_ms;
  e.words = text_cleanup::count_words(text);
  e.language = language;
  return e;
}

std::vector<Entry> load(int limit, const std::string &path) {
  auto all = load_all(resolve(path));
  std::reverse(all.begin(), all.end());
  if (limit > 0 && static_cast<int>(all.size()) > limit) {
    all.resize(static_cast<size_t>(limit));
  }
  return all;
}

bool append(const Entry &entry, int max_entries, const std::string &path) {
  const std::string p = resolve(path);
  auto all = load_all(p);
  all.push_back(entry);
  if (max_entries > 0 && static_cast<int>(all.size()) > max_entries) {
    all.erase(all.begin(), all.end() - max_entries);
  }
  return write_all(all, p);
}

bool remove(const std::string &id, const std::string &path) {
  const std::string p = resolve(path);
  auto all = load_all(p);
  const auto before = all.size();
  all.erase(std::remove_if(all.begin(), all.end(), [&](const Entry &e) { return e.id == id; }),
            all.end());
  return all.size() != before && write_all(all, p);
}

bool clear(const std::string &path) {
  std::error_code ec;
  fs::remove(resolve(path), ec);
  return !ec;
}

Stats compute_stats(const std::vector<Entry> &entries, int64_t now) {
  Stats s;
  std::set<int64_t> days;
  for (const auto &e : entries) {
    ++s.entries;
    s.total_words += e.words;
    s.total_audio_ms += e.duration_ms;
    days.insert(local_day(e.timestamp_ms));
  }
  if (s.total_audio_ms > 0) {
    s.words_per_minute =
        static_cast<double>(s.total_words) / (static_cast<double>(s.total_audio_ms) / 60000.0);
  }

  int64_t day = local_day(now);
  if (days.count(day) == 0) {
    day -= 1; // a streak survives until the end of today
  }
  while (days.count(day) != 0) {
    ++s.day_streak;
    day -= 1;
  }
  return s;
}

} // namespace history
