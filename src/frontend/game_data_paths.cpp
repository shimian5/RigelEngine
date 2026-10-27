#include "game_data_paths.hpp"

#include "assets/file_utils.hpp"
#include "base/string_utils.hpp"

#include <algorithm>
#include <vector>

namespace rigel
{
namespace
{
std::optional<std::filesystem::path> findIn(const std::filesystem::path& base)
{
  namespace fs = std::filesystem;
  if (assets::findFileIgnoringCase(base, "NUKEM2.CMP"))
    return base;

  std::vector<fs::path> candidates;
  std::error_code error;
  for (fs::directory_iterator it(base, error), end; !error && it != end;
       it.increment(error))
  {
    std::error_code itemError;
    const auto name = strings::toUppercase(it->path().filename().u8string());
    if (
      it->is_directory(itemError) && !itemError &&
      (name == "DN2" || name == "DUKE2"))
      candidates.push_back(it->path());
  }
  std::sort(candidates.begin(), candidates.end());
  for (const auto& candidate : candidates)
  {
    if (assets::findFileIgnoringCase(candidate, "NUKEM2.CMP"))
      return candidate;
  }
  return std::nullopt;
}
} // namespace

std::optional<std::filesystem::path>
  findDuke2DataDir(const std::filesystem::path& selected)
{
  if (selected.empty())
    return std::nullopt;

  const auto base =
    selected.filename().empty() ? selected.parent_path() : selected;
  if (const auto result = findIn(base))
    return result;

  const auto name = strings::toUppercase(base.filename().u8string());
  if (name == "DN1" || name == "DUKE1")
    return findIn(base.parent_path());
  return std::nullopt;
}
} // namespace rigel
