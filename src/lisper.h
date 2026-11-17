#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

struct Segment {
  int64_t start_ms;
  int64_t end_ms;
  std::string text;
};

struct TranscriptionResult {
  std::string full_text;
  std::vector<Segment> segments;
  std::string detected_language;
  int duration_ms = 0;
  bool success = false;
  std::string error;
};

// Per-call overrides for a single decode.
struct DecodeOptions {
  std::string language; // empty = use the engine's configured language
  int translate = -1;   // -1 = engine default, 0 = off, 1 = translate to English
  bool single_segment = false;
  std::string prompt; // empty = use config.initial_prompt
  // Optional per-call cancellation flag, checked alongside the global
  // interrupt_state so independent jobs can be cancelled separately.
  const std::atomic<bool> *cancel = nullptr;
  // Encoder context in frames (50 per second of audio). 0 = full 30 s window.
  // Smaller values are much faster for short clips at a small accuracy cost,
  // which suits live previews.
  int audio_ctx = 0;
};

struct LisperConfig {
  enum class Device {
    Auto,
    CPU,
    GPU,
  };

  std::string model_path;
  std::string language = "en";
  int threads = 4;
  bool translate = false;
  Device device = Device::Auto;
  int gpu_device = 0;
  bool flash_attn = true;
  // Optional text that biases decoding toward specific vocabulary (names,
  // jargon). Used for the personal dictionary in dictation mode.
  std::string initial_prompt;
};

class Lisper {
public:
  explicit Lisper(const LisperConfig &config);
  ~Lisper();

  Lisper(const Lisper &) = delete;
  Lisper &operator=(const Lisper &) = delete;

  // Transcribe a 16 kHz mono 16-bit PCM WAV file (see media::prepare_audio).
  TranscriptionResult transcribe(const std::string &audio_path);

  // Transcribe raw 16 kHz mono float samples in [-1, 1]. `prompt` overrides
  // config.initial_prompt when non-empty. Thread-safe: calls are serialized.
  TranscriptionResult transcribe_pcm(const std::vector<float> &pcm, bool single_segment = false,
                                     const std::string &prompt = "");
  TranscriptionResult transcribe_pcm(const std::vector<float> &pcm, const DecodeOptions &options);
  TranscriptionResult transcribe(const std::string &audio_path, const DecodeOptions &options);

  bool is_loaded() const;
  int threads() const { return config_.threads; }
  bool translate() const { return config_.translate; }
  const std::string &language() const { return config_.language; }

  const LisperConfig &config() const { return config_; }
  void set_initial_prompt(const std::string &prompt);

private:
  struct whisper_context *ctx_ = nullptr;
  LisperConfig config_;
  std::mutex mutex_;

  bool load_wav(const std::string &path, std::vector<float> &pcm, int &sample_rate);
};
