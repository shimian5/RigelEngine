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

#include "duke1/exe_text.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>


namespace rigel
{

constexpr int DUKE1_SCREEN_W = 320;
constexpr int DUKE1_SCREEN_H = 200;

/** Draws the Duke Nukem 1 story and message screens from the player's own data
 * files.
 *
 * All pictures come from the selected episode's data files and all text from
 * the player's matching episode EXE; the engine itself contains no game text.
 */
class Duke1Screens
{
public:
  Duke1Screens(
    const std::filesystem::path& dataDir,
    const duke1::ExeText& text);

  /** The introduction after "New Game": the story, then the exchange with Dr.
   * Proton */
  std::vector<std::vector<std::uint8_t>> introPages() const;
  std::vector<std::vector<std::uint8_t>> endingPages() const;

  /** Four-line machine tip from an intermission context (original map-sequence
   * index). */
  std::vector<std::uint8_t> hallwayMessage(int index) const;

  /** Free-standing text box on a dimmed copy of whatever 320x200 picture is
   * given */
  std::vector<std::uint8_t> boxOverPortrait(
    const std::string& portrait,
    const std::vector<std::string>& lines,
    int boxX,
    int boxY) const;

  bool valid() const { return mValid; }

private:
  struct Glyphs
  {
    std::vector<std::vector<std::uint8_t>> tiles; // 8x8 RGBA each
  };

  std::vector<std::uint8_t> portraitPicture(const std::string& name) const;
  std::vector<std::uint8_t> storyBackground() const;
  void drawText(
    std::vector<std::uint8_t>& rgba,
    int x,
    int y,
    const std::string& text) const;
  void drawBox(
    std::vector<std::uint8_t>& rgba,
    int x,
    int y,
    int columns,
    int rows) const;
  void drawCentered(
    std::vector<std::uint8_t>& rgba,
    int y,
    const std::string& text) const;

  std::filesystem::path mDataDir;
  const duke1::ExeText& mText;
  std::vector<std::vector<std::uint8_t>>
    mGlyphs; // index: the original's 0-99 character numbering
  std::vector<std::uint8_t> mPaper; // 16x16 hatched paper tile
  bool mValid = false;
};

} // namespace rigel
