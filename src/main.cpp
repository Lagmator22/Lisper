#include "cli_style.h"
#include "formatter.h"
#include "interrupt.h"
#include "lisper.h"
#include "live.h"
#include "media.h"
#include "model_profiles.h"
#include "watcher.h"

#include <chrono>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// Signal handler for graceful shutdown
static void signal_handler(int signal) {
  if (signal == SIGINT || signal == SIGTERM) {
    interrupt_state::request_stop();
  }
}

struct CliArgs {
  enum class DeviceArg {
    Auto,
    CPU,
    GPU,
  };

  std::string model_path;
  std::string model_profile = "quality";
  std::string input_path;
  std::string output_path;
  std::string format_str = "text";
  std::string watch_dir;
  std::string batch_dir;
  std::string language = "en";
  int threads = 4;
  int gpu_device = 0;
  bool translate = false;
  bool help = false;
  bool live = false;
  bool no_color = false;
  bool no_animate = false;
  bool list_profiles = false;
  bool no_flash_attn = false;
  DeviceArg device = DeviceArg::Auto;
};

static void print_model_profiles() {
  std::cout << cli_style::divider() << "\n";
  std::cout << cli_style::gradient_text("MODEL PROFILES") << "\n";
  std::cout << cli_style::style("Choose a profile for the machine you have, or "
                                "override it with -m.",
                                cli_style::Tone::Muted)
            << "\n";
  std::cout << cli_style::divider() << "\n\n";
  for (const auto &profile : all_model_profiles()) {
    std::cout << "  " << cli_style::badge(profile.name, cli_style::Tone::Accent)
              << " " << cli_style::gradient_text(profile.filename, false)
              << "\n"
              << "    "
              << cli_style::style(profile.description, cli_style::Tone::Muted)
              << "\n\n";
  }
}

static std::string style_usage_heading(const std::string &text) {
  return cli_style::gradient_text(text);
}

