#include "gui/layout.h"

#include "gui/file_dialog.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace gui {

namespace {

float button_width_for_columns(int columns) {
  const float available = ImGui::GetContentRegionAvail().x;
  const float spacing = ImGui::GetStyle().ItemSpacing.x;
  if (columns <= 1) {
    return available;
  }
  return std::max(80.0f, (available - spacing * static_cast<float>(columns - 1)) /
                             static_cast<float>(columns));
}

void draw_flow_pill(bool &first, const std::string &text, const ImVec4 &text_color,
                    const ImVec4 &bg_color) {
  const float pill_width = ImGui::CalcTextSize(text.c_str()).x + 20.0f;
  if (!first) {
    float left_edge = ImGui::GetCursorScreenPos().x;
    float right_edge = left_edge + ImGui::GetContentRegionAvail().x;
    float next_item_x2 = ImGui::GetItemRectMax().x + 8.0f + pill_width;
    if (next_item_x2 < right_edge) {
      ImGui::SameLine(0.0f, 8.0f);
    }
  }
  first = false;
  draw_pill(text, text_color, bg_color);
}

void render_header_status_card(const AppState &state, BackgroundTranscriber &job,
                               const ImVec2 &size) {
  const ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
  if (!begin_surface("header_status_panel", size, true, flags)) {
    end_surface();
    return;
  }

  const bool running = job.running();
  const std::string status_text = running ? "Transcribing" : "Ready";
  const ImVec4 status_color = running ? color_v4(210, 153, 34) : color_v4(63, 185, 80);

  ImGui::PushStyleColor(ImGuiCol_Text, color_v4(145, 152, 161));
  ImGui::TextUnformatted("Current run");
  ImGui::PopStyleColor();

  ImGui::PushStyleColor(ImGuiCol_Text, status_color);
  ImGui::TextUnformatted(status_text.c_str());
  ImGui::PopStyleColor();

  end_surface();
}

void render_header(const Fonts &fonts, const AppState &state, BackgroundTranscriber &job) {
  const bool compact = ImGui::GetContentRegionAvail().x < 1040.0f;
  const float header_height = compact ? 220.0f : 190.0f;
  const ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

  if (begin_surface("app_header", ImVec2(0.0f, header_height), true, flags)) {
    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    const ImVec2 min = ImGui::GetWindowPos();
    const ImVec2 max(min.x + ImGui::GetWindowSize().x, min.y + ImGui::GetWindowSize().y);
    draw_list->AddRectFilledMultiColor(min, ImVec2(max.x, min.y + 2.0f),
                                       color_u32(31, 111, 235, 255), color_u32(68, 147, 248, 255),
                                       color_u32(31, 111, 235, 255), color_u32(9, 105, 218, 255));

    if (!compact &&
        ImGui::BeginTable("header_grid", 2,
                          ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoPadOuterX)) {
      ImGui::TableSetupColumn("hero", ImGuiTableColumnFlags_WidthStretch, 0.68f);
      ImGui::TableSetupColumn("status", ImGuiTableColumnFlags_WidthFixed, 320.0f);

      ImGui::TableNextColumn();
      if (fonts.hero != nullptr) {
        ImGui::PushFont(fonts.hero);
      }
      ImGui::TextUnformatted("Lisper");
      if (fonts.hero != nullptr) {
        ImGui::PopFont();
      }

      ImGui::PushStyleColor(ImGuiCol_Text, color_v4(145, 152, 161));
      ImGui::TextUnformatted("Local transcription studio");
      ImGui::PushTextWrapPos(0.0f);
      ImGui::TextUnformatted("Choose a Whisper model, run transcription locally, preview the "
                             "result, and export cleanly from one workspace.");
      ImGui::PopTextWrapPos();
      ImGui::PopStyleColor();

      ImGui::Spacing();
      bool first_pill = true;
      draw_flow_pill(first_pill, job.running() ? "Transcribing" : "Ready",
                     job.running() ? color_v4(210, 153, 34) : color_v4(63, 185, 80),
                     job.running() ? color_v4(54, 40, 4, 180) : color_v4(13, 45, 29, 180));
      draw_flow_pill(first_pill, current_profile_name(state.profile_index), color_v4(121, 192, 255),
                     color_v4(16, 35, 62, 180));
      draw_flow_pill(first_pill, describe_device(device_from_index(state.device_index)),
                     color_v4(230, 237, 243), color_v4(22, 27, 34, 220));
      draw_flow_pill(first_pill, format_label(format_from_index(state.format_index)),
                     color_v4(145, 152, 161), color_v4(22, 27, 34, 220));

      ImGui::TableNextColumn();
      render_header_status_card(state, job, ImVec2(0.0f, 80.0f));
      ImGui::EndTable();
    } else {
      if (fonts.hero != nullptr) {
        ImGui::PushFont(fonts.hero);
      }
      ImGui::TextUnformatted("Lisper");
      if (fonts.hero != nullptr) {
        ImGui::PopFont();
      }

      ImGui::PushStyleColor(ImGuiCol_Text, color_v4(145, 152, 161));
      ImGui::TextUnformatted("Local transcription studio");
      ImGui::PushTextWrapPos(0.0f);
      ImGui::TextUnformatted("Choose a Whisper model, transcribe locally, and "
                             "export from a single high-contrast workspace.");
      ImGui::PopTextWrapPos();
      ImGui::PopStyleColor();

      ImGui::Spacing();
      bool first_pill = true;
      draw_flow_pill(first_pill, job.running() ? "Transcribing" : "Ready",
                     job.running() ? color_v4(210, 153, 34) : color_v4(63, 185, 80),
                     job.running() ? color_v4(54, 40, 4, 180) : color_v4(13, 45, 29, 180));
      draw_flow_pill(first_pill, current_profile_name(state.profile_index), color_v4(121, 192, 255),
                     color_v4(16, 35, 62, 180));
      draw_flow_pill(first_pill, describe_device(device_from_index(state.device_index)),
                     color_v4(230, 237, 243), color_v4(22, 27, 34, 220));
      draw_flow_pill(first_pill, format_label(format_from_index(state.format_index)),
                     color_v4(145, 152, 161), color_v4(22, 27, 34, 220));

      ImGui::Spacing();
      render_header_status_card(state, job, ImVec2(0.0f, 72.0f));
    }
  }
  end_surface();
}

void render_input_section(AppState &state) {
  const std::string input = trim_copy(state.input_path.data());
  const bool exists = path_exists(input);

  label_text("Input");
  ImGui::SetNextItemWidth(-1.0f);
  ImGui::InputTextWithHint("##input_path", "Drop an audio/video file here or paste its path",
                           state.input_path.data(), state.input_path.size());

  const float button_w = button_width_for_columns(2);

  if (choice_button("Browse file", false, ImVec2(button_w, 0.0f))) {
    if (const auto selected = pick_input_media_file(input)) {
      copy_string(state.input_path, *selected);
      append_log(state, "Picked input file from native dialog.");
    }
  }
  ImGui::SameLine();
  if (choice_button("Clear", false, ImVec2(button_w, 0.0f))) {
    copy_string(state.input_path, "");
  }

  const bool sample_wav = path_exists("jfk.wav");
  const bool sample_aiff = path_exists("test.aiff");

  if (sample_wav) {
    if (choice_button("Use sample WAV", false, ImVec2(button_w, 0.0f))) {
      copy_string(state.input_path, fs::absolute("jfk.wav").string());
      append_log(state, "Loaded bundled WAV sample.");
    }
  }

  if (sample_aiff) {
    if (sample_wav) {
      ImGui::SameLine();
    }
    if (choice_button("Use sample AIFF", false, ImVec2(button_w, 0.0f))) {
      copy_string(state.input_path, fs::absolute("test.aiff").string());
      append_log(state, "Loaded bundled AIFF sample.");
    }
  }

  const ImVec4 status_color = exists ? color_v4(110, 224, 183) : color_v4(255, 182, 108);
  ImGui::PushStyleColor(ImGuiCol_Text, status_color);
  if (input.empty()) {
    ImGui::TextUnformatted("Waiting for input.");
  } else if (exists) {
    const std::string selected = "Selected: " + filename_for_display(input);
    ImGui::TextWrapped("%s", selected.c_str());
  } else {
    ImGui::TextUnformatted("The current input path does not exist.");
  }
  ImGui::PopStyleColor();
}

void render_output_section(AppState &state) {
  label_text("Output");
  ImGui::SetNextItemWidth(-1.0f);
  ImGui::InputTextWithHint("##output_path",
                           "Leave blank for preview only, or enter a directory / exact file path",
                           state.output_path.data(), state.output_path.size());

  const float button_w = button_width_for_columns(2);

  if (choice_button("Preview only", trim_copy(state.output_path.data()).empty(),
                    ImVec2(button_w, 0.0f))) {
    copy_string(state.output_path, "");
  }
  ImGui::SameLine();
  if (choice_button("Pick folder", false, ImVec2(button_w, 0.0f))) {
    if (const auto folder = pick_output_folder(trim_copy(state.output_path.data()))) {
      copy_string(state.output_path, *folder);
      append_log(state, "Selected output folder from native dialog.");
    }
  }
  if (choice_button("Save as file", false, ImVec2(-1.0f, 0.0f))) {
    const std::string preview = computed_output_preview(state);
    if (const auto file =
            pick_output_file(preview.empty() ? trim_copy(state.output_path.data()) : preview)) {
      copy_string(state.output_path, *file);
      append_log(state, "Selected explicit output file from native dialog.");
    }
  }

  ImGui::Spacing();
  label_text("Format");
  static const char *kFormats[] = {"Text", "SRT", "JSON", "RAG"};
  const float format_w = button_width_for_columns(2);
  for (int i = 0; i < 4; ++i) {
    if (i % 2 == 1) {
      ImGui::SameLine();
    }
    if (choice_button(kFormats[i], state.format_index == i, ImVec2(format_w, 0.0f))) {
      state.format_index = i;
    }
  }

  const std::string preview = computed_output_preview(state);
  ImGui::PushStyleColor(ImGuiCol_Text, color_v4(149, 161, 180));
  if (trim_copy(state.output_path.data()).empty()) {
    ImGui::TextWrapped("Mode: preview only. Nothing will be written to disk.");
  } else if (!preview.empty()) {
    ImGui::TextWrapped("Next write target: %s", preview.c_str());
  } else {
    ImGui::TextWrapped("If the output path is a directory, Lisper will "
                       "generate a transcript filename automatically.");
  }
  ImGui::PopStyleColor();
}

void render_model_section(AppState &state) {
  const auto &all = profiles();
  std::vector<const char *> names;
  names.reserve(all.size());
  for (const auto &profile : all) {
    names.push_back(profile.name.c_str());
  }

  label_text("Model profile");
  ImGui::SetNextItemWidth(-1.0f);
  ImGui::Combo("##profile", &state.profile_index, names.data(), static_cast<int>(names.size()));

  ImGui::PushStyleColor(ImGuiCol_Text, color_v4(149, 161, 180));
  ImGui::TextWrapped("%s", profile_description(state.profile_index).c_str());
  ImGui::TextWrapped("Default filename: %s", current_profile_filename(state.profile_index).c_str());
  ImGui::PopStyleColor();

  ImGui::Spacing();
  label_text("Device");
  static const char *kDevices[] = {"Auto", "CPU", "GPU"};
  const float device_w = button_width_for_columns(3);
  for (int i = 0; i < 3; ++i) {
    if (i > 0) {
      ImGui::SameLine();
    }
    if (choice_button(kDevices[i], state.device_index == i, ImVec2(device_w, 0.0f))) {
      state.device_index = i;
    }
  }

  ImGui::Spacing();
  label_text("Threads");
  ImGui::SetNextItemWidth(std::min(ImGui::GetContentRegionAvail().x, 140.0f));
  ImGui::InputInt("##threads", &state.threads);
  if (state.threads < 1)
    state.threads = 1;

  const std::string language = trim_copy(state.language.data());
  const bool show_advanced_default =
      !trim_copy(state.manual_model_path.data()).empty() || state.translate || !state.flash_attn ||
      state.gpu_device != 0 || (!language.empty() && language != "en");
  if (ImGui::CollapsingHeader("Advanced model options",
                              show_advanced_default ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
    label_text("Manual model override");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##manual_model", "Optional: path to a GGML model file",
                             state.manual_model_path.data(), state.manual_model_path.size());

    label_text("GPU device index");
    ImGui::InputInt("##gpu_device", &state.gpu_device);
    if (state.gpu_device < 0) {
      state.gpu_device = 0;
    }

    label_text("Language");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##language", "en", state.language.data(), state.language.size());

    ImGui::Checkbox("Translate to English", &state.translate);
    ImGui::Checkbox("Enable flash attention", &state.flash_attn);
  }

  refresh_resolved_model(state);
  const bool model_exists =
      !state.resolved_model_path.empty() && path_exists(state.resolved_model_path);
  ImGui::PushStyleColor(ImGuiCol_Text,
                        model_exists ? color_v4(110, 224, 183) : color_v4(255, 182, 108));
  ImGui::TextWrapped("Resolved model: %s", state.resolved_model_path.empty()
                                               ? "not found yet"
                                               : state.resolved_model_path.c_str());
  ImGui::PopStyleColor();
}

void render_run_section(AppState &state, const Fonts &fonts, BackgroundTranscriber &job) {
  const auto issues = validation_issues(state);
  const bool can_start = !job.running() && issues.empty();

  section_title(fonts, "Run", "Start a job when setup is valid, or cancel the active run.");
  ImGui::Spacing();

  if (!can_start) {
    ImGui::BeginDisabled();
  }
  if (ImGui::Button("Start transcription", ImVec2(-1.0f, 46.0f))) {
    const auto request = build_request(state);
    append_log(state, "Starting transcription for " + request.input_path);
    state.status_line = "Launching transcription job...";
    job.start(request);
  }
  if (!can_start) {
    ImGui::EndDisabled();
  }

  if (job.running()) {
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Button, color_v4(135, 49, 60));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color_v4(160, 61, 72));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, color_v4(118, 42, 53));
    if (ImGui::Button("Cancel Current Run", ImVec2(-1.0f, 38.0f))) {
      append_log(state, "Cancellation requested.");
      job.cancel();
    }
    ImGui::PopStyleColor(3);
  }

  ImGui::Spacing();
  const char frames[] = {'|', '/', '-', '\\'};
  const char spinner = frames[static_cast<int>(ImGui::GetTime() * 10.0) % 4];
  ImGui::PushStyleColor(ImGuiCol_Text,
                        job.running() ? color_v4(210, 153, 34) : color_v4(145, 152, 161));
  ImGui::Text("%c %s", job.running() ? spinner : '-', job.status().c_str());
  ImGui::PopStyleColor();

  ImGui::PushStyleColor(ImGuiCol_Text, color_v4(145, 152, 161));
  ImGui::PushTextWrapPos(0.0f);
  ImGui::TextUnformatted(state.status_line.c_str());
  ImGui::PopTextWrapPos();
  ImGui::PopStyleColor();

  if (issues.empty()) {
    ImGui::Spacing();
    draw_pill("Ready to run", color_v4(63, 185, 80), color_v4(13, 45, 29, 180));
  } else {
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, color_v4(210, 153, 34));
    ImGui::TextUnformatted("Before you start:");
    for (const auto &issue : issues) {
      ImGui::BulletText("%s", issue.c_str());
    }
    ImGui::PopStyleColor();
  }
}

