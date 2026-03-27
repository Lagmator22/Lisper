#include "cli_style.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <io.h>
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace {

bool g_colors_enabled = false;
bool g_animations_enabled = false;
bool g_banner_shown = false;

bool stdout_is_tty() {
#ifdef _WIN32
  return _isatty(_fileno(stdout));
#else
  return isatty(fileno(stdout));
#endif
}

bool enable_windows_vt_processing() {
#ifdef _WIN32
  HANDLE h_out = GetStdHandle(STD_OUTPUT_HANDLE);
  if (h_out == INVALID_HANDLE_VALUE) {
    return false;
  }

  DWORD mode = 0;
  if (!GetConsoleMode(h_out, &mode)) {
    return false;
  }

  mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
  if (!SetConsoleMode(h_out, mode)) {
    return false;
  }
#endif
  return true;
}

const char *tone_code(cli_style::Tone tone) {
  switch (tone) {
  case cli_style::Tone::Accent:
    return "\033[1;38;2;68;147;248m";
  case cli_style::Tone::Success:
    return "\033[1;38;2;63;185;80m";
  case cli_style::Tone::Warning:
    return "\033[1;38;2;210;153;34m";
  case cli_style::Tone::Error:
    return "\033[1;38;2;248;81;73m";
  case cli_style::Tone::Muted:
    return "\033[38;2;145;152;161m";
  }
  return "\033[0m";
}

struct Rgb {
  int r;
  int g;
  int b;
};

std::string rgb_code(const Rgb &color, bool bold = false) {
  std::ostringstream out;
  out << "\033[" << (bold ? "1;" : "") << "38;2;" << color.r << ";" << color.g << ";" << color.b
      << "m";
  return out.str();
}

Rgb blend(const Rgb &a, const Rgb &b, float t) {
  const float clamped = std::clamp(t, 0.0f, 1.0f);
  return {
      static_cast<int>(std::round(a.r + (b.r - a.r) * clamped)),
      static_cast<int>(std::round(a.g + (b.g - a.g) * clamped)),
      static_cast<int>(std::round(a.b + (b.b - a.b) * clamped)),
  };
}

Rgb palette_color(float t) {
  static const std::vector<Rgb> kPalette = {
      {255, 120, 70},
      {255, 196, 84},
      {109, 224, 213},
      {94, 173, 255},
  };

  if (kPalette.size() == 1) {
    return kPalette.front();
  }

  const float scaled = std::clamp(t, 0.0f, 1.0f) * (kPalette.size() - 1);
  const size_t index = static_cast<size_t>(scaled);
  const size_t next_index = std::min(index + 1, kPalette.size() - 1);
  return blend(kPalette[index], kPalette[next_index], scaled - static_cast<float>(index));
}

std::string gradient_line(const std::string &line, float shimmer_center) {
  if (!g_colors_enabled) {
    return line;
  }

  std::vector<std::string> chars;
  for (size_t i = 0; i < line.size();) {
    size_t char_len = 1;
    unsigned char c = line[i];
    if (c >= 0xC0) {
      if ((c & 0xE0) == 0xC0)
        char_len = 2;
      else if ((c & 0xF0) == 0xE0)
        char_len = 3;
      else if ((c & 0xF8) == 0xF0)
        char_len = 4;
    }
    if (i + char_len > line.size()) {
      char_len = line.size() - i;
    }
    chars.push_back(line.substr(i, char_len));
    i += char_len;
  }

  std::ostringstream out;
  const float width = static_cast<float>(std::max<size_t>(1, chars.size() - 1));

  for (size_t i = 0; i < chars.size(); ++i) {
    const float position = static_cast<float>(i) / width;
    Rgb color = palette_color(position);

    const float dist = std::fabs(static_cast<float>(i) - shimmer_center);
    if (dist < 5.5f) {
      color = blend(color, {255, 255, 255}, (5.5f - dist) / 5.5f * 0.55f);
    }

    out << rgb_code(color, chars[i] != " ") << chars[i];
  }

  out << "\033[0m";
  return out.str();
}

size_t terminal_width() {
#ifdef _WIN32
  CONSOLE_SCREEN_BUFFER_INFO info;
  if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info)) {
    const int width = info.srWindow.Right - info.srWindow.Left + 1;
    if (width > 0) {
      return static_cast<size_t>(width);
    }
  }
