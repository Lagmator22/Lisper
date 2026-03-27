#pragma once

#include <string>

namespace cli_style {

enum class Tone {
  Accent,
  Success,
  Warning,
  Error,
  Muted,
};

// Configure terminal styling behavior for this run.
void initialize(bool enable_colors, bool enable_animations);

// Returns true when ANSI color output is enabled.
bool colors_enabled();

// Returns true when lightweight terminal animations are enabled.
bool animations_enabled();

// Wrap text with ANSI color when enabled.
std::string style(const std::string &text, Tone tone);

// Apply the Lisper multicolor gradient to a line of text.
std::string gradient_text(const std::string &text, bool bold = true);

// Render a compact status chip for CLI summaries.
std::string badge(const std::string &text, Tone tone);

// Render a decorative separator line.
std::string divider(size_t width = 72);

// Print an animated startup banner for interactive terminals.
void show_startup_banner();

class Spinner {
public:
  explicit Spinner(std::string message);
  ~Spinner();

  // Non-copyable due to thread ownership
  Spinner(const Spinner &) = delete;
  Spinner &operator=(const Spinner &) = delete;

  void success(const std::string &message = "");
  void fail(const std::string &message = "");

private:
  std::string message_;
  bool running_ = false;
  bool finished_ = false;
  class Impl;
  Impl *impl_ = nullptr;

  void stop(const std::string &symbol, const std::string &message, Tone tone);
};

} // namespace cli_style