static void print_usage() {
  std::cout << cli_style::divider() << "\n";
  std::cout << style_usage_heading("LISPER CLI") << "\n";
  std::cout << cli_style::style(
                   "Local speech-to-text for offline transcription on the "
                   "hardware you already have.",
                   cli_style::Tone::Muted)
            << "\n";
  std::cout << cli_style::badge("whisper.cpp", cli_style::Tone::Accent) << " "
            << cli_style::badge("cpu and gpu aware", cli_style::Tone::Success)
            << " "
            << cli_style::badge("private by default", cli_style::Tone::Warning)
            << "\n";
  std::cout << cli_style::divider() << "\n\n";

  std::cout << style_usage_heading("Usage") << "\n";
  std::cout << "  "
            << cli_style::gradient_text("lisper -m <model> <input>", false)
            << "              Transcribe one file\n";
  std::cout << "  "
            << cli_style::gradient_text(
                   "lisper --model-profile <profile> <input>", false)
            << "  Auto-select a model profile\n";
  std::cout << "  "
            << cli_style::gradient_text("lisper -m <model> -d <dir>", false)
            << "             Batch transcribe a directory\n";
  std::cout << "  "
            << cli_style::gradient_text("lisper -m <model> -w <dir>", false)
            << "             Watch a directory for new files\n";
  std::cout << "  " << cli_style::gradient_text("./build/lisper_gui", false)
            << "                    Launch the desktop app\n\n";

  std::cout << style_usage_heading("Options") << "\n";
  std::cout << "  " << cli_style::gradient_text("-m, --model <path>", false)
            << "     Path to a Whisper GGML model file\n";
  std::cout << "  " << cli_style::gradient_text("--model-profile <name>", false)
            << " Model profile: fast, balanced, quality, max\n";
  std::cout << "  " << cli_style::gradient_text("--list-model-profiles", false)
            << "  Show model profiles and exit\n";
  std::cout << "  " << cli_style::gradient_text("-f, --format <fmt>", false)
            << "     Output format: text, srt, json, rag\n";
  std::cout << "  " << cli_style::gradient_text("-o, --output <path>", false)
            << "    Output file or directory\n";
  std::cout << "  " << cli_style::gradient_text("-d, --dir <path>", false)
            << "       Batch process all media in a directory\n";
  std::cout << "  " << cli_style::gradient_text("-w, --watch <path>", false)
            << "     Watch a directory for new media\n";
  std::cout << "  " << cli_style::gradient_text("-l, --language <lang>", false)
            << "  Language code (default: en)\n";
  std::cout << "  " << cli_style::gradient_text("-t, --threads <n>", false)
            << "      Number of threads (default: 4)\n";
  std::cout << "  " << cli_style::gradient_text("--device <mode>", false)
            << "        auto, cpu, gpu (default: auto)\n";
  std::cout << "  " << cli_style::gradient_text("--gpu-device <id>", false)
            << "      GPU device index for supported backends\n";
  std::cout << "  " << cli_style::gradient_text("--no-flash-attn", false)
            << "        Disable flash attention\n";
  std::cout << "  " << cli_style::gradient_text("--no-color", false)
            << "             Disable colored output\n";
  std::cout << "  " << cli_style::gradient_text("--no-animate", false)
            << "           Disable spinner and splash animation\n";
  std::cout << "  " << cli_style::gradient_text("--live", false)
            << "                 Start real-time microphone transcription\n";
  std::cout << "  " << cli_style::gradient_text("--translate", false)
            << "            Translate output to English\n";
  std::cout << "  " << cli_style::gradient_text("-h, --help", false)
            << "             Show this help\n\n";

  std::cout << style_usage_heading("Formats") << "\n";
  std::cout << "  " << cli_style::badge("audio", cli_style::Tone::Accent)
            << " .wav .mp3 .flac .ogg .m4a .aac .wma .aiff .aif\n";
  std::cout << "  " << cli_style::badge("video", cli_style::Tone::Success)
            << " .mp4 .mkv .avi .webm .mov .flv .wmv .m4v\n\n";

  std::cout << style_usage_heading("Examples") << "\n";
  std::cout << "  "
            << cli_style::gradient_text(
                   "lisper --model-profile quality recording.wav", false)
            << "\n";
  std::cout << "  "
            << cli_style::gradient_text(
                   "lisper -m models/ggml-large-v3-turbo-q5_0.bin lecture.mp4 "
                   "-f srt -o lecture.srt",
                   false)
            << "\n";
  std::cout << "  "
            << cli_style::gradient_text(
                   "lisper -m models/ggml-large-v3-turbo-q5_0.bin -d ./videos/ "
                   "-o ./transcripts/",
                   false)
            << "\n";
  std::cout
      << "  "
      << cli_style::gradient_text(
             "lisper --model-profile quality -w ./media/ -o ./output/ -f rag",
             false)
      << "\n";
}

static CliArgs parse_args(int argc, char **argv) {
  CliArgs args;

  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];

    if (arg == "-h" || arg == "--help") {
      args.help = true;
    } else if ((arg == "-m" || arg == "--model") && i + 1 < argc) {
      args.model_path = argv[++i];
    } else if (arg == "--model-profile" && i + 1 < argc) {
      args.model_profile = argv[++i];
    } else if (arg == "--list-model-profiles") {
      args.list_profiles = true;
    } else if ((arg == "-f" || arg == "--format") && i + 1 < argc) {
      args.format_str = argv[++i];
    } else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
      args.output_path = argv[++i];
    } else if ((arg == "-d" || arg == "--dir") && i + 1 < argc) {
      args.batch_dir = argv[++i];
    } else if ((arg == "-w" || arg == "--watch") && i + 1 < argc) {
      args.watch_dir = argv[++i];
    } else if ((arg == "-l" || arg == "--language") && i + 1 < argc) {
      args.language = argv[++i];
    } else if ((arg == "-t" || arg == "--threads") && i + 1 < argc) {
      try {
        args.threads = std::stoi(argv[++i]);
        if (args.threads <= 0) {
          std::cerr << "Error: thread count must be positive\n";
          args.threads = 4; // Reset to default
        }
      } catch (const std::exception &e) {
        std::cerr << "Error: invalid thread count\n";
        args.threads = 4; // Reset to default
      }
    } else if (arg == "--device" && i + 1 < argc) {
      const std::string device = argv[++i];
      if (device == "cpu") {
        args.device = CliArgs::DeviceArg::CPU;
      } else if (device == "gpu") {
        args.device = CliArgs::DeviceArg::GPU;
      } else if (device == "auto") {
        args.device = CliArgs::DeviceArg::Auto;
      } else {
        std::cerr << "Warning: invalid --device value '" << device
                  << "', using auto\n";
        args.device = CliArgs::DeviceArg::Auto;
      }
    } else if (arg == "--gpu-device" && i + 1 < argc) {
      try {
        args.gpu_device = std::stoi(argv[++i]);
      } catch (const std::exception &) {
        std::cerr << "Error: invalid GPU device index\n";
        args.gpu_device = 0;
      }
    } else if (arg == "--no-flash-attn") {
      args.no_flash_attn = true;
    } else if (arg == "--no-color") {
      args.no_color = true;
    } else if (arg == "--no-animate") {
      args.no_animate = true;
    } else if (arg == "--live") {
      args.live = true;
    } else if (arg == "--translate") {
      args.translate = true;
    } else if (arg[0] != '-') {
      args.input_path = arg;
    }
  }

  return args;
}

