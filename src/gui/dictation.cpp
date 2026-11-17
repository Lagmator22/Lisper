#include "gui/dictation.h"

#include "model_profiles.h"
#include "text_cleanup.h"

#include <filesystem>

namespace fs = std::filesystem;

namespace gui {

namespace {

constexpr size_t kMinSamples = 16000 * 3 / 10; // ignore taps shorter than 0.3 s

std::string engine_key(const LisperConfig &c) {
  return c.model_path + "|" + std::to_string(static_cast<int>(c.device)) + "|" +
         std::to_string(c.gpu_device) + "|" + std::to_string(c.threads) + "|" +
         (c.flash_attn ? "1" : "0");
}

std::string resolve_settings_model(const Settings &s) {
  return s.model_path.empty() ? resolve_model_path("", s.model_profile) : s.model_path;
}

} // namespace

const char *phase_label(Dictation::Phase phase) {
  switch (phase) {
  case Dictation::Phase::Recording:
    return "Listening";
  case Dictation::Phase::Transcribing:
    return "Transcribing";
  case Dictation::Phase::Done:
    return "Done";
  case Dictation::Phase::Error:
    return "Error";
  default:
    return "Ready";
  }
}

Dictation::Dictation() = default;

Dictation::~Dictation() {
  cancel_flag_ = true;
  capture_.cancel();
  join_workers();
}

void Dictation::join_workers() {
  if (decoder_.joinable()) {
    decoder_.join();
  }
  if (loader_.joinable()) {
    loader_.join();
  }
}

void Dictation::set_phase(Phase phase, const std::string &message) {
  // mutex_ must be held by the caller
  phase_ = phase;
  phase_since_ = std::chrono::steady_clock::now();
  message_ = message;
}

void Dictation::configure(const Settings &settings) {
  LisperConfig config;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    settings_ = settings;
    const std::string model = resolve_settings_model(settings);
    if (model.empty() || !fs::exists(model)) {
      model_status_ = "Model for profile '" + settings.model_profile + "' is not installed";
      return;
    }
    config = settings.engine_config(model);
    if (engine_ && engine_key(config) == engine_key_) {
      // Same weights: only refresh the vocabulary prompt.
      engine_->set_initial_prompt(config.initial_prompt);
      return;
    }
    if (loading_) {
      reconfigure_pending_ = true; // applied by update() once the load finishes
      return;
    }
  }
  load_engine_async(config);
}

void Dictation::load_engine_async(const LisperConfig &config) {
  if (loader_.joinable()) {
    loader_.join();
  }
  {
    std::lock_guard<std::mutex> lock(mutex_);
    loading_ = true;
    model_status_ = "Loading " + fs::path(config.model_path).filename().string() + "...";
  }
  loader_ = std::thread([this, config] {
    auto engine = std::make_shared<Lisper>(config);
    std::lock_guard<std::mutex> lock(mutex_);
    loading_ = false;
    if (engine->is_loaded()) {
      engine_ = engine;
      engine_key_ = engine_key(config);
      model_status_ = fs::path(config.model_path).filename().string();
    } else {
      model_status_ = "Failed to load " + fs::path(config.model_path).filename().string();
    }
  });
}

void Dictation::start() {
  std::unique_lock<std::mutex> lock(mutex_);
  if (phase_ == Phase::Recording || phase_ == Phase::Transcribing) {
    return;
  }
  if (!engine_ && !loading_) {
    set_phase(Phase::Error, model_status_);
    return;
  }
  const std::string device = settings_.input_device;
  const int max_seconds = settings_.max_dictation_seconds;
  lock.unlock();

  std::string error;
  if (!capture_.start(device, max_seconds, error)) {
    lock.lock();
    set_phase(Phase::Error, error);
    return;
  }
  lock.lock();
  set_phase(Phase::Recording);
}

