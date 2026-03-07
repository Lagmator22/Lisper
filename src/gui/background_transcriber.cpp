#include "gui/background_transcriber.h"

#include "formatter.h"
#include "interrupt.h"
#include "lisper.h"
#include "media.h"

#include <filesystem>
#include <fstream>
#include <utility>

namespace fs = std::filesystem;

namespace gui {

namespace {

struct TempFileGuard {
  explicit TempFileGuard(std::string path = {}, bool should_delete = false)
      : path_(std::move(path)), should_delete_(should_delete) {}

  ~TempFileGuard() {
    if (!should_delete_ || path_.empty()) {
      return;
    }

    std::error_code ec;
    fs::remove(path_, ec);
  }

  TempFileGuard(const TempFileGuard &) = delete;
  TempFileGuard &operator=(const TempFileGuard &) = delete;

private:
  std::string path_;
  bool should_delete_ = false;
};

} // namespace

BackgroundTranscriber::~BackgroundTranscriber() {
  cancel();
  wait();
}

void BackgroundTranscriber::start(const JobRequest &request) {
  join_finished_worker();
  interrupt_state::reset();

  {
    std::lock_guard<std::mutex> lock(mutex_);
    completed_.reset();
    running_ = true;
    status_ = "Loading model";
  }

  worker_ = std::thread([this, request]() { run_job(request); });
}

void BackgroundTranscriber::cancel() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!running_) {
      return;
    }
    status_ = "Cancelling";
  }
  interrupt_state::request_stop();
}

bool BackgroundTranscriber::running() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return running_;
}

std::string BackgroundTranscriber::status() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return status_;
}

std::optional<JobOutcome> BackgroundTranscriber::consume_completed() {
  join_finished_worker();

  std::lock_guard<std::mutex> lock(mutex_);
  if (!completed_.has_value()) {
    return std::nullopt;
  }

  auto outcome = std::move(completed_);
  completed_.reset();
  return outcome;
}

void BackgroundTranscriber::wait() {
  if (worker_.joinable()) {
    worker_.join();
  }
}

void BackgroundTranscriber::set_status(const std::string &status) {
  std::lock_guard<std::mutex> lock(mutex_);
  status_ = status;
}

void BackgroundTranscriber::finish(JobOutcome outcome) {
  std::lock_guard<std::mutex> lock(mutex_);
  running_ = false;
  status_ = outcome.cancelled ? "Cancelled"
                              : (outcome.success ? "Complete" : "Needs attention");
  completed_ = std::move(outcome);
}

void BackgroundTranscriber::join_finished_worker() {
  if (!worker_.joinable()) {
    return;
  }

  bool should_join = false;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    should_join = !running_;
  }

  if (should_join) {
    worker_.join();
  }
}

void BackgroundTranscriber::run_job(const JobRequest &request) {
  JobOutcome outcome;
  outcome.status = "Ready";

  try {
    outcome.resolved_model_path =
        resolve_model_path(request.explicit_model_path, request.model_profile);
    if (outcome.resolved_model_path.empty()) {
      outcome.status = request.explicit_model_path.empty()
                           ? "No local model matched the selected profile."
                           : "The manual model path could not be resolved.";
      finish(std::move(outcome));
      return;
    }

    if (!path_exists(outcome.resolved_model_path)) {
      outcome.status = "The model file does not exist: " + outcome.resolved_model_path;
      finish(std::move(outcome));
      return;
    }

    set_status("Loading model");
    LisperConfig config;
    config.model_path = outcome.resolved_model_path;
    config.language = request.language;
    config.threads = request.threads;
    config.translate = request.translate;
    config.device = request.device;
    config.gpu_device = request.gpu_device;
    config.flash_attn = request.flash_attn;

    Lisper engine(config);
    if (!engine.is_loaded()) {
      outcome.status = "Failed to load the selected model.";
      finish(std::move(outcome));
      return;
    }

    if (interrupt_state::is_interrupted()) {
      outcome.cancelled = true;
      outcome.status = "Cancelled before transcription started.";
      finish(std::move(outcome));
      return;
    }

    set_status("Preparing audio");
    auto prepared = media::prepare_audio(request.input_path);
    if (!prepared.success) {
      outcome.status = prepared.error;
      finish(std::move(outcome));
      return;
    }

    TempFileGuard guard(prepared.wav_path, prepared.is_temp);

    set_status("Running transcription");
    outcome.result = engine.transcribe(prepared.wav_path);
    if (!outcome.result.success) {
      if (interrupt_state::is_interrupted() ||
          outcome.result.error == "Interrupted by user") {
        outcome.cancelled = true;
        outcome.status = "Transcription cancelled.";
      } else {
        outcome.status = outcome.result.error.empty() ? "Transcription failed."
                                                      : outcome.result.error;
      }
      finish(std::move(outcome));
      return;
    }

    outcome.preview_text = formatter::format_result(
        outcome.result, request.format,
        fs::path(request.input_path).filename().string());

    if (!request.output_path.empty()) {
      set_status("Writing output");
      outcome.final_output_path = formatter::resolve_output_path(
          request.output_path, request.format,
          fs::path(request.input_path).filename().string());
      if (outcome.final_output_path.empty()) {
        outcome.status = "Could not resolve the output path.";
        finish(std::move(outcome));
        return;
      }

      const fs::path final_path(outcome.final_output_path);
      const fs::path parent = final_path.parent_path();
      if (!parent.empty()) {
        std::error_code ec;
        fs::create_directories(parent, ec);
        if (ec) {
          outcome.status = "Failed to create the output directory: " + ec.message();
          finish(std::move(outcome));
          return;
        }
      }

      std::ofstream out(outcome.final_output_path);
      if (!out.is_open()) {
        outcome.status = "Failed to write: " + outcome.final_output_path;
        finish(std::move(outcome));
        return;
      }
      out << outcome.preview_text;
    }

    outcome.success = true;
    outcome.status = outcome.final_output_path.empty()
                         ? "Transcript ready in preview."
                         : "Transcript saved to " + outcome.final_output_path;
    finish(std::move(outcome));
  } catch (const std::exception &ex) {
    outcome.status = std::string("Unexpected error: ") + ex.what();
    finish(std::move(outcome));
  } catch (...) {
    outcome.status = "Unexpected unknown error.";
    finish(std::move(outcome));
  }
}

} // namespace gui
