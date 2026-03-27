#include "formatter.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace formatter {

Format parse_format(const std::string &fmt_str) {
  std::string f = fmt_str;
  std::transform(f.begin(), f.end(), f.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });

  if (f == "srt")
    return Format::SRT;
  if (f == "json")
    return Format::JSON;
  if (f == "rag")
    return Format::RAG;
  return Format::TEXT;
}

static std::string ms_to_srt_time(int64_t ms) {
  int hours = static_cast<int>(ms / 3600000);
  int minutes = static_cast<int>((ms % 3600000) / 60000);
  int seconds = static_cast<int>((ms % 60000) / 1000);
  int millis = static_cast<int>(ms % 1000);

  std::ostringstream ss;
  ss << std::setfill('0') << std::setw(2) << hours << ":" << std::setw(2)
     << minutes << ":" << std::setw(2) << seconds << "," << std::setw(3)
     << millis;
  return ss.str();
}

static std::string escape_json(const std::string &s) {
  std::string out;
  out.reserve(s.size() + 16);
  for (char c : s) {
    switch (c) {
    case '"':
      out += "\\\"";
      break;
    case '\\':
      out += "\\\\";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      out += "\\r";
      break;
    case '\t':
      out += "\\t";
      break;
    default:
      out += c;
    }
  }
  return out;
}

static std::string format_text(const TranscriptionResult &result) {
  return result.full_text + "\n";
}

static std::string format_srt(const TranscriptionResult &result) {
  std::ostringstream ss;
  for (size_t i = 0; i < result.segments.size(); i++) {
    const auto &seg = result.segments[i];
    ss << (i + 1) << "\n"
       << ms_to_srt_time(seg.start_ms) << " --> " << ms_to_srt_time(seg.end_ms)
       << "\n"
       << seg.text << "\n\n";
  }
  return ss.str();
}

static std::string format_json(const TranscriptionResult &result,
                               const std::string &source) {
  std::ostringstream ss;
  ss << "{\n"
     << "  \"source\": \"" << escape_json(source) << "\",\n"
     << "  \"language\": \"" << result.detected_language << "\",\n"
     << "  \"duration_ms\": " << result.duration_ms << ",\n"
     << "  \"text\": \"" << escape_json(result.full_text) << "\",\n"
     << "  \"segments\": [\n";

  for (size_t i = 0; i < result.segments.size(); i++) {
    const auto &seg = result.segments[i];
    ss << "    {"
       << "\"start\": " << seg.start_ms << ", "
       << "\"end\": " << seg.end_ms << ", "
       << "\"text\": \"" << escape_json(seg.text) << "\""
       << "}";
    if (i + 1 < result.segments.size())
      ss << ",";
    ss << "\n";
  }

  ss << "  ]\n}\n";
  return ss.str();
}

static std::string format_rag(const TranscriptionResult &result,
                              const std::string &source) {
  // split transcription into ~500 character chunks at sentence boundaries
  // for vector database ingestion
  std::ostringstream ss;
  std::string chunk;
  int chunk_index = 0;

  for (const auto &seg : result.segments) {
    chunk += seg.text;

    // flush chunk if it's large enough and ends at a sentence boundary
    bool at_boundary =
        !chunk.empty() &&
        (chunk.back() == '.' || chunk.back() == '!' || chunk.back() == '?');

    if (chunk.size() >= 400 && at_boundary) {
      // trim leading whitespace
      size_t start = chunk.find_first_not_of(" \t\n");
      if (start != std::string::npos) {
        chunk = chunk.substr(start);
      }

      ss << "Source: " << source << " [chunk " << chunk_index << "]\n"
         << chunk << "\n\n---\n\n";
      chunk.clear();
      chunk_index++;
    }
  }

  // flush remaining text
  if (!chunk.empty()) {
    size_t start = chunk.find_first_not_of(" \t\n");
    if (start != std::string::npos) {
      chunk = chunk.substr(start);
    }
    ss << "Source: " << source << " [chunk " << chunk_index << "]\n"
       << chunk << "\n";
  }

  return ss.str();
}

std::string format_result(const TranscriptionResult &result, Format fmt,
                          const std::string &source_filename) {
  switch (fmt) {
  case Format::SRT:
    return format_srt(result);
  case Format::JSON:
    return format_json(result, source_filename);
  case Format::RAG:
    return format_rag(result, source_filename);
  default:
    return format_text(result);
  }
}

std::string resolve_output_path(const std::string &output_path, Format fmt,
                                const std::string &source_filename) {
  if (output_path.empty()) {
    return "";
  }

  std::string final_path = output_path;
  if (std::filesystem::is_directory(output_path)) {
    std::string base = source_filename;
    if (base.empty())
      base = "transcription";

    // strip extension from source
    auto dot = base.rfind('.');
    if (dot != std::string::npos)
      base = base.substr(0, dot);

    std::string ext;
    switch (fmt) {
    case Format::SRT:
      ext = ".srt";
      break;
    case Format::JSON:
      ext = ".json";
      break;
    case Format::RAG:
      ext = ".txt";
      break;
    default:
      ext = ".txt";
      break;
    }

    final_path = output_path + "/" + base + ext;
  }
  return final_path;
}

bool write_output(const TranscriptionResult &result, Format fmt,
                  const std::string &output_path,
                  const std::string &source_filename) {
  std::string content = format_result(result, fmt, source_filename);
  std::string final_path =
      resolve_output_path(output_path, fmt, source_filename);

  std::ofstream out(final_path);
  if (!out.is_open()) {
    std::cerr << "Failed to write: " << final_path << "\n";
    return false;
  }

  out << content;
  std::cout << "Output written to: " << final_path << "\n";
  return true;
}

} // namespace formatter
