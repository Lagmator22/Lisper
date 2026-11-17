#include "audio_capture.h"

#include <algorithm>
#include <cmath>

#ifdef LISPER_ENABLE_LIVE
#include <SDL2/SDL.h>
#endif

namespace {
constexpr int kSampleRate = 16000;
constexpr size_t kMaxLevels = 512;
} // namespace

AudioCapture::AudioCapture() = default;

AudioCapture::~AudioCapture() {
  cancel();
}

#ifdef LISPER_ENABLE_LIVE

namespace {

void sdl_callback(void *userdata, Uint8 *stream, int len) {
  auto *self = static_cast<AudioCapture *>(userdata);
  self->on_samples(reinterpret_cast<const float *>(stream), len / static_cast<int>(sizeof(float)));
}

} // namespace

bool AudioCapture::supported() {
  return true;
}

std::vector<std::string> AudioCapture::list_input_devices() {
  std::vector<std::string> out;
  const bool was_init = SDL_WasInit(SDL_INIT_AUDIO) != 0;
  if (!was_init && SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
    return out;
  }
  const int n = SDL_GetNumAudioDevices(1);
  for (int i = 0; i < n; ++i) {
    const char *name = SDL_GetAudioDeviceName(i, 1);
    if (name != nullptr) {
      out.emplace_back(name);
    }
  }
  if (!was_init) {
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
  }
  return out;
}

bool AudioCapture::start(const std::string &device_name, int max_seconds, std::string &error) {
  cancel();

  if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
    error = std::string("Could not initialise audio: ") + SDL_GetError();
    return false;
  }
  sdl_audio_initialized_ = true;

  {
    std::lock_guard<std::mutex> lock(mutex_);
    buffer_.clear();
    levels_.clear();
    max_samples_ = static_cast<size_t>(std::max(1, max_seconds)) * kSampleRate;
    buffer_.reserve(std::min<size_t>(max_samples_, static_cast<size_t>(kSampleRate) * 60));
  }

  SDL_AudioSpec want;
  SDL_AudioSpec have;
  SDL_zero(want);
  want.freq = kSampleRate;
  want.format = AUDIO_F32SYS;
  want.channels = 1;
  want.samples = 512;
  want.callback = sdl_callback;
  want.userdata = this;

  // Let SDL convert from the device's native format to 16 kHz mono float.
  device_ =
      SDL_OpenAudioDevice(device_name.empty() ? nullptr : device_name.c_str(), 1, &want, &have, 0);
  if (device_ == 0) {
    error = std::string("Could not open microphone: ") + SDL_GetError();
    close_device();
    return false;
  }

  recording_ = true;
  SDL_PauseAudioDevice(device_, 0);
  return true;
}

void AudioCapture::close_device() {
  if (device_ != 0) {
    SDL_CloseAudioDevice(device_);
    device_ = 0;
  }
  if (sdl_audio_initialized_) {
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    sdl_audio_initialized_ = false;
  }
}

#else // !LISPER_ENABLE_LIVE

bool AudioCapture::supported() {
  return false;
}

std::vector<std::string> AudioCapture::list_input_devices() {
  return {};
}

bool AudioCapture::start(const std::string &, int, std::string &error) {
  error = "Microphone capture is not available: Lisper was built without SDL2";
  return false;
}

void AudioCapture::close_device() {}

#endif

std::vector<float> AudioCapture::stop() {
  recording_ = false;
  close_device(); // joins SDL's audio thread, so no more callbacks after this
  std::lock_guard<std::mutex> lock(mutex_);
  level_ = 0.0f;
  std::vector<float> out;
  out.swap(buffer_);
  return out;
}

void AudioCapture::cancel() {
  stop();
}

void AudioCapture::on_samples(const float *samples, int count) {
  if (!recording_ || count <= 0) {
    return;
  }

  float peak = 0.0f;
  double sum_sq = 0.0;
  for (int i = 0; i < count; ++i) {
    const float v = samples[i];
    peak = std::max(peak, std::fabs(v));
    sum_sq += static_cast<double>(v) * v;
  }
  const float rms = static_cast<float>(std::sqrt(sum_sq / count));
  // Perceptual-ish scaling so normal speech fills most of the meter.
  const float scaled = std::clamp(std::sqrt(rms) * 2.2f, 0.0f, 1.0f);
  level_ = level_.load() * 0.6f + scaled * 0.4f;

  std::lock_guard<std::mutex> lock(mutex_);
  const size_t room = max_samples_ > buffer_.size() ? max_samples_ - buffer_.size() : 0;
  const size_t take = std::min(room, static_cast<size_t>(count));
  buffer_.insert(buffer_.end(), samples, samples + take);
  levels_.push_back(std::clamp(std::sqrt(peak) * 1.4f, 0.0f, 1.0f));
  if (levels_.size() > kMaxLevels) {
    levels_.erase(levels_.begin(),
                  levels_.begin() + static_cast<long>(levels_.size() - kMaxLevels));
  }
}

std::vector<float> AudioCapture::recent_levels(size_t count) const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (levels_.size() <= count) {
    std::vector<float> out(count - levels_.size(), 0.0f);
    out.insert(out.end(), levels_.begin(), levels_.end());
    return out;
  }
  return std::vector<float>(levels_.end() - static_cast<long>(count), levels_.end());
}

int64_t AudioCapture::recorded_ms() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return static_cast<int64_t>(buffer_.size()) * 1000 / kSampleRate;
}

std::vector<float> AudioCapture::snapshot(size_t max_samples) const {
  std::lock_guard<std::mutex> lock(mutex_);
  const size_t n = std::min(max_samples, buffer_.size());
  return std::vector<float>(buffer_.end() - static_cast<long>(n), buffer_.end());
}
