#include "lisper.h"
#include "whisper.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <numeric>

Lisper::Lisper(const LisperConfig &config) : config_(config) {
  whisper_context_params cparams = whisper_context_default_params();
  ctx_ =
      whisper_init_from_file_with_params(config_.model_path.c_str(), cparams);
  if (!ctx_) {
    std::cerr << "Failed to load model: " << config_.model_path << "\n";
  }
}

Lisper::~Lisper() {
  if (ctx_) {
    whisper_free(ctx_);
  }
}

bool Lisper::is_loaded() const { return ctx_ != nullptr; }

bool Lisper::load_wav(const std::string &path, std::vector<float> &pcm,
                      int &sample_rate) {
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    return false;
  }

  // read the RIFF header
  char riff[4];
  file.read(riff, 4);
  if (std::strncmp(riff, "RIFF", 4) != 0) {
    return false;
  }

  uint32_t chunk_size;
  file.read(reinterpret_cast<char *>(&chunk_size), 4);

  char wave[4];
  file.read(wave, 4);
  if (std::strncmp(wave, "WAVE", 4) != 0) {
    return false;
  }

  // find the fmt and data chunks
  int16_t audio_format = 0;
  int16_t num_channels = 0;
  int32_t sr = 0;
  int16_t bits_per_sample = 0;

  while (file.good()) {
    char sub_id[4];
    uint32_t sub_size;
    file.read(sub_id, 4);
    file.read(reinterpret_cast<char *>(&sub_size), 4);

    if (!file.good())
      break;

    if (std::strncmp(sub_id, "fmt ", 4) == 0) {
      file.read(reinterpret_cast<char *>(&audio_format), 2);
      file.read(reinterpret_cast<char *>(&num_channels), 2);
      file.read(reinterpret_cast<char *>(&sr), 4);

      uint32_t byte_rate;
      file.read(reinterpret_cast<char *>(&byte_rate), 4);

      uint16_t block_align;
      file.read(reinterpret_cast<char *>(&block_align), 2);

      file.read(reinterpret_cast<char *>(&bits_per_sample), 2);

      // skip any extra fmt bytes
      if (sub_size > 16) {
        file.seekg(sub_size - 16, std::ios::cur);
      }
    } else if (std::strncmp(sub_id, "data", 4) == 0) {
      sample_rate = sr;

      if (audio_format != 1 || bits_per_sample != 16) {
        std::cerr << "Only 16-bit PCM WAV is supported.\n";
        return false;
      }

      int num_samples = sub_size / (bits_per_sample / 8);
      std::vector<int16_t> raw(num_samples);
      file.read(reinterpret_cast<char *>(raw.data()), sub_size);

      // convert to mono float32 normalized to [-1, 1]
      if (num_channels == 1) {
        pcm.resize(num_samples);
        for (int i = 0; i < num_samples; i++) {
          pcm[i] = static_cast<float>(raw[i]) / 32768.0f;
        }
      } else {
        // downmix to mono by averaging channels
        int mono_samples = num_samples / num_channels;
        pcm.resize(mono_samples);
        for (int i = 0; i < mono_samples; i++) {
          float sum = 0.0f;
          for (int ch = 0; ch < num_channels; ch++) {
            sum += static_cast<float>(raw[i * num_channels + ch]);
          }
          pcm[i] = (sum / num_channels) / 32768.0f;
        }
      }
      return true;
    } else {
      file.seekg(sub_size, std::ios::cur);
    }
  }

  return false;
}

TranscriptionResult Lisper::transcribe(const std::string &audio_path) {
  TranscriptionResult result;

  if (!ctx_) {
    result.error = "Model not loaded";
    return result;
  }

  std::vector<float> pcm;
  int sample_rate = 0;

  if (!load_wav(audio_path, pcm, sample_rate)) {
    result.error = "Failed to read audio file: " + audio_path;
    return result;
  }

  // whisper expects 16kHz mono. if the wav is at a different rate we should
  // have already resampled via ffmpeg, but warn just in case.
  if (sample_rate != WHISPER_SAMPLE_RATE) {
    std::cerr << "Warning: sample rate is " << sample_rate << "Hz, expected "
              << WHISPER_SAMPLE_RATE << "Hz.\n"
              << "Use FFmpeg to convert: ffmpeg -i input -ar 16000 -ac 1 "
                 "output.wav\n";
  }

  result.duration_ms =
      static_cast<int>((pcm.size() * 1000) / WHISPER_SAMPLE_RATE);

  whisper_full_params params =
      whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
  params.print_realtime = false;
  params.print_progress = config_.print_progress;
  params.print_timestamps = false;
  params.print_special = false;
  params.translate = config_.translate;
  params.language = config_.language.c_str();
  params.n_threads = config_.threads;
  params.token_timestamps = true;

  int ret =
      whisper_full(ctx_, params, pcm.data(), static_cast<int>(pcm.size()));
  if (ret != 0) {
    result.error = "Transcription failed with code " + std::to_string(ret);
    return result;
  }

  int n_segments = whisper_full_n_segments(ctx_);
  std::string full_text;

  for (int i = 0; i < n_segments; i++) {
    const char *text = whisper_full_get_segment_text(ctx_, i);
    int64_t t0 = whisper_full_get_segment_t0(ctx_, i);
    int64_t t1 = whisper_full_get_segment_t1(ctx_, i);

    Segment seg;
    seg.start_ms = t0 * 10; // whisper timestamps are in centiseconds
    seg.end_ms = t1 * 10;
    seg.text = text ? text : "";

    result.segments.push_back(seg);
    full_text += seg.text;
  }

  result.full_text = full_text;
  result.detected_language = config_.language;
  result.success = true;

  return result;
}
