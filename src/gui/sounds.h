#pragma once

namespace gui {

enum class Chime { Start, Stop, Error };

// Short synthesized feedback tones played on the default output device.
// Non-blocking; failures are silent.
void play_chime(Chime chime);

// Release the output device (call once at shutdown).
void shutdown_sounds();

} // namespace gui
