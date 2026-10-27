/* Copyright (C) 2026, RigelEngine contributors. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace rigel::duke1
{

// The story, dialogue and hint texts of Duke Nukem 1 live inside the
// (compressed) DN1.EXE. This unpacks the player's own copy in memory and reads
// the text rows from it. Nothing is stored by us: no game text is part of the
// engine.
class ExeText
{
public:
  struct LevelInfo
  {
    std::string filename;
    std::array<std::string, 2> backdrops;
    int sequenceIndex = 0;
    int intermissionMessage = 0;
  };

  struct TextDraw
  {
    int offset = 0;
    int column = 0;
    int row = 0;
  };

  struct PortraitPage
  {
    bool player = false;
    std::vector<TextDraw> lines;
  };

  struct TextLayout
  {
    std::vector<std::vector<TextDraw>> introPages;
    std::array<std::vector<TextDraw>, 3> dialogue;
    std::vector<PortraitPage> ending;
    int tipTable = 0;
    int tipCount = 0;
  };

  static std::optional<ExeText>
    load(const std::filesystem::path& dataDir, int episode = 0);
  static std::optional<ExeText>
    fromImage(std::vector<std::uint8_t> image, int episode);

  int episode() const { return mEpisode; }
  const std::vector<LevelInfo>& levels() const { return mLevels; }
  const std::vector<LevelInfo>& sequence() const { return mSequence; }
  const TextLayout& layout() const { return mLayout; }
  std::string assetName(const std::string& stem) const;
  std::optional<std::array<std::string, 2>>
    backdropPair(const std::string& filename) const;

  // NUL terminated text at the given data-segment offset
  std::string at(int dsOffset) const;
  // `count` fixed-stride rows starting at a data-segment offset (trailing
  // blanks removed)
  std::vector<std::string> rows(int dsOffset, int count, int stride) const;
  // Contextual four-line machine tip number `index` (original map-sequence
  // index)
  std::vector<std::string> tipMessage(int index) const;

  // Validate the decoded DOS table layout before reading any resource or text.
  bool plausible() const;

private:
  std::vector<std::uint8_t> mImage;
  std::size_t mDataSegment = 0;
  int mEpisode = 0;
  std::vector<LevelInfo> mSequence;
  std::vector<LevelInfo> mLevels;
  TextLayout mLayout{};
};

// Decompress an LZEXE 0.91 packed MZ executable; returns the load image (code
// and data), or nothing.
std::optional<std::vector<std::uint8_t>>
  unpackLzexe91(const std::vector<std::uint8_t>& exe);

} // namespace rigel::duke1