#else
  winsize size{};
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0 && size.ws_col > 0) {
    return static_cast<size_t>(size.ws_col);
  }
#endif
  if (const char *env = std::getenv("COLUMNS")) {
    const long parsed = std::strtol(env, nullptr, 10);
    if (parsed > 0) {
      return static_cast<size_t>(parsed);
    }
  }
  return 100;
}

size_t fit_banner_width(size_t preferred) {
  const size_t width = terminal_width();
  if (width <= 8) {
    return preferred;
  }
  return std::max<size_t>(30, std::min(preferred, width - 6));
}

std::vector<std::string> build_banner_frame(float shimmer_center) {
  static const std::string kLogoRaw = R"(  ╔══════════════════════════════════════════════════╗
  ║  ██       ██  ███████  ██████   ███████  ██████  ║
  ║  ██       ██  ██       ██   ██  ██       ██   ██ ║
  ║  ██       ██  ███████  ██████   █████    ██████  ║
  ║  ██       ██       ██  ██       ██       ██   ██ ║
  ║  ████████ ██  ███████  ██       ███████  ██   ██ ║
  ╚══════════════════════════════════════════════════╝)";

  const bool compact = terminal_width() < 45;
  const size_t rule_width = fit_banner_width(compact ? 44 : 62);
  const std::string rule(rule_width, '-');

  std::vector<std::string> lines;
  lines.push_back("  " + gradient_line(rule, shimmer_center + 2.0f));

  if (compact) {
    lines.push_back("  " + cli_style::gradient_text("LISPER", true));
    lines.push_back("  " + cli_style::style("Local transcription studio", cli_style::Tone::Muted));
    lines.push_back("  " + cli_style::badge("local-first", cli_style::Tone::Accent) + " " +
                    cli_style::badge("cpu/gpu", cli_style::Tone::Success));
    lines.push_back(
        "  " + cli_style::style("CLI  ", cli_style::Tone::Muted) +
        cli_style::gradient_text("./build/lisper --model-profile quality recording.wav", false));
    lines.push_back("  " + cli_style::style("GUI  ", cli_style::Tone::Muted) +
                    cli_style::gradient_text("./build/lisper_gui", false));
  } else {
    std::istringstream stream(kLogoRaw);
    std::string logo_line;
    int line_idx = 0;
    while (std::getline(stream, logo_line)) {
      if (!logo_line.empty()) {
        lines.push_back(
            gradient_line(logo_line, shimmer_center + static_cast<float>(line_idx) * 3.4f));
        line_idx++;
      }
    }
    lines.push_back("  " + cli_style::style("Local transcription studio", cli_style::Tone::Muted));
    lines.push_back("  " + cli_style::badge("local-first", cli_style::Tone::Accent) + " " +
                    cli_style::badge("cpu + gpu aware", cli_style::Tone::Success) + " " +
                    cli_style::badge("desktop gui", cli_style::Tone::Warning));
    lines.push_back("  " + cli_style::style("Private Whisper transcription with "
                                            "better defaults and zero cloud detours.",
                                            cli_style::Tone::Muted));
    lines.push_back(
        "  " + cli_style::style("CLI   ", cli_style::Tone::Muted) +
        cli_style::gradient_text("./build/lisper --model-profile quality recording.wav", false));
    lines.push_back(
        "  " + cli_style::style("LIVE  ", cli_style::Tone::Muted) +
        cli_style::gradient_text("./build/lisper --model-profile quality --live", false));
    lines.push_back("  " + cli_style::style("GUI   ", cli_style::Tone::Muted) +
                    cli_style::gradient_text("./build/lisper_gui", false));
  }

  lines.push_back("  " + gradient_line(rule, shimmer_center + 11.0f));
  return lines;
}

void print_banner_frame(float shimmer_center) {
  const auto lines = build_banner_frame(shimmer_center);
  for (const auto &line : lines) {
    std::cout << "\033[2K\r" << line << "\n";
  }
}

} // namespace