void render_sidebar(AppState &state, const Fonts &fonts, BackgroundTranscriber &job) {
  section_title(fonts, "Setup",
                "Pick the input, output mode, model, and device settings for "
                "the next run.");

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();
  render_input_section(state);

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();
  render_output_section(state);

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();
  render_model_section(state);

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();
  render_run_section(state, fonts, job);
}

void render_transcript_tab(const AppState &state) {
  if (!state.preview_text.empty()) {
    if (choice_button("Copy transcript", false)) {
      ImGui::SetClipboardText(state.preview_text.c_str());
    }

    if (!state.final_output_path.empty()) {
      ImGui::SameLine();
      ImGui::PushStyleColor(ImGuiCol_Text, color_v4(149, 161, 180));
      ImGui::TextWrapped("Saved to %s", state.final_output_path.c_str());
      ImGui::PopStyleColor();
    }

    ImGui::Spacing();
    if (begin_surface("transcript_surface", ImVec2(0.0f, 0.0f), true)) {
      ImGui::PushTextWrapPos(0.0f);
      ImGui::TextUnformatted(state.preview_text.c_str());
      ImGui::PopTextWrapPos();
    }
    end_surface();
    return;
  }

  if (begin_surface("transcript_empty", ImVec2(0.0f, 0.0f), true)) {
    section_title(Fonts{}, "No transcript yet", "Run a file to populate this workspace.");
    ImGui::PushStyleColor(ImGuiCol_Text, color_v4(145, 152, 161));
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted("Choose an input in the setup panel, confirm the model and output "
                           "settings, then start transcription. The preview will appear here when "
                           "the run finishes.");
    ImGui::BulletText("Blank output path means preview only.");
    ImGui::BulletText("Use the profile picker unless you want to override the "
                      "model path manually.");
    ImGui::BulletText("Native file and folder pickers are available in the setup panel.");
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
  }
  end_surface();
}

