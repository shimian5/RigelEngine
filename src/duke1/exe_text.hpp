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

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace duke1 {

// The story, dialogue and hint texts of Duke Nukem 1 live inside the (compressed) DN1.EXE.
// This unpacks the player's own copy in memory and reads the text rows from it. Nothing is stored
// by us: no game text is part of the engine.
class ExeText {
 public:
  static std::optional<ExeText> load(const std::filesystem::path& dataDir);

  // NUL terminated text at the given data-segment offset
  std::string at(int dsOffset) const;
  // `count` fixed-stride rows starting at a data-segment offset (trailing blanks removed)
  std::vector<std::string> rows(int dsOffset, int count, int stride) const;
  // The four text rows of hallway message number `index` (Dr. Proton's messages)
  std::vector<std::string> hallwayMessage(int index) const;

  // Sanity check: does this look like the DN1 1.0 executable these offsets were made for?
  bool plausible() const;

 private:
  std::vector<std::uint8_t> image_;
};

// Decompress an LZEXE 0.91 packed MZ executable; returns the load image (code and data), or nothing.
std::optional<std::vector<std::uint8_t>> unpackLzexe91(const std::vector<std::uint8_t>& exe);

}  // namespace duke1
