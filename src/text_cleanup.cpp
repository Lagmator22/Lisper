#include "text_cleanup.h"

#include <algorithm>
#include <cctype>
#include <set>

namespace text_cleanup {

namespace {

char lower(char c) {
  return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

bool is_word_char(char c) {
  const unsigned char u = static_cast<unsigned char>(c);
  return std::isalnum(u) || c == '\'' || u >= 0x80; // treat UTF-8 bytes as letters
}

std::string to_lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), lower);
  return s;
}

std::string trim(const std::string &s) {
  const auto a = s.find_first_not_of(" \t\r\n");
  if (a == std::string::npos) {
    return "";
  }
  const auto b = s.find_last_not_of(" \t\r\n");
  return s.substr(a, b - a + 1);
}

// Case-insensitive, whole-word phrase replacement. `phrase` may contain
// several words; any run of spaces in the text matches a single space.
// When `eat_trailing_punct` is set, a comma/period directly after the match
// is removed too (whisper often writes "Um, so" or "new line.").
std::string replace_phrase(const std::string &text, const std::string &phrase,
                           const std::string &replacement, bool eat_trailing_punct) {
  const std::string needle = to_lower(trim(phrase));
  if (needle.empty()) {
    return text;
  }

  std::string out;
  out.reserve(text.size());
  size_t i = 0;
  while (i < text.size()) {
    const bool boundary_before = i == 0 || !is_word_char(text[i - 1]);
    if (boundary_before) {
      size_t t = i;
      size_t n = 0;
      while (n < needle.size() && t < text.size()) {
        if (needle[n] == ' ') {
          if (text[t] != ' ') {
            break;
          }
          while (t < text.size() && text[t] == ' ') {
            ++t;
          }
          ++n;
          continue;
        }
        if (lower(text[t]) != needle[n]) {
          break;
        }
        ++t;
        ++n;
      }
      const bool matched = n == needle.size() && (t == text.size() || !is_word_char(text[t]));
      if (matched) {
        out += replacement;
        if (eat_trailing_punct && t < text.size() && (text[t] == ',' || text[t] == '.')) {
          ++t;
        }
        i = t;
        continue;
      }
    }
    out.push_back(text[i]);
    ++i;
  }
  return out;
}

std::string collapse_spaces(const std::string &text) {
  std::string out;
  out.reserve(text.size());
  for (char c : text) {
    if (c == ' ' && (out.empty() || out.back() == ' ' || out.back() == '\n')) {
      continue;
    }
    if ((c == ',' || c == '.' || c == '?' || c == '!' || c == ';' || c == ':') && !out.empty() &&
        out.back() == ' ') {
      out.pop_back();
    }
    if (c == '\n') {
      while (!out.empty() && out.back() == ' ') {
        out.pop_back();
      }
    }
    out.push_back(c);
  }
  // Drop orphaned punctuation left at the start of a line by removals
  std::string fixed;
  fixed.reserve(out.size());
  for (size_t i = 0; i < out.size(); ++i) {
    const bool line_start = fixed.empty() || fixed.back() == '\n';
    if (line_start && (out[i] == ',' || out[i] == ' ')) {
      continue;
    }
    fixed.push_back(out[i]);
  }
  // Remove doubled commas created by filler removal ("so, , then")
  std::string result;
  for (char c : fixed) {
    if (c == ',' && !result.empty() && result.back() == ',') {
      continue;
    }
    result.push_back(c);
  }
  return trim(result);
}

void capitalize_sentences(std::string &text) {
  bool start = true;
  for (char &c : text) {
    if (start && std::isalpha(static_cast<unsigned char>(c))) {
      c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
      start = false;
    } else if (c == '.' || c == '?' || c == '!' || c == '\n') {
      start = true;
    } else if (!std::isspace(static_cast<unsigned char>(c))) {
      start = false;
    }
  }
}

void apply_casual(std::string &text) {
  bool start = true;
  for (size_t i = 0; i < text.size(); ++i) {
    char &c = text[i];
    if (start && std::isalpha(static_cast<unsigned char>(c))) {
      // Keep "I" and acronyms ("API") intact.
      const bool next_upper =
          i + 1 < text.size() && std::isupper(static_cast<unsigned char>(text[i + 1]));
      const bool single_i = c == 'I' && (i + 1 >= text.size() || !is_word_char(text[i + 1]));
      if (!next_upper && !single_i) {
        c = lower(c);
      }
      start = false;
    } else if (c == '.' || c == '?' || c == '!' || c == '\n') {
      start = true;
    } else if (!std::isspace(static_cast<unsigned char>(c))) {
      start = false;
    }
  }
  if (!text.empty() && text.back() == '.') {
    text.pop_back();
  }
}

} // namespace

std::string strip_annotations(const std::string &text) {
  std::string out;
  out.reserve(text.size());
  int depth = 0;
  char open = 0;
  for (char c : text) {
    if (depth == 0 && (c == '[' || c == '(')) {
      open = c;
      depth = 1;
      continue;
    }
    if (depth > 0) {
      if ((open == '[' && c == ']') || (open == '(' && c == ')')) {
        depth = 0;
      }
      continue;
    }
    out.push_back(c);
  }
  // Unterminated bracket: keep the original text rather than dropping it.
  if (depth > 0) {
    return text;
  }
  return out;
}

std::string clean(const std::string &raw, const Options &options) {
  std::string text = strip_annotations(raw);
  for (char &c : text) {
    if (c == '\n' || c == '\r' || c == '\t') {
      c = ' ';
    }
  }

  if (options.remove_fillers) {
    static const char *kFillers[] = {"um",  "umm", "uh", "uhh", "uhm",
                                     "erm", "er",  "ah", "hmm", "mm"};
    for (const char *filler : kFillers) {
      text = replace_phrase(text, filler, "", true);
    }
  }

  // Snippets first so a trigger is not mangled by dictionary entries.
  for (const auto &snippet : options.snippets) {
    text = replace_phrase(text, snippet.trigger, snippet.text, true);
  }
  for (const auto &entry : options.dictionary) {
    text = replace_phrase(text, entry.from, entry.to, false);
  }

  if (options.voice_commands) {
    text = replace_phrase(text, "new paragraph", "\n\n", true);
    text = replace_phrase(text, "new line", "\n", true);
  }

  text = collapse_spaces(text);

  if (options.style == Style::Casual) {
    apply_casual(text);
  } else {
    capitalize_sentences(text);
  }
  return text;
}

std::string vocabulary_prompt(const std::vector<Replacement> &dictionary,
                              const std::vector<std::string> &extra_words) {
  std::set<std::string> seen;
  std::string prompt;
  auto add = [&](const std::string &word) {
    const std::string w = trim(word);
    if (w.empty() || !seen.insert(to_lower(w)).second || prompt.size() + w.size() > 600) {
      return;
    }
    if (!prompt.empty()) {
      prompt += ", ";
    }
    prompt += w;
  };
  for (const auto &entry : dictionary) {
    add(entry.to);
  }
  for (const auto &word : extra_words) {
    add(word);
  }
  return prompt.empty() ? "" : prompt + ".";
}

int count_words(const std::string &text) {
  int words = 0;
  bool in_word = false;
  for (char c : text) {
    if (std::isspace(static_cast<unsigned char>(c))) {
      in_word = false;
    } else if (!in_word) {
      in_word = true;
      ++words;
    }
  }
  return words;
}

} // namespace text_cleanup
