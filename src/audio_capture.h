#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

// Microphone capture at 16 kHz mono float, as whisper expects. Available only
// when built with LISPER_ENABLE_LIVE (SDL2); otherwise every call fails with a
// descriptive error.
class AudioCapture {
public:
  AudioCapture();
  ~AudioCapture();

  AudioCapture(const AudioCapture &) = delete;
  AudioCapture &operator=(const AudioCapture &) = delete;

  static bool supported();
  static std::vector<std::string> list_input_devices();

  // Open `device_name` (empty = system default) and start recording.
  // Any previous recording is discarded.
  bool start(const std::string &device_name, int max_seconds, std::string &error);

  // Stop recording and return the captured samples.
  std::vector<float> stop();

  // Abort and discard.
  void cancel();

  bool recording() const { return recording_.load(); }

  // Smoothed input level in [0, 1] for meters and waveform animation.
  float level() const { return level_.load(); }

  // Recent per-block peak levels (oldest first) for drawing a waveform.
  std::vector<float> recent_levels(size_t count) const;

  int64_t recorded_ms() const;

  // Copy of the most recent `max_samples` samples recorded so far, for live
  // preview transcription while recording continues.
  std::vector<float> snapshot(size_t max_samples) const;

  // Internal: called from the audio thread.
  void on_samples(const float *samples, int count);

private:
  void close_device();

  mutable std::mutex mutex_;
  std::vector<float> buffer_;
  std::vector<float> levels_;
  size_t max_samples_ = 0;
  std::atomic<bool> recording_{false};
  std::atomic<float> level_{0.0f};
  uint32_t device_ = 0;
  bool sdl_audio_initialized_ = false;
};
