#pragma once

#include "lisper.h"

#include <string>

namespace formatter {

enum class Format { TEXT, SRT, JSON, RAG };

Format parse_format(const std::string &fmt_str);

std::string format_result(const TranscriptionResult &result, Format fmt,
                          const std::string &source_filename = "");

std::string resolve_output_path(const std::string &output_path, Format fmt,
                                const std::string &source_filename = "");

// write formatted output to a file. returns true on success.
bool write_output(const TranscriptionResult &result, Format fmt,
                  const std::string &output_path,
                  const std::string &source_filename = "");

} // namespace formatter
