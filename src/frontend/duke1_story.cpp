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

#include "duke1_story.hpp"

#include "duke1/assets.hpp"

#include <algorithm>
#include <array>
#include <cstring>


namespace rigel
{

namespace
{

// The data segment offset is the file offset in an unpacked DN1.EXE minus 0x19810; these are the
// places where the text rows of the 1.0 release live.
constexpr int DS_BIAS = 0x19810;
constexpr int STORY_ONE = 0x1b5fb - DS_BIAS;
constexpr int STORY_ONE_ROWS = 19;
constexpr int STORY_TWO = 0x1b92f - DS_BIAS;
constexpr int STORY_TWO_ROWS = 9;
constexpr int EPISODE_ONE = 0x1d113 - DS_BIAS;
constexpr int EPISODE_ONE_ROWS = 8;
constexpr int TEXT_STRIDE = 41;
constexpr int PROTON_SPEAKS = 0x1db7e - DS_BIAS;
constexpr int DUKE_SPEAKS = 0x1dbe2 - DS_BIAS;
constexpr int GET_READY = 0x1c89d - DS_BIAS;
constexpr int DIALOGUE_ROWS = 4;
constexpr int DIALOGUE_STRIDE = 25;

constexpr std::array<std::array<std::uint8_t, 3>, 16> EGA = {{
  {0, 0, 0},       {0, 0, 170},     {0, 170, 0},     {0, 170, 170},
  {170, 0, 0},     {170, 0, 170},   {170, 85, 0},    {170, 170, 170},
  {85, 85, 85},    {85, 85, 255},   {85, 255, 85},   {85, 255, 255},
  {255, 85, 85},   {255, 85, 255},  {255, 255, 85},  {255, 255, 255}}};


void fillRect(
  std::vector<std::uint8_t>& rgba,
  const int x,
  const int y,
  const int w,
  const int h,
  const std::array<std::uint8_t, 3>& color)
{
  for (int j = y; j < y + h; ++j)
  {
    for (int i = x; i < x + w; ++i)
    {
      if (i < 0 || j < 0 || i >= DUKE1_SCREEN_W || j >= DUKE1_SCREEN_H)
      {
        continue;
      }
      auto* p = &rgba[(std::size_t(j) * DUKE1_SCREEN_W + std::size_t(i)) * 4];
      p[0] = color[0];
      p[1] = color[1];
      p[2] = color[2];
      p[3] = 255;
    }
  }
}


void blit8(
  std::vector<std::uint8_t>& rgba,
  const std::vector<std::uint8_t>& tile,
  const int x,
  const int y,
  const bool opaque)
{
  for (int j = 0; j < 8; ++j)
  {
    for (int i = 0; i < 8; ++i)
    {
      const auto* s = &tile[(std::size_t(j) * 8 + std::size_t(i)) * 4];
      const int px = x + i;
      const int py = y + j;
      if (px < 0 || py < 0 || px >= DUKE1_SCREEN_W || py >= DUKE1_SCREEN_H)
      {
        continue;
      }
      if (!opaque && s[3] == 0)
      {
        continue;
      }
      auto* d = &rgba[(std::size_t(py) * DUKE1_SCREEN_W + std::size_t(px)) * 4];
      d[0] = s[0];
      d[1] = s[1];
      d[2] = s[2];
      d[3] = 255;
    }
  }
}


// The two font sheets form one numbering: 0-49 first sheet, 50-99 second; capitals follow the space
// and lower case letters map to the second alphabet.
int glyphIndex(const unsigned char c)
{
  const int index = c >= 97 ? 69 + (c - 97) : 10 + (c - 32);
  return index >= 0 && index < 100 ? index : -1;
}

} // namespace


Duke1Screens::Duke1Screens(const std::filesystem::path& dataDir, const duke1::ExeText& text)
  : mDataDir(dataDir)
  , mText(text)
{
  try
  {
    const auto font1 = duke1::parse_tiles(duke1::read_file(dataDir, "FONT1.DN1"));
    const auto font2 = duke1::parse_tiles(duke1::read_file(dataDir, "FONT2.DN1"));
    for (const auto* set : {&font1, &font2})
    {
      for (const auto& tile : set->tiles)
      {
        mGlyphs.push_back(tile.rgba());
      }
    }
    const auto border = duke1::parse_tiles(duke1::read_file(dataDir, "BORDER.DN1"));
    if (mGlyphs.size() >= 100 && border.tiles.size() > 17)
    {
      mPaper = border.tiles[17].rgba(); // 16x16 diagonal red hatching
      mValid = true;
    }
  }
  catch (const std::exception&)
  {
    mValid = false;
  }
}


void Duke1Screens::drawText(
  std::vector<std::uint8_t>& rgba,
  const int x,
  const int y,
  const std::string& text) const
{
  int cursor = x;
  for (const auto ch : text)
  {
    const auto index = glyphIndex(static_cast<unsigned char>(ch));
    if (index >= 0 && std::size_t(index) < mGlyphs.size())
    {
      blit8(rgba, mGlyphs[std::size_t(index)], cursor, y, false);
    }
    cursor += 8;
  }
}


void Duke1Screens::drawCentered(std::vector<std::uint8_t>& rgba, const int y, const std::string& text) const
{
  drawText(rgba, std::max(0, (DUKE1_SCREEN_W - int(text.size()) * 8) / 2), y, text);
}


std::vector<std::uint8_t> Duke1Screens::storyBackground() const
{
  // The story pages show the hatching dimmed: dark red lines on black
  std::vector<std::uint8_t> rgba(std::size_t(DUKE1_SCREEN_W) * DUKE1_SCREEN_H * 4, 0);
  for (int y = 0; y < DUKE1_SCREEN_H; ++y)
  {
    for (int x = 0; x < DUKE1_SCREEN_W; ++x)
    {
      const auto* s = &mPaper[(std::size_t(y % 16) * 16 + std::size_t(x % 16)) * 4];
      auto* d = &rgba[(std::size_t(y) * DUKE1_SCREEN_W + std::size_t(x)) * 4];
      const bool line = s[0] == EGA[12][0]; // the light red diagonal
      d[0] = line ? EGA[4][0] : 0;
      d[1] = 0;
      d[2] = 0;
      d[3] = 255;
    }
  }
  return rgba;
}


void Duke1Screens::drawBox(
  std::vector<std::uint8_t>& rgba,
  const int x,
  const int y,
  const int columns,
  const int rows) const
{
  // Hatched paper inside a border of blue squares with a cyan edge
  for (int r = 0; r < rows + 2; ++r)
  {
    for (int c = 0; c < columns + 2; ++c)
    {
      const bool border = r == 0 || c == 0 || r == rows + 1 || c == columns + 1;
      if (border)
      {
        fillRect(rgba, x + c * 8, y + r * 8, 8, 8, EGA[1]);
        fillRect(rgba, x + c * 8 + 1, y + r * 8 + 1, 6, 1, EGA[11]);
        fillRect(rgba, x + c * 8 + 1, y + r * 8 + 1, 1, 6, EGA[11]);
      }
      else
      {
        for (int j = 0; j < 8; ++j)
        {
          for (int i = 0; i < 8; ++i)
          {
            const auto px = (c * 8 + i) % 16;
            const auto py = (r * 8 + j) % 16;
            const auto* s = &mPaper[(std::size_t(py) * 16 + std::size_t(px)) * 4];
            const int dx = x + c * 8 + i;
            const int dy = y + r * 8 + j;
            if (dx >= 0 && dy >= 0 && dx < DUKE1_SCREEN_W && dy < DUKE1_SCREEN_H)
            {
              auto* d = &rgba[(std::size_t(dy) * DUKE1_SCREEN_W + std::size_t(dx)) * 4];
              d[0] = s[0];
              d[1] = s[1];
              d[2] = s[2];
              d[3] = 255;
            }
          }
        }
      }
    }
  }
}


std::vector<std::uint8_t> Duke1Screens::portraitPicture(const std::string& name) const
{
  std::vector<std::uint8_t> rgba(std::size_t(DUKE1_SCREEN_W) * DUKE1_SCREEN_H * 4, 255);
  try
  {
    const auto screen = duke1::parse_screen(duke1::read_file(mDataDir, name));
    for (std::size_t i = 0; i < screen.indices.size() && i * 4 + 3 < rgba.size(); ++i)
    {
      const auto& color = EGA[screen.indices[i] & 15];
      rgba[i * 4 + 0] = color[0];
      rgba[i * 4 + 1] = color[1];
      rgba[i * 4 + 2] = color[2];
      rgba[i * 4 + 3] = 255;
    }
  }
  catch (const std::exception&)
  {
    std::fill(rgba.begin(), rgba.end(), 0);
    for (std::size_t i = 3; i < rgba.size(); i += 4)
    {
      rgba[i] = 255;
    }
  }
  return rgba;
}


std::vector<std::uint8_t> Duke1Screens::boxOverPortrait(
  const std::string& portrait,
  const std::vector<std::string>& lines,
  const int boxX,
  const int boxY) const
{
  auto rgba = portraitPicture(portrait);
  std::size_t width = 0;
  for (const auto& line : lines)
  {
    width = std::max(width, line.size());
  }
  const int columns = int(width) + 2;
  drawBox(rgba, boxX, boxY, columns, int(lines.size()) + 0);
  int y = boxY + 8;
  for (const auto& line : lines)
  {
    drawText(rgba, boxX + 16, y, line);
    y += 8;
  }
  return rgba;
}


std::vector<std::vector<std::uint8_t>> Duke1Screens::introPages() const
{
  std::vector<std::vector<std::uint8_t>> pages;
  if (!mValid)
  {
    return pages;
  }

  auto page = storyBackground();
  int y = 16;
  for (const auto& row : mText.rows(STORY_ONE, STORY_ONE_ROWS, TEXT_STRIDE))
  {
    drawText(page, 0, y, row);
    y += 8;
  }
  pages.push_back(std::move(page));

  page = storyBackground();
  y = 16;
  for (const auto& row : mText.rows(STORY_TWO, STORY_TWO_ROWS, TEXT_STRIDE))
  {
    drawText(page, 0, y, row);
    y += 8;
  }
  y += 8;
  for (const auto& row : mText.rows(EPISODE_ONE, EPISODE_ONE_ROWS, TEXT_STRIDE))
  {
    drawText(page, 0, y, row);
    y += 8;
  }
  pages.push_back(std::move(page));

  pages.push_back(boxOverPortrait(
    "BADGUY.DN1", mText.rows(PROTON_SPEAKS, DIALOGUE_ROWS, DIALOGUE_STRIDE), 0, 144));
  pages.push_back(boxOverPortrait(
    "DUKE.DN1", mText.rows(DUKE_SPEAKS, DIALOGUE_ROWS, DIALOGUE_STRIDE), 0, 144));
  pages.push_back(boxOverPortrait(
    "DUKE.DN1", mText.rows(GET_READY, 2, DIALOGUE_STRIDE), 112, 72));
  return pages;
}


std::vector<std::uint8_t> Duke1Screens::hallwayMessage(const int index) const
{
  const auto lines = mText.hallwayMessage(index);
  if (lines.empty())
  {
    return {};
  }
  return boxOverPortrait("BADGUY.DN1", lines, 0, 144);
}

} // namespace rigel
