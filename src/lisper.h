#pragma once

#include <cstdint>
#include <functional>
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
  bool print_progress = true;
  Device device = Device::Auto;
  int gpu_device = 0;
  bool flash_attn = true;
};

class Lisper {
public:
  explicit Lisper(const LisperConfig &config);
  ~Lisper();

  Lisper(const Lisper &) = delete;
  Lisper &operator=(const Lisper &) = delete;

  TranscriptionResult transcribe(const std::string &audio_path);

  bool is_loaded() const;
  int threads() const { return config_.threads; }
  bool translate() const { return config_.translate; }
  const std::string &language() const { return config_.language; }

  struct whisper_context *get_ctx() const { return ctx_; }

private:
  struct whisper_context *ctx_ = nullptr;
  LisperConfig config_;

  bool load_wav(const std::string &path, std::vector<float> &pcm, int &sample_rate);
};