// RAII wrapper for temporary file cleanup
class TempFileGuard {
  std::string path_;
  bool should_delete_;

public:
  TempFileGuard(const std::string &path, bool should_delete)
      : path_(path), should_delete_(should_delete) {}
  ~TempFileGuard() {
    if (should_delete_ && !path_.empty()) {
      try {
        fs::remove(path_);
      } catch (...) {
        // Silently ignore cleanup errors
      }
    }
  }
  // Disable copy to ensure single ownership
  TempFileGuard(const TempFileGuard &) = delete;
  TempFileGuard &operator=(const TempFileGuard &) = delete;
};

static bool transcribe_file(Lisper &engine, const std::string &input,
                            formatter::Format fmt, const std::string &output) {
  if (interrupt_state::is_interrupted()) {
    std::cerr << "  Interrupted before processing started.\n";
    return false;
  }

  std::string filename = fs::path(input).filename().string();
  std::cout << cli_style::style("Processing: ", cli_style::Tone::Accent)
            << filename << "\n";

  auto prepared = media::prepare_audio(input);
  if (!prepared.success) {
    std::cerr << "  Error: " << prepared.error << "\n";
    return false;
  }

  // Ensure temp file cleanup even on exceptions
  TempFileGuard temp_guard(prepared.wav_path, prepared.is_temp);

  auto result = engine.transcribe(prepared.wav_path);

  if (!result.success) {
    if (interrupt_state::is_interrupted() ||
        result.error == "Interrupted by user") {
      std::cerr << "  Interrupted.\n";
      return false;
    }
    std::cerr << "  Transcription failed: " << result.error << "\n";
    return false;
  }

  int duration_sec = result.duration_ms / 1000;
  std::cout << "  Duration: " << duration_sec / 60 << "m " << duration_sec % 60
            << "s"
            << " | Segments: " << result.segments.size() << "\n";

  if (output.empty()) {
    // print to stdout
    std::cout << "\n" << formatter::format_result(result, fmt, filename);
  } else {
    if (!formatter::write_output(result, fmt, output, filename)) {
      return false;
    }
  }

  return true;
}

static bool ensure_directory(const std::string &path,
                             const std::string &description) {
  std::error_code ec;
  const fs::path dir(path);

  if (fs::exists(dir, ec)) {
    if (ec) {
      std::cerr << "Failed to access " << description << ": " << ec.message()
                << "\n";
      return false;
    }
    if (!fs::is_directory(dir, ec) || ec) {
      std::cerr << description << " exists but is not a directory: " << path
                << "\n";
      return false;
    }
    return true;
  }

  fs::create_directories(dir, ec);
  if (ec) {
    std::cerr << "Failed to create " << description << ": " << ec.message()
              << "\n";
    return false;
  }
  return true;
}