void Dictation::stop() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (phase_ != Phase::Recording) {
      return;
    }
  }
  std::vector<float> pcm = capture_.stop();

  std::lock_guard<std::mutex> lock(mutex_);
  if (pcm.size() < kMinSamples) {
    set_phase(Phase::Idle);
    return;
  }
  set_phase(Phase::Transcribing);
  if (decoder_.joinable()) {
    decoder_.join();
  }
  cancel_flag_ = false;

  const Settings settings = settings_;
  decoder_ = std::thread([this, settings, pcm = std::move(pcm)] {
    // Wait for a model that is still loading when the user started talking.
    std::shared_ptr<Lisper> engine;
    for (;;) {
      {
        std::lock_guard<std::mutex> guard(mutex_);
        engine = engine_;
        if (engine || !loading_ || cancel_flag_) {
          break;
        }
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    if (!engine || cancel_flag_) {
      std::lock_guard<std::mutex> guard(mutex_);
      set_phase(cancel_flag_ ? Phase::Idle : Phase::Error,
                cancel_flag_ ? "" : "No speech model is loaded");
      return;
    }

    DecodeOptions options;
    options.cancel = &cancel_flag_;
    const TranscriptionResult r = engine->transcribe_pcm(pcm, options);

    std::lock_guard<std::mutex> guard(mutex_);
    if (cancel_flag_) {
      set_phase(Phase::Idle);
      return;
    }
    if (!r.success) {
      set_phase(Phase::Error, r.error);
      return;
    }
    Result out;
    out.raw_text = r.full_text;
    out.text = text_cleanup::clean(r.full_text, settings.cleanup_options());
    out.duration_ms = r.duration_ms;
    out.language = r.detected_language;
    if (out.text.empty()) {
      set_phase(Phase::Idle, "No speech detected");
      return;
    }
    result_ = std::move(out);
    set_phase(Phase::Done);
  });
}

void Dictation::cancel() {
  cancel_flag_ = true;
  capture_.cancel();
  std::lock_guard<std::mutex> lock(mutex_);
  if (phase_ == Phase::Recording) {
    set_phase(Phase::Idle);
  }
}

void Dictation::toggle() {
  Phase p;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    p = phase_;
  }
  if (p == Phase::Recording) {
    stop();
  } else if (p != Phase::Transcribing) {
    start();
  }
}

void Dictation::update() {
  bool hit_limit = false;
  bool reconfigure = false;
  Settings pending_settings;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (reconfigure_pending_ && !loading_) {
      reconfigure_pending_ = false;
      reconfigure = true;
      pending_settings = settings_;
    }
    const float elapsed =
        std::chrono::duration<float>(std::chrono::steady_clock::now() - phase_since_).count();
    if (phase_ == Phase::Recording) {
      hit_limit = elapsed >= static_cast<float>(settings_.max_dictation_seconds);
    } else if ((phase_ == Phase::Done && elapsed > 1.4f && !result_) ||
               (phase_ == Phase::Error && elapsed > 4.0f)) {
      set_phase(Phase::Idle, phase_ == Phase::Error ? message_ : std::string());
    }
  }
  if (reconfigure) {
    configure(pending_settings);
  }
  if (hit_limit) {
    stop();
  }
}

std::optional<Dictation::Result> Dictation::consume_result() {
  std::lock_guard<std::mutex> lock(mutex_);
  auto r = std::move(result_);
  result_.reset();
  if (r) {
    phase_since_ = std::chrono::steady_clock::now(); // restart the Done timer
  }
  return r;
}

void Dictation::report_inserted(bool ok, const std::string &message) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!ok) {
    set_phase(Phase::Error, message);
  } else {
    message_ = message;
  }
}

Dictation::Phase Dictation::phase() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return phase_;
}

float Dictation::phase_seconds() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return std::chrono::duration<float>(std::chrono::steady_clock::now() - phase_since_).count();
}

std::string Dictation::message() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return message_;
}

bool Dictation::model_ready() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return engine_ != nullptr;
}

bool Dictation::model_loading() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return loading_;
}

std::string Dictation::model_status() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return model_status_;
}

std::string Dictation::model_path() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return engine_ ? engine_->config().model_path : std::string();
}

std::shared_ptr<Lisper> Dictation::engine() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return engine_;
}

} // namespace gui
