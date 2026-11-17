#include "gui/sounds.h"

#include <SDL2/SDL.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace gui {

namespace {

constexpr int kRate = 44100;
SDL_AudioDeviceID g_device = 0;

bool ensure_device() {
  if (g_device != 0) {
    return true;
  }
  if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
    return false;
  }
  SDL_AudioSpec want;
  SDL_zero(want);
  want.freq = kRate;
  want.format = AUDIO_F32SYS;
  want.channels = 1;
  want.samples = 1024;
  g_device = SDL_OpenAudioDevice(nullptr, 0, &want, nullptr, 0);
  if (g_device == 0) {
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    return false;
  }
  SDL_PauseAudioDevice(g_device, 0);
  return true;
}

void append_tone(std::vector<float> &out, float freq, float seconds, float volume) {
  const int n = static_cast<int>(seconds * kRate);
  for (int i = 0; i < n; ++i) {
    const float t = static_cast<float>(i) / kRate;
    // Soft attack and exponential decay so the chime is gentle.
    const float env = std::min(1.0f, t * 200.0f) * std::exp(-t * 18.0f);
    const float s = std::sin(2.0f * 3.14159265f * freq * t) +
                    0.25f * std::sin(2.0f * 3.14159265f * freq * 2.0f * t);
    out.push_back(s * env * volume);
  }
}

} // namespace

void play_chime(Chime chime) {
  if (!ensure_device()) {
    return;
  }
  std::vector<float> samples;
  switch (chime) {
  case Chime::Start:
    append_tone(samples, 660.0f, 0.09f, 0.18f);
    append_tone(samples, 990.0f, 0.16f, 0.18f);
    break;
  case Chime::Stop:
    append_tone(samples, 880.0f, 0.09f, 0.16f);
    append_tone(samples, 587.0f, 0.16f, 0.16f);
    break;
  case Chime::Error:
    append_tone(samples, 330.0f, 0.22f, 0.2f);
    break;
  }
  SDL_ClearQueuedAudio(g_device);
  SDL_QueueAudio(g_device, samples.data(), static_cast<Uint32>(samples.size() * sizeof(float)));
}

void shutdown_sounds() {
  if (g_device != 0) {
    SDL_CloseAudioDevice(g_device);
    g_device = 0;
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
  }
}

} // namespace gui