void render_activity_tab(AppState &state) {
  if (choice_button("Clear log", false)) {
    state.logs.clear();
    append_log(state, "Activity log cleared.");
  }
  ImGui::SameLine();
  ImGui::Checkbox("Auto-scroll", &state.auto_scroll_log);

  ImGui::Spacing();
  if (begin_surface("activity_surface", ImVec2(0.0f, 0.0f), true,
                    ImGuiWindowFlags_HorizontalScrollbar)) {
    for (const auto &entry : state.logs) {
      ImGui::TextUnformatted(entry.c_str());
    }
    if (state.auto_scroll_log && !state.logs.empty()) {
      ImGui::SetScrollHereY(1.0f);
    }
  }
  end_surface();
}

void render_details_tab(const AppState &state) {
  if (begin_surface("details_surface", ImVec2(0.0f, 0.0f), true)) {
    label_text("Input");
    const std::string input = trim_copy(state.input_path.data());
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(input.empty() ? "No input selected" : input.c_str());
    ImGui::PopTextWrapPos();

    ImGui::Spacing();
    label_text("Resolved model");
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(state.resolved_model_path.empty() ? "No model resolved"
                                                             : state.resolved_model_path.c_str());
    ImGui::PopTextWrapPos();

    ImGui::Spacing();
    label_text("Output target");
    const std::string output_preview = computed_output_preview(state);
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(output_preview.empty() ? "Preview only" : output_preview.c_str());
    ImGui::PopTextWrapPos();

    ImGui::Spacing();
    label_text("Current config");
    ImGui::BulletText("Profile: %s", current_profile_name(state.profile_index).c_str());
    ImGui::BulletText("Device: %s", describe_device(device_from_index(state.device_index)).c_str());
    ImGui::BulletText("Format: %s", format_label(format_from_index(state.format_index)));
    ImGui::BulletText("Threads: %d", state.threads);
    const std::string language = trim_copy(state.language.data());
    ImGui::BulletText("Language: %s", language.empty() ? "en" : language.c_str());
    ImGui::BulletText("Translate: %s", state.translate ? "yes" : "no");
    ImGui::BulletText("Flash attention: %s", state.flash_attn ? "enabled" : "disabled");

    if (state.duration_ms > 0 || state.segment_count > 0 || !state.detected_language.empty()) {
      ImGui::Spacing();
      label_text("Last result");
      ImGui::BulletText("Duration: %s", format_duration_ms(state.duration_ms).c_str());
      ImGui::BulletText("Segments: %d", state.segment_count);
      ImGui::BulletText("Detected language: %s", state.detected_language.empty()
                                                     ? "unknown"
                                                     : state.detected_language.c_str());
    }
  }
  end_surface();
}

