#pragma once

#include "gui/state.h"

#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace gui {

struct JobOutcome {
  bool success = false;
  bool cancelled = false;
  std::string status;
  std::string resolved_model_path;
  std::string final_output_path;
  std::string preview_text;
  TranscriptionResult result;
};

class BackgroundTranscriber {
public:
  ~BackgroundTranscriber();

  void start(const JobRequest &request);
  void cancel();
  bool running() const;
  std::string status() const;
  std::optional<JobOutcome> consume_completed();
  void wait();

private:
  void set_status(const std::string &status);
  void finish(JobOutcome outcome);
  void join_finished_worker();
  void run_job(const JobRequest &request);

  mutable std::mutex mutex_;
  std::thread worker_;
  bool running_ = false;
  std::string status_ = "Idle";
  std::optional<JobOutcome> completed_;
};

} // namespace gui
