#include "live.h"

#ifdef LISPER_ENABLE_LIVE

#include "interrupt.h"
#include "whisper.h"

#include <SDL2/SDL.h>
#include <chrono>
#include <cstring>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

namespace live {

// 16kHz, mono, 32-bit float (whisper.cpp expectation)
static const int SAMPLE_RATE = 16000;
static const int CHANNELS = 1;

struct AudioState {
  std::mutex mutex;
  std::vector<float> audio_buffer;
};

// SDL audio callback
static void audio_callback(void *userdata, Uint8 *stream, int len) {
  AudioState *state = reinterpret_cast<AudioState *>(userdata);
  float *float_stream = reinterpret_cast<float *>(stream);
  int num_samples = len / sizeof(float);

  std::lock_guard<std::mutex> lock(state->mutex);
  state->audio_buffer.insert(state->audio_buffer.end(), float_stream, float_stream + num_samples);
}

void start_live_transcription(Lisper &engine) {
  if (SDL_Init(SDL_INIT_AUDIO) < 0) {
    std::cerr << "SDL_Init Error: " << SDL_GetError() << "\n";
    return;
  }

  AudioState state;

  SDL_AudioSpec want, have;
  SDL_zero(want);
  want.freq = SAMPLE_RATE;
  want.format = AUDIO_F32SYS;
  want.channels = CHANNELS;
  want.samples = 1024; // buffer size
  want.callback = audio_callback;
  want.userdata = &state;

  SDL_AudioDeviceID dev = SDL_OpenAudioDevice(NULL, 1, &want, &have, 0);
  if (dev == 0) {
    std::cerr << "Failed to open audio device: " << SDL_GetError() << "\n";
    SDL_Quit();
    return;
  }

  if (have.format != want.format || have.channels != want.channels || have.freq != want.freq) {
    std::cerr << "Warning: Could not get exact audio format requested.\n";
  }

  std::cout << "\n[LIVE TRANSCRIPTION STARTED]\n";
  std::cout << "Listening... (Press Ctrl+C to stop, wait 2-3s for the first "
               "output)\n\n";

  SDL_PauseAudioDevice(dev, 0); // start capturing

  // Capture in a sliding window approach
  // We capture ~2 seconds of audio at a time, then transcribe it
  const int step_ms = 2000;
  const int step_samples = (SAMPLE_RATE * step_ms) / 1000;

  std::vector<float> process_buffer;

  while (!interrupt_state::is_interrupted()) {
    for (int i = 0; i < step_ms / 100 && !interrupt_state::is_interrupted(); ++i) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (interrupt_state::is_interrupted()) {
      break;
    }

    {
      std::lock_guard<std::mutex> lock(state.mutex);
      if (state.audio_buffer.size() < step_samples) {
        continue; // Not enough data yet
      }

      // Move buffered samples to local processing buffer
      process_buffer = state.audio_buffer;
      state.audio_buffer.clear();
    }

    // process_buffer is guaranteed non-empty after the copy above
    whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    params.print_realtime = false;
    params.print_progress = false;
    params.print_timestamps = false;
    params.print_special = false;
    params.single_segment = true;
    params.translate = engine.translate();
    params.n_threads = engine.threads();
    params.language = engine.language().c_str();
    params.abort_callback = [](void *) { return interrupt_state::is_interrupted(); };
    params.abort_callback_user_data = nullptr;

    // Re-use engine's whisper context
    int ret = whisper_full(engine.get_ctx(), params, process_buffer.data(),
                           static_cast<int>(process_buffer.size()));
    if (ret != 0 && interrupt_state::is_interrupted()) {
      break;
    }
    if (ret == 0) {
      int n_segments = whisper_full_n_segments(engine.get_ctx());
      for (int i = 0; i < n_segments; ++i) {
        const char *text = whisper_full_get_segment_text(engine.get_ctx(), i);
        if (text && strlen(text) > 0) {
          std::string clean_text = text;
          // trim leading whitespace
          size_t first = clean_text.find_first_not_of(" \t\r\n");
          if (first != std::string::npos) {
            clean_text = clean_text.substr(first);
          }
          if (!clean_text.empty() && clean_text != "[BLANK_AUDIO]") {
            std::cout << clean_text << " " << std::flush;
          }
        }
      }
    }
  }

  SDL_CloseAudioDevice(dev);
  SDL_Quit();
}

} // namespace live

#endif // LISPER_ENABLE_LIVE