void render_workspace(AppState &state, const Fonts &fonts, BackgroundTranscriber &job) {
  const std::string input = trim_copy(state.input_path.data());
  const std::string input_name = input.empty() ? "Workspace" : filename_for_display(input);

  section_title(fonts, input_name,
                "Preview the transcript, inspect the activity log, and check "
                "the resolved run details.");
  ImGui::Spacing();

  bool first_pill = true;
  draw_flow_pill(
      first_pill, job.running() ? job.status() : state.status_line,
      job.running() ? color_v4(210, 153, 34)
                    : (state.last_run_success ? color_v4(63, 185, 80) : color_v4(145, 152, 161)),
      job.running()
          ? color_v4(54, 40, 4, 180)
          : (state.last_run_success ? color_v4(13, 45, 29, 180) : color_v4(22, 27, 34, 220)));
  draw_flow_pill(first_pill, "Segments " + std::to_string(state.segment_count),
                 color_v4(121, 192, 255), color_v4(16, 35, 62, 180));
  draw_flow_pill(first_pill, "Duration " + format_duration_ms(state.duration_ms),
                 color_v4(145, 152, 161), color_v4(22, 27, 34, 220));

  ImGui::Spacing();
  if (ImGui::BeginTabBar("workspace_tabs", ImGuiTabBarFlags_FittingPolicyResizeDown)) {
    if (ImGui::BeginTabItem("Transcript")) {
      render_transcript_tab(state);
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Activity")) {
      render_activity_tab(state);
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Details")) {
      render_details_tab(state);
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
}

} // namespace

void render_main_window(AppState &state, const Fonts &fonts, BackgroundTranscriber &job) {
  refresh_resolved_model(state);

  const ImGuiViewport *viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->Pos);
  ImGui::SetNextWindowSize(viewport->Size);

  ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                           ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                           ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
                           ImGuiWindowFlags_NoBackground;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24.0f, 24.0f));
  ImGui::Begin("Lisper Studio Root", nullptr, flags);
  ImGui::PopStyleVar();

  const ImVec2 pos = ImGui::GetWindowPos();
  const ImVec2 size = ImGui::GetWindowSize();
  draw_background(ImGui::GetWindowDrawList(), pos, size, static_cast<float>(ImGui::GetTime()));

  render_header(fonts, state, job);
  ImGui::Spacing();

  const ImVec2 available = ImGui::GetContentRegionAvail();
  const bool stacked = available.x < 1120.0f;

  if (stacked) {
    const float sidebar_height = std::clamp(available.y * 0.48f, 320.0f, 520.0f);

    if (begin_surface("sidebar_stack", ImVec2(0.0f, sidebar_height))) {
      render_sidebar(state, fonts, job);
    }
    end_surface();

    ImGui::Spacing();
    if (begin_surface("workspace_stack", ImVec2(0.0f, 0.0f), true,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
      render_workspace(state, fonts, job);
    }
    end_surface();
  } else {
    const float sidebar_width = std::clamp(available.x * 0.34f, 340.0f, 430.0f);

    if (begin_surface("sidebar_side", ImVec2(sidebar_width, 0.0f))) {
      render_sidebar(state, fonts, job);
    }
    end_surface();

    ImGui::SameLine(0.0f, 16.0f);
    if (begin_surface("workspace_side", ImVec2(0.0f, 0.0f), true,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
      render_workspace(state, fonts, job);
    }
    end_surface();
  }

  ImGui::End();
}

} // namespace gui
