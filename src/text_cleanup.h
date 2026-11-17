#pragma once

#include <string>
#include <vector>

// Post-processing applied to dictated text before it is inserted, in the
// spirit of Wispr Flow: drop filler words, honour spoken formatting commands,
// apply the personal dictionary and expand snippets.
namespace text_cleanup {

struct Replacement {
  std::string from; // phrase as whisper tends to hear it ("lisa per")
  std::string to;   // what should be written ("Lisper")
};

struct Snippet {
  std::string trigger; // spoken cue ("my calendar link")
  std::string text;    // expansion ("https://cal.example.com/me")
};

enum class Style {
  Formal, // keep whisper's punctuation and capitalization
  Casual, // lowercase sentence starts, drop the final period
};

struct Options {
  bool remove_fillers = true;
  bool voice_commands = true; // "new line", "new paragraph"
  Style style = Style::Formal;
  std::vector<Replacement> dictionary;
  std::vector<Snippet> snippets;
};

// Remove whisper annotations like [BLANK_AUDIO], (music) or [ Silence ].
std::string strip_annotations(const std::string &text);

// Full cleanup pipeline.
std::string clean(const std::string &raw, const Options &options);

// Build an initial prompt for whisper from dictionary targets so rare names
// and jargon are spelled correctly more often.
std::string vocabulary_prompt(const std::vector<Replacement> &dictionary,
                              const std::vector<std::string> &extra_words = {});

int count_words(const std::string &text);

} // namespace text_cleanup
