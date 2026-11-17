#pragma once

#include "audio_capture.h"
#include "lisper.h"
#include "settings.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace gui {

// Wispr Flow style dictation: hotkey → record → transcribe → clean up →
// insert into the focused app. All public methods are called from the UI
// thread; recording, model loading and decoding happen on worker threads.
class Dictation {
public:
  enum class Phase { Idle, Recording, Transcribing, Done, Error };

  struct Result {
    std::string text;     // cleaned, ready to insert
    std::string raw_text; // whisper output
    int duration_ms = 0;
    std::string language;
  };

  Dictation();
  ~Dictation();

  Dictation(const Dictation &) = delete;
  Dictation &operator=(const Dictation &) = delete;

  // Apply settings; reloads the model in the background if it changed.
  void configure(const Settings &settings);

  void start();
  void stop();   // stop recording and transcribe
  void cancel(); // discard the recording / abort decoding
  void toggle();

  // Drive timers (max length, Done/Error fade-out). Call once per frame.
  void update();

  std::optional<Result> consume_result();

  Phase phase() const;
  float phase_seconds() const; // time spent in the current phase
  std::string message() const; // last error or status text

  bool model_ready() const;
  bool model_loading() const;
  std::string model_status() const;
  std::string model_path() const;

  const AudioCapture &capture() const { return capture_; }

  // Mark the outcome of inserting a result so the overlay can reflect it.
  void report_inserted(bool ok, const std::string &message);

  // Expose the engine for other features (file transcription) to reuse.
  std::shared_ptr<Lisper> engine() const;

private:
  void set_phase(Phase phase, const std::string &message = {});
  void load_engine_async(const LisperConfig &config);
  void join_workers();

  mutable std::mutex mutex_;
  Settings settings_;
  AudioCapture capture_;

  std::shared_ptr<Lisper> engine_;
  std::string engine_key_;
  std::string model_status_ = "No model loaded";
  bool loading_ = false;
  bool reconfigure_pending_ = false;
  std::thread loader_;

  std::thread decoder_;
  std::atomic<bool> cancel_flag_{false};
  std::optional<Result> result_;

  Phase phase_ = Phase::Idle;
  std::chrono::steady_clock::time_point phase_since_ = std::chrono::steady_clock::now();
  std::string message_;
};

const char *phase_label(Dictation::Phase phase);

} // namespace gui
