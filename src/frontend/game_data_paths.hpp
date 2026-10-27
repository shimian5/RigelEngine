#pragma once

#include <filesystem>
#include <optional>

namespace rigel
{
// Find Duke II in the selected folder, its DN2/Duke2 child, or next to a
// selected DN1/Duke1 folder.
std::optional<std::filesystem::path>
  findDuke2DataDir(const std::filesystem::path& selected);
} // namespace rigel
