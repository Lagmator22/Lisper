#include "formatter.h"
#include "lisper.h"
#include "live.h"
#include "media.h"
#include "watcher.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct CliArgs {
  std::string model_path;
  std::string input_path;
  std::string output_path;
  std::string format_str = "text";
  std::string watch_dir;
  std::string batch_dir;
  std::string language = "en";
  int threads = 4;
  bool translate = false;
  bool help = false;
  bool live = false;
};

static void print_usage() {
  std::cout
      << "Lisper - Local Speech-to-Text Engine\n"
         "Powered by whisper.cpp + OpenVINO\n\n"
         "Usage:\n"
         "  lisper -m <model> <input>              Transcribe a file\n"
         "  lisper -m <model> -d <dir>             Batch transcribe a "
         "directory\n"
         "  lisper -m <model> -w <dir>             Watch directory for new "
         "files\n\n"
         "Options:\n"
         "  -m, --model <path>     Path to whisper ggml model file (required)\n"
         "  -f, --format <fmt>     Output format: text, srt, json, rag "
         "(default: text)\n"
         "  -o, --output <path>    Output file or directory\n"
         "  -d, --dir <path>       Batch process all media files in directory\n"
         "  -w, --watch <path>     Watch directory and auto-transcribe new "
         "files\n"
         "  -l, --language <lang>  Language code (default: en)\n"
         "  -t, --threads <n>      Number of threads (default: 4)\n"
         "  --live                 Start real-time microphone transcription\n"
         "  --translate            Translate to English\n"
         "  -h, --help             Show this help\n\n"
         "Supported formats:\n"
         "  Audio: .wav .mp3 .flac .ogg .m4a .aac .wma\n"
         "  Video: .mp4 .mkv .avi .webm .mov .flv .wmv .m4v\n\n"
         "Examples:\n"
         "  lisper -m models/ggml-base.en.bin recording.wav\n"
         "  lisper -m models/ggml-base.en.bin lecture.mp4 -f srt -o "
         "lecture.srt\n"
         "  lisper -m models/ggml-base.en.bin -d ./videos/ -o ./transcripts/\n"
         "  lisper -m models/ggml-base.en.bin -w ./media/ -o ./output/ -f "
         "rag\n";
}

static CliArgs parse_args(int argc, char **argv) {
  CliArgs args;

  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];

    if (arg == "-h" || arg == "--help") {
      args.help = true;
    } else if ((arg == "-m" || arg == "--model") && i + 1 < argc) {
      args.model_path = argv[++i];
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
      args.threads = std::stoi(argv[++i]);
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

static bool transcribe_file(Lisper &engine, const std::string &input,
                            formatter::Format fmt, const std::string &output) {
  std::string filename = fs::path(input).filename().string();
  std::cout << "Processing: " << filename << "\n";

  auto prepared = media::prepare_audio(input);
  if (!prepared.success) {
    std::cerr << "  Error: " << prepared.error << "\n";
    return false;
  }

  auto result = engine.transcribe(prepared.wav_path);

  // clean up temp wav if we created one
  if (prepared.is_temp) {
    fs::remove(prepared.wav_path);
  }

  if (!result.success) {
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
    formatter::write_output(result, fmt, output, filename);
  }

  return true;
}

int main(int argc, char **argv) {
  CliArgs args = parse_args(argc, argv);

  if (args.help || args.model_path.empty()) {
    print_usage();
    return args.help ? 0 : 1;
  }

  if (!fs::exists(args.model_path)) {
    std::cerr << "Model not found: " << args.model_path << "\n";
    return 1;
  }

  LisperConfig config;
  config.model_path = args.model_path;
  config.language = args.language;
  config.threads = args.threads;
  config.translate = args.translate;

  std::cout << "Loading model: "
            << fs::path(args.model_path).filename().string() << "\n";

  Lisper engine(config);
  if (!engine.is_loaded()) {
    std::cerr << "Failed to load model.\n";
    return 1;
  }

  formatter::Format fmt = formatter::parse_format(args.format_str);

#ifdef LISPER_ENABLE_LIVE
  if (args.live) {
    live::start_live_transcription(engine);
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

    fs::create_directories(args.output_path);

    watcher::WatchConfig wconf;
    wconf.watch_dir = args.watch_dir;
    wconf.output_dir = args.output_path;

    watcher::watch_directory(wconf, [&](const std::string &path) {
      transcribe_file(engine, path, fmt, args.output_path);
    });

    return 0;
  }

  // batch mode
  if (!args.batch_dir.empty()) {
    auto files = watcher::scan_directory(args.batch_dir);
    if (files.empty()) {
      std::cerr << "No media files found in: " << args.batch_dir << "\n";
      return 1;
    }

    if (!args.output_path.empty()) {
      fs::create_directories(args.output_path);
    }

    std::cout << "Found " << files.size() << " media file(s).\n\n";

    int success = 0;
    for (size_t i = 0; i < files.size(); i++) {
      std::cout << "[" << (i + 1) << "/" << files.size() << "] ";
      if (transcribe_file(engine, files[i], fmt, args.output_path)) {
        success++;
      }
      std::cout << "\n";
    }

    std::cout << "Done. " << success << "/" << files.size()
              << " files transcribed.\n";
    return 0;
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
  return ok ? 0 : 1;
}
