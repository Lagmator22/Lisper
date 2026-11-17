#include "live.h"

#ifdef LISPER_ENABLE_LIVE

#include "interrupt.h"

#include <SDL2/SDL.h>
#include <chrono>
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
  // Bound memory if transcription falls behind real time: keep the newest 30s.
  constexpr size_t kMaxBuffered = static_cast<size_t>(SAMPLE_RATE) * 30;
  if (state->audio_buffer.size() > kMaxBuffered) {
    state->audio_buffer.erase(state->audio_buffer.begin(),
                              state->audio_buffer.end() - static_cast<long>(kMaxBuffered));
  }
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

    const TranscriptionResult result = engine.transcribe_pcm(process_buffer, true);
    if (!result.success) {
      if (interrupt_state::is_interrupted()) {
        break;
      }
      continue;
    }
    for (const auto &segment : result.segments) {
      std::string clean_text = segment.text;
      const size_t first = clean_text.find_first_not_of(" \t\r\n");
      if (first == std::string::npos) {
        continue;
      }
      clean_text.erase(0, first);
      if (clean_text != "[BLANK_AUDIO]") {
        std::cout << clean_text << " " << std::flush;
      }
    }
  }

  SDL_CloseAudioDevice(dev);
  SDL_Quit();
}

} // namespace live

#endif // LISPER_ENABLE_LIVE
