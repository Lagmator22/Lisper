#pragma once

#include "gui/background_transcriber.h"
#include "gui/state.h"
#include "gui/theme.h"

namespace gui {

void render_main_window(AppState &state, const Fonts &fonts,
                        BackgroundTranscriber &job);

} // namespace gui
