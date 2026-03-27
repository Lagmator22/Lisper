#pragma once

#include "lisper.h"
#include <string>

#ifdef LISPER_ENABLE_LIVE

namespace live {

// Start capturing from default microphone and transcribe in real-time.
// Blocks until interrupted by Ctrl+C.
void start_live_transcription(Lisper &engine);

} // namespace live

#endif // LISPER_ENABLE_LIVE
