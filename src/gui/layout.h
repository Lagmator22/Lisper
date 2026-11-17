#pragma once

#include "gui/background_transcriber.h"
#include "gui/dictation.h"
#include "gui/state.h"
#include "gui/theme.h"

#include <functional>
#include <string>

namespace gui {

// Everything the pages need from the application shell.
struct UiContext {
  AppState &state;
  const Fonts &fonts;
  BackgroundTranscriber &job;
  Dictation &dictation;
  std::function<void()> rebind_hotkey;
  std::function<void(const std::string &profile)> download_model;
  std::function<void(const std::string &text)> copy_text;
  std::function<void()> request_accessibility;
};

void render_main_window(UiContext &ctx);

} // namespace gui
