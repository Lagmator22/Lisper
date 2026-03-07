#pragma once

#include <optional>
#include <string>

namespace gui {

std::optional<std::string> pick_input_media_file(
    const std::string &initial_path = "");
std::optional<std::string> pick_output_folder(
    const std::string &initial_path = "");
std::optional<std::string> pick_output_file(
    const std::string &suggested_path = "");

} // namespace gui
