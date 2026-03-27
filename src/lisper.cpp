#include "lisper.h"
#include "interrupt.h"
#include "whisper.h"

#include <cstring>
#include <fstream>
#include <iostream>

Lisper::Lisper(const LisperConfig &config) : config_(config) {
  // Suppress whisper.cpp and ggml logging to keep CLI output clean
  whisper_log_set(
      [](ggml_log_level level, const char *text, void *user_data) {
        (void)user_data;
        if (level == GGML_LOG_LEVEL_ERROR) {
          std::cerr << "whisper.cpp error: " << text;
        }
      },
      nullptr);

  whisper_context_params cparams = whisper_context_default_params();
  cparams.use_gpu = config_.device != LisperConfig::Device::CPU;
  cparams.gpu_device = config_.gpu_device;
  cparams.flash_attn = config_.flash_attn && cparams.use_gpu;
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

  // Get file size for validation
  file.seekg(0, std::ios::end);
  const std::streamoff file_size_off = file.tellg();
  if (file_size_off <= 0) {
    return false;
  }
  const size_t file_size = static_cast<size_t>(file_size_off);
  file.seekg(0, std::ios::beg);

  // Minimum WAV file size check
  if (file_size < 44) {
    std::cerr << "File too small to be a valid WAV file\n";
    return false;
  }

  // read the RIFF header
  char riff[4];
  file.read(riff, 4);
  if (!file.good() || std::strncmp(riff, "RIFF", 4) != 0) {
    return false;
  }

  uint32_t chunk_size;
  file.read(reinterpret_cast<char *>(&chunk_size), 4);

  // Validate chunk size
  if (chunk_size + 8 > file_size) {
    std::cerr << "Invalid chunk size in WAV header\n";
    return false;
  }

  char wave[4];
  file.read(wave, 4);
  if (!file.good() || std::strncmp(wave, "WAVE", 4) != 0) {
    return false;
  }

  // find the fmt and data chunks
  int16_t audio_format = 0;
  int16_t num_channels = 0;
  int32_t sr = 0;
  int16_t bits_per_sample = 0;
  bool fmt_found = false;

  const size_t max_iterations = 100; // Prevent infinite loops
  size_t iterations = 0;

  while (file.good() && iterations++ < max_iterations) {
    char sub_id[4];
    uint32_t sub_size;
    file.read(sub_id, 4);
    file.read(reinterpret_cast<char *>(&sub_size), 4);

    if (!file.good())
      break;

    // Validate sub-chunk size
    const std::streamoff current_pos_off = file.tellg();
    if (current_pos_off < 0) {
      std::cerr << "Failed to parse WAV chunk offsets\n";
      return false;
    }
    const size_t current_pos = static_cast<size_t>(current_pos_off);
    if (current_pos + sub_size > file_size) {
      std::cerr << "Invalid sub-chunk size\n";
      return false;
    }

    if (std::strncmp(sub_id, "fmt ", 4) == 0) {
      if (sub_size < 16) {
        std::cerr << "Invalid fmt chunk size\n";
        return false;
      }

      file.read(reinterpret_cast<char *>(&audio_format), 2);
      file.read(reinterpret_cast<char *>(&num_channels), 2);
      file.read(reinterpret_cast<char *>(&sr), 4);

      uint32_t byte_rate;
      file.read(reinterpret_cast<char *>(&byte_rate), 4);

      uint16_t block_align;
      file.read(reinterpret_cast<char *>(&block_align), 2);

      file.read(reinterpret_cast<char *>(&bits_per_sample), 2);

      // Validate format parameters
      if (num_channels <= 0 || num_channels > 32 || sr <= 0 || sr > 384000 ||
          bits_per_sample <= 0 || bits_per_sample > 64) {
        std::cerr << "Invalid audio format parameters\n";
        return false;
      }

      fmt_found = true;

      // skip any extra fmt bytes
      if (sub_size > 16) {
        file.seekg(sub_size - 16, std::ios::cur);
      }
      if (sub_size % 2 != 0) {
        file.seekg(1, std::ios::cur);
      }
    } else if (std::strncmp(sub_id, "data", 4) == 0) {
      if (!fmt_found) {
        std::cerr << "Data chunk found before fmt chunk\n";
        return false;
      }

      sample_rate = sr;

      if (audio_format != 1 || bits_per_sample != 16) {
        std::cerr << "Only 16-bit PCM WAV is supported.\n";
        return false;
      }

      size_t bytes_per_sample = bits_per_sample / 8;
      size_t num_samples = sub_size / bytes_per_sample;

      // Sanity check for sample count
      constexpr size_t kMaxSamples = static_cast<size_t>(16000) * 60 * 60 * 12;
      if (num_samples > kMaxSamples) { // 12 hours at 16kHz mono
        std::cerr << "Audio file too large\n";
        return false;
      }

      std::vector<int16_t> raw;
      try {
        raw.resize(num_samples);
      } catch (const std::bad_alloc &) {
        std::cerr << "Failed to allocate memory for audio data\n";
        return false;
      }

      file.read(reinterpret_cast<char *>(raw.data()),
                static_cast<std::streamsize>(sub_size));
      if (file.gcount() != static_cast<std::streamsize>(sub_size)) {
        std::cerr << "Failed to read audio data\n";
        return false;
      }

      // convert to mono float32 normalized to [-1, 1]
      try {
        if (num_channels == 1) {
          pcm.resize(num_samples);
          for (size_t i = 0; i < num_samples; i++) {
            pcm[i] = static_cast<float>(raw[i]) / 32768.0f;
          }
        } else {
          // downmix to mono by averaging channels
          size_t mono_samples = num_samples / num_channels;
          pcm.resize(mono_samples);
          for (size_t i = 0; i < mono_samples; i++) {
            float sum = 0.0f;
            for (int ch = 0; ch < num_channels; ch++) {
              size_t idx = i * num_channels + ch;
              if (idx < raw.size()) {
                sum += static_cast<float>(raw[idx]);
              }
            }
            pcm[i] = (sum / num_channels) / 32768.0f;
          }
        }
      } catch (const std::bad_alloc &) {
        std::cerr << "Failed to allocate memory for PCM data\n";
        return false;
      }

      return true;
    } else {
      // Skip unknown chunks
      file.seekg(sub_size + (sub_size % 2), std::ios::cur);
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
  params.abort_callback = [](void *) {
    return interrupt_state::is_interrupted();
  };
  params.abort_callback_user_data = nullptr;

  int ret =
      whisper_full(ctx_, params, pcm.data(), static_cast<int>(pcm.size()));
  if (ret != 0) {
    if (interrupt_state::is_interrupted()) {
      result.error = "Interrupted by user";
      return result;
    }
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
