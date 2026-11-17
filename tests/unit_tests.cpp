// Lightweight unit tests for lisper_core. No external framework so the suite
// builds anywhere the library does. Run via `ctest` or ./lisper_tests.

#include "formatter.h"
#include "media.h"
#include "model_download.h"
#include "model_profiles.h"
#include "paths.h"
#include "process.h"
#include "watcher.h"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

#ifndef _WIN32
#include <sys/stat.h>
#endif

namespace fs = std::filesystem;

namespace {

int g_failures = 0;
int g_checks = 0;

#define CHECK(cond)                                                                                \
  do {                                                                                             \
    ++g_checks;                                                                                    \
    if (!(cond)) {                                                                                 \
      ++g_failures;                                                                                \
      std::cerr << "  FAILED " << __FILE__ << ":" << __LINE__ << ": " #cond "\n";                  \
    }                                                                                              \
  } while (0)

struct TestCase {
  const char *name;
  std::function<void()> fn;
};

fs::path scratch_dir() {
  static fs::path dir = [] {
    fs::path d = fs::temp_directory_path() / ("lisper_tests_" + std::to_string(std::rand()));
    fs::create_directories(d);
    return d;
  }();
  return dir;
}

void write_wav(const fs::path &path, int sample_rate, int channels, int bits, int samples) {
  std::ofstream out(path, std::ios::binary);
  const uint32_t data_bytes = static_cast<uint32_t>(samples * channels * (bits / 8));
  const uint32_t riff_size = 36 + data_bytes;
  const uint16_t fmt = 1;
  const uint16_t ch = static_cast<uint16_t>(channels);
  const uint32_t rate = static_cast<uint32_t>(sample_rate);
  const uint32_t byte_rate = rate * ch * (bits / 8);
  const uint16_t align = static_cast<uint16_t>(ch * (bits / 8));
  const uint16_t bps = static_cast<uint16_t>(bits);
  const uint32_t fmt_size = 16;
  out.write("RIFF", 4);
  out.write(reinterpret_cast<const char *>(&riff_size), 4);
  out.write("WAVEfmt ", 8);
  out.write(reinterpret_cast<const char *>(&fmt_size), 4);
  out.write(reinterpret_cast<const char *>(&fmt), 2);
  out.write(reinterpret_cast<const char *>(&ch), 2);
  out.write(reinterpret_cast<const char *>(&rate), 4);
  out.write(reinterpret_cast<const char *>(&byte_rate), 4);
  out.write(reinterpret_cast<const char *>(&align), 2);
  out.write(reinterpret_cast<const char *>(&bps), 2);
  out.write("data", 4);
  out.write(reinterpret_cast<const char *>(&data_bytes), 4);
  std::vector<char> zeros(data_bytes, 0);
  out.write(zeros.data(), static_cast<std::streamsize>(zeros.size()));
}

TranscriptionResult sample_result() {
  TranscriptionResult r;
  r.success = true;
  r.detected_language = "en";
  r.duration_ms = 3723004;
  r.full_text = "Hello \"world\"\n\x01";
  r.segments.push_back({0, 1500, "Hello"});
  r.segments.push_back({3723000, 3723004, " \"world\"\t\x01"});
  return r;
}

void test_json_escaping() {
  const std::string json =
      formatter::format_result(sample_result(), formatter::Format::JSON, "a\"b.wav");
  CHECK(json.find("\\\"world\\\"") != std::string::npos);
  CHECK(json.find("\\u0001") != std::string::npos);
  CHECK(json.find('\x01') == std::string::npos);
  CHECK(json.find("\"source\": \"a\\\"b.wav\"") != std::string::npos);
  CHECK(json.find("\\t") != std::string::npos);
}

void test_srt_timestamps() {
  const std::string srt = formatter::format_result(sample_result(), formatter::Format::SRT);
  CHECK(srt.find("00:00:00,000 --> 00:00:01,500") != std::string::npos);
  CHECK(srt.find("01:02:03,000 --> 01:02:03,004") != std::string::npos);
}

void test_parse_format() {
  CHECK(formatter::parse_format("SRT") == formatter::Format::SRT);
  CHECK(formatter::parse_format("json") == formatter::Format::JSON);
  CHECK(formatter::parse_format("rag") == formatter::Format::RAG);
  CHECK(formatter::parse_format("nope") == formatter::Format::TEXT);
}

void test_resolve_output_path() {
  const fs::path dir = scratch_dir();
  const std::string p =
      formatter::resolve_output_path(dir.string(), formatter::Format::SRT, "talk.mp4");
  CHECK(fs::path(p).filename() == "talk.srt");
  CHECK(formatter::resolve_output_path("", formatter::Format::TEXT, "x") == "");
}

void test_media_extensions() {
  CHECK(media::is_audio_file("A.WAV"));
  CHECK(media::is_video_file("clip.MkV"));
  CHECK(!media::is_media_file("notes.txt"));
  CHECK(!media::is_media_file("wav"));
}

void test_probe_wav() {
  const fs::path ready = scratch_dir() / "ready.wav";
  const fs::path cd = scratch_dir() / "cd.wav";
  write_wav(ready, 16000, 1, 16, 1600);
  write_wav(cd, 44100, 2, 16, 4410);

  media::WavInfo info;
  CHECK(media::probe_wav(ready.string(), info));
  CHECK(media::wav_is_whisper_ready(info));
  CHECK(media::probe_wav(cd.string(), info));
  CHECK(info.sample_rate == 44100);
  CHECK(!media::wav_is_whisper_ready(info));

  const fs::path junk = scratch_dir() / "junk.wav";
  std::ofstream(junk) << "not a wav";
  CHECK(!media::probe_wav(junk.string(), info));

  const auto prepared = media::prepare_audio(ready.string());
  CHECK(prepared.success && !prepared.is_temp);
  CHECK(!media::prepare_audio((scratch_dir() / "missing.wav").string()).success);
  CHECK(!media::prepare_audio(scratch_dir().string()).success);
}

void test_secure_temp_file() {
  const std::string a = media::create_secure_temp_file(scratch_dir().string(), ".wav");
  const std::string b = media::create_secure_temp_file(scratch_dir().string(), ".wav");
  CHECK(!a.empty());
  CHECK(a != b);
  CHECK(fs::exists(a));
#ifndef _WIN32
  struct stat st {};
  CHECK(::stat(a.c_str(), &st) == 0);
  CHECK((st.st_mode & 0777) == 0600);
#endif
}

void test_ffmpeg_input_arg() {
  const std::string arg = media::ffmpeg_input_arg("concat:a|b");
  CHECK(arg.rfind("file:", 0) == 0);
  CHECK(fs::path(arg.substr(5)).is_absolute());
}

void test_process_no_shell() {
#ifndef _WIN32
  CHECK(proc::run({"true"}) == 0);
  CHECK(proc::run({"false"}) != 0);
  CHECK(proc::run({"definitely-not-a-real-program-xyz"}) != 0);
  // Metacharacters must reach the program verbatim, never a shell.
  const fs::path marker = scratch_dir() / "PWNED";
  CHECK(proc::run({"echo", "$(touch " + marker.string() + ")", ";", "touch", marker.string()}) ==
        0);
  CHECK(!fs::exists(marker));
  proc::RunOptions opts;
  opts.feed_stdin = true;
  opts.stdin_data = "hello";
  CHECK(proc::run({"cat"}, opts) == 0);
#else
  CHECK(proc::quote_windows_arg(L"plain") == L"plain");
  CHECK(proc::quote_windows_arg(L"a b") == L"\"a b\"");
  CHECK(proc::quote_windows_arg(L"say \"hi\"") == L"\"say \\\"hi\\\"\"");
  CHECK(proc::quote_windows_arg(L"C:\\dir\\") == L"C:\\dir\\");
  CHECK(proc::quote_windows_arg(L"C:\\my dir\\") == L"\"C:\\my dir\\\\\"");
#endif
}

void test_ggml_magic() {
  const fs::path good = scratch_dir() / "good.bin";
  const fs::path bad = scratch_dir() / "bad.bin";
  {
    std::ofstream out(good, std::ios::binary);
    const uint32_t magic = 0x67676d6cu;
    out.write(reinterpret_cast<const char *>(&magic), 4);
  }
  std::ofstream(bad) << "<html>";
  CHECK(model_download::looks_like_ggml_model(good.string()));
  CHECK(!model_download::looks_like_ggml_model(bad.string()));
  CHECK(model_download::model_url("ggml-tiny.bin").rfind("https://", 0) == 0);
}

void test_model_profiles() {
  CHECK(find_model_profile("quality") != nullptr);
  CHECK(find_model_profile("tiny") != nullptr);
  CHECK(find_model_profile("bogus") == nullptr);
  CHECK(resolve_model_path("/explicit/model.bin", "quality") == "/explicit/model.bin");
}

void test_paths_private_file() {
  const fs::path target = scratch_dir() / "cfg" / "settings.json";
  CHECK(paths::write_private_file(target.string(), "{}"));
  std::ifstream in(target);
  std::string content;
  in >> content;
  CHECK(content == "{}");
#ifndef _WIN32
  struct stat st {};
  CHECK(::stat(target.c_str(), &st) == 0);
  CHECK((st.st_mode & 0777) == 0600);
#endif
  CHECK(!paths::models_dir().empty());
}

void test_scan_directory() {
  const fs::path dir = scratch_dir() / "scan";
  fs::create_directories(dir);
  std::ofstream(dir / "b.mp3") << "x";
  std::ofstream(dir / "a.wav") << "x";
  std::ofstream(dir / "c.txt") << "x";
  const auto files = watcher::scan_directory(dir.string());
  CHECK(files.size() == 2);
  CHECK(!files.empty() && fs::path(files[0]).filename() == "a.wav");
  CHECK(watcher::scan_directory((dir / "nope").string()).empty());
}

} // namespace

int main() {
  const std::vector<TestCase> tests = {
      {"json escaping", test_json_escaping},
      {"srt timestamps", test_srt_timestamps},
      {"parse format", test_parse_format},
      {"resolve output path", test_resolve_output_path},
      {"media extensions", test_media_extensions},
      {"probe wav", test_probe_wav},
      {"secure temp file", test_secure_temp_file},
      {"ffmpeg input arg", test_ffmpeg_input_arg},
      {"process without shell", test_process_no_shell},
      {"ggml magic", test_ggml_magic},
      {"model profiles", test_model_profiles},
      {"private files", test_paths_private_file},
      {"scan directory", test_scan_directory},
  };

  for (const auto &t : tests) {
    const int before = g_failures;
    t.fn();
    std::cout << (g_failures == before ? "[ OK ] " : "[FAIL] ") << t.name << "\n";
  }

  std::error_code ec;
  fs::remove_all(scratch_dir(), ec);

  std::cout << g_checks << " checks, " << g_failures << " failures\n";
  return g_failures == 0 ? 0 : 1;
}