namespace cli_style {

void initialize(bool enable_colors, bool enable_animations) {
  const bool tty = stdout_is_tty();
  g_colors_enabled = enable_colors && tty && enable_windows_vt_processing();
  g_animations_enabled = enable_animations && tty;
}

bool colors_enabled() {
  return g_colors_enabled;
}

bool animations_enabled() {
  return g_animations_enabled;
}

std::string style(const std::string &text, Tone tone) {
  if (!g_colors_enabled) {
    return text;
  }
  return std::string(tone_code(tone)) + text + "\033[0m";
}

std::string gradient_text(const std::string &text, bool bold) {
  if (!g_colors_enabled) {
    return text;
  }

  std::ostringstream out;
  const float width = static_cast<float>(std::max<size_t>(1, text.size() - 1));
  for (size_t i = 0; i < text.size(); ++i) {
    if (text[i] == ' ') {
      out << text[i];
      continue;
    }
    out << rgb_code(palette_color(static_cast<float>(i) / width), bold) << text[i];
  }
  out << "\033[0m";
  return out.str();
}

std::string badge(const std::string &text, Tone tone) {
  if (!g_colors_enabled) {
    return "[" + text + "]";
  }
  return style("[ " + text + " ]", tone);
}

std::string divider(size_t width) {
  const std::string line(std::min(width, fit_banner_width(width)), '-');
  return gradient_text(line, false);
}

void show_startup_banner() {
  if (!stdout_is_tty() || g_banner_shown) {
    return;
  }
  g_banner_shown = true;

  if (!animations_enabled()) {
    print_banner_frame(200.0f);
    return;
  }

  static const int kFrames = 7;
  const int banner_lines = static_cast<int>(build_banner_frame(0.0f).size());

  std::cout << "\033[?25l";
  for (int frame = 0; frame < kFrames; ++frame) {
    if (frame > 0) {
      std::cout << "\033[" << banner_lines << "F";
    }
    const float shimmer_center = 4.0f + static_cast<float>(frame) * 6.5f;
    print_banner_frame(shimmer_center);
    std::cout.flush();
    std::this_thread::sleep_for(std::chrono::milliseconds(46));
  }
  std::cout << "\033[?25h";
}

class Spinner::Impl {
public:
  std::atomic<bool> running{false};
  std::thread worker;
};

Spinner::Spinner(std::string message) : message_(std::move(message)) {
  if (!animations_enabled()) {
    std::cout << style(message_, Tone::Accent) << "\n";
    return;
  }

  impl_ = new Impl();
  running_ = true;
  impl_->running = true;

  impl_->worker = std::thread([this]() {
    static const char *frames[] = {"[    ]", "[=   ]", "[==  ]", "[=== ]",
                                   "[ ===]", "[  ==]", "[   =]"};
    size_t idx = 0;

    while (impl_->running) {
      std::cout << "\r" << style(frames[idx % (sizeof(frames) / sizeof(frames[0]))], Tone::Accent)
                << " " << style(message_, Tone::Accent) << std::flush;
      idx++;
      std::this_thread::sleep_for(std::chrono::milliseconds(90));
    }
  });
}

void Spinner::stop(const std::string &symbol, const std::string &message, Tone tone) {
  if (finished_) {
    return;
  }
  finished_ = true;

  if (!animations_enabled()) {
    if (!message.empty()) {
      std::cout << style(message, tone) << "\n";
    }
    return;
  }

  if (impl_) {
    impl_->running = false;
    if (impl_->worker.joinable()) {
      impl_->worker.join();
    }
  }

  const std::string final_message = message.empty() ? message_ : message;
  std::cout << "\r" << style(symbol, tone) << " " << style(final_message, tone) << "          \n";
}

void Spinner::success(const std::string &message) {
  stop("OK", message, Tone::Success);
}

void Spinner::fail(const std::string &message) {
  stop("!!", message, Tone::Error);
}

Spinner::~Spinner() {
  if (!finished_) {
    stop("..", message_, Tone::Muted);
  }
  if (impl_) {
    delete impl_;
    impl_ = nullptr;
  }
}

} // namespace cli_style