int main(int argc, char **argv) {
  // Set up signal handlers
  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  CliArgs args = parse_args(argc, argv);
  cli_style::initialize(!args.no_color, !args.no_animate);
  interrupt_state::reset();
  cli_style::show_startup_banner();

  if (args.help) {
    print_usage();
    return 0;
  }

  if (args.list_profiles) {
    print_model_profiles();
    return 0;
  }

  if (find_model_profile(args.model_profile) == nullptr &&
      args.model_path.empty()) {
    std::cerr << "Unknown model profile: " << args.model_profile << "\n";
    print_model_profiles();
    return 1;
  }

  std::string model_path =
      resolve_model_path(args.model_path, args.model_profile);

  if (model_path.empty() && args.model_path.empty()) {
    const ModelProfile *profile = find_model_profile(args.model_profile);
    if (profile != nullptr) {
      std::cout << cli_style::style(
          "Model not found locally. Auto-downloading profile '" +
              args.model_profile + "'...\n",
          cli_style::Tone::Warning);

      std::string model_name = profile->filename;
      if (model_name.find("ggml-") == 0)
        model_name = model_name.substr(5);
      if (model_name.find(".bin") != std::string::npos)
        model_name = model_name.substr(0, model_name.find(".bin"));

      std::string script_path =
          "./third_party/whisper.cpp/models/download-ggml-model.sh";
      std::string out_dir = "models";
      if (!fs::exists(script_path) &&
          fs::exists(
              "../third_party/whisper.cpp/models/download-ggml-model.sh")) {
        script_path =
            "../third_party/whisper.cpp/models/download-ggml-model.sh";
        if (!fs::exists("models")) {
          out_dir = "../models";
        }
      }

      std::string cmd = script_path + " " + model_name + " " + out_dir;
      std::cout << cli_style::style("Executing: " + cmd, cli_style::Tone::Muted)
                << "\n";

      int res = std::system(cmd.c_str());
      if (res == 0) {
        model_path = resolve_model_path("", args.model_profile);
        std::cout << cli_style::style("Download complete.\n",
                                      cli_style::Tone::Success);
      } else {
        std::cerr << "Failed to download model.\n";
      }
    }
  }

  if (model_path.empty()) {
    if (args.model_path.empty()) {
      std::cerr << "Model for --model-profile '" << args.model_profile
                << "' was not found locally.\n";
    } else {
      std::cerr << "Model not found: " << args.model_path << "\n";
    }
    std::cerr << "Tip: download profile models with:\n";
    std::cerr << "  ./third_party/whisper.cpp/models/download-ggml-model.sh "
                 "large-v3-turbo-q5_0 models\n";
    return 1;
  }

  if (!fs::exists(model_path)) {
    std::cerr << "Model not found: " << model_path << "\n";
    return 1;
  }

  LisperConfig config;
  config.model_path = model_path;
  config.language = args.language;
  config.threads = args.threads;
  config.translate = args.translate;
  config.gpu_device = args.gpu_device;
  config.flash_attn = !args.no_flash_attn;

  switch (args.device) {
  case CliArgs::DeviceArg::CPU:
    config.device = LisperConfig::Device::CPU;
    break;
  case CliArgs::DeviceArg::GPU:
    config.device = LisperConfig::Device::GPU;
    break;
  default:
    config.device = LisperConfig::Device::Auto;
    break;
  }

  const std::string model_name = fs::path(model_path).filename().string();
  cli_style::Spinner load_spinner("Loading model " + model_name);
  Lisper engine(config);

  if (!engine.is_loaded()) {
    load_spinner.fail("Failed to load model " + model_name);
    std::cerr << "Failed to load model.\n";
    return 1;
  }
  load_spinner.success("Loaded model " + model_name);

  const std::string active_device =
      (config.device == LisperConfig::Device::CPU)
          ? "CPU"
          : (config.device == LisperConfig::Device::GPU ? "GPU" : "AUTO");
  std::cout << cli_style::badge(active_device, cli_style::Tone::Accent) << " "
            << cli_style::badge("threads " + std::to_string(config.threads),
                                cli_style::Tone::Muted)
            << " "
            << cli_style::badge("lang " + config.language,
                                cli_style::Tone::Success)
            << "\n";

  formatter::Format fmt = formatter::parse_format(args.format_str);

#ifdef LISPER_ENABLE_LIVE
  if (args.live) {
    live::start_live_transcription(engine);
    if (interrupt_state::is_interrupted()) {
      std::cout << "\n"
                << cli_style::style("Interrupted. Exiting live mode.",
                                    cli_style::Tone::Warning)
                << "\n";
      return 130;
    }
    return 0;
  }
#else
  if (args.live) {
    std::cerr << "Live transcription was not enabled at compile time (SDL2 "
                 "missing).\n";
    return 1;
  }
#endif

  // watch mode
  if (!args.watch_dir.empty()) {
    if (args.output_path.empty()) {
      std::cerr << "Watch mode requires an output directory (-o).\n";
      return 1;
    }

    if (!ensure_directory(args.output_path, "watch output directory")) {
      return 1;
    }

    watcher::WatchConfig wconf;
    wconf.watch_dir = args.watch_dir;
    wconf.output_dir = args.output_path;

    watcher::watch_directory(wconf, [&](const std::string &path) {
      transcribe_file(engine, path, fmt, args.output_path);
    });

    if (interrupt_state::is_interrupted()) {
      std::cout << cli_style::style("Interrupted. Watch mode stopped.",
                                    cli_style::Tone::Warning)
                << "\n";
      return 130;
    }
    return 0;
  }

  // batch mode
  if (!args.batch_dir.empty()) {
    auto files = watcher::scan_directory(args.batch_dir);
    if (files.empty()) {
      std::cerr << "No media files found in: " << args.batch_dir << "\n";
      return 1;
    }

    if (!args.output_path.empty() &&
        !ensure_directory(args.output_path, "batch output directory")) {
      return 1;
    }

    std::cout << cli_style::badge(std::to_string(files.size()) +
                                      " media file(s)",
                                  cli_style::Tone::Accent)
              << "\n";
    std::cout << cli_style::style("Starting batch transcription...",
                                  cli_style::Tone::Muted)
              << "\n";
    std::cout << cli_style::divider(50) << "\n\n";

    int success = 0;
    int failed = 0;
    auto start_time = std::chrono::steady_clock::now();

    bool interrupted = false;
    for (size_t i = 0; i < files.size() && !interrupt_state::is_interrupted();
         i++) {
      std::cout << "[" << (i + 1) << "/" << files.size() << "] ";

      auto file_start = std::chrono::steady_clock::now();
      bool ok = transcribe_file(engine, files[i], fmt, args.output_path);
      auto file_end = std::chrono::steady_clock::now();

      if (ok) {
        success++;
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(
                            file_end - file_start)
                            .count();
        std::cout << "  "
                  << cli_style::style("Completed", cli_style::Tone::Success)
                  << " in " << duration << "s";
      } else {
        if (interrupt_state::is_interrupted()) {
          interrupted = true;
          std::cout << "  "
                    << cli_style::style("Interrupted",
                                        cli_style::Tone::Warning);
          std::cout << "\n\n";
          break;
        }
        failed++;
        std::cout << "  " << cli_style::style("Failed", cli_style::Tone::Error);
      }

      // Estimate remaining time
      if (i < files.size() - 1) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                           file_end - start_time)
                           .count();
        auto avg_time = elapsed / (i + 1);
        auto remaining = avg_time * (files.size() - i - 1);
        std::cout << " | Est. remaining: " << remaining / 60 << "m "
                  << remaining % 60 << "s";
      }
      std::cout << "\n\n";
    }

    auto total_time = std::chrono::duration_cast<std::chrono::seconds>(
                          std::chrono::steady_clock::now() - start_time)
                          .count();

    std::cout << cli_style::divider(50) << "\n";
    std::cout << cli_style::gradient_text("BATCH COMPLETE") << "\n";
    std::cout << "  • Successful: " << success << "/" << files.size() << "\n";
    if (failed > 0) {
      std::cout << "  • Failed: " << failed << "\n";
    }
    std::cout << "  • Total time: " << total_time / 60 << "m "
              << total_time % 60 << "s\n";

    if (interrupted || interrupt_state::is_interrupted()) {
      return 130;
    }
    return failed > 0 ? 1 : 0;
  }

  // single file mode
  if (args.input_path.empty() && !args.live) {
    std::cerr << "No input file specified. Use -h for help.\n";
    return 1;
  }

  if (!fs::exists(args.input_path)) {
    std::cerr << "File not found: " << args.input_path << "\n";
    return 1;
  }

  bool ok = transcribe_file(engine, args.input_path, fmt, args.output_path);
  if (interrupt_state::is_interrupted()) {
    return 130;
  }
  return ok ? 0 : 1;
}
