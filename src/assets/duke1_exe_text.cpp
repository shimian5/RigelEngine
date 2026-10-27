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

#include "assets/duke1_exe_text.hpp"
#include "assets/duke1_assets.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace duke1
{

namespace
{

// Shared DOS data-segment table offsets verified independently in all four
// EXEs.
constexpr std::size_t LEVEL_TABLE = 0x3a4;
constexpr std::size_t BACKDROP_TABLE = 0xb47;

std::string upper(std::string text)
{
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
    return static_cast<char>(std::toupper(c));
  });
  return text;
}

std::uint16_t word(const std::vector<std::uint8_t>& d, const std::size_t at)
{
  return static_cast<std::uint16_t>(d[at] | (d[at + 1] << 8));
}

// Decode only literal arguments to the DOS text printer. This is metadata
// inspection, not an x86 interpreter; unfamiliar instruction sequences are
// ignored.
struct TextCall
{
  std::size_t address;
  std::size_t target;
  std::size_t end;
  ExeText::TextDraw draw;
};

std::vector<TextCall> textCalls(
  const std::vector<std::uint8_t>& image,
  std::size_t begin,
  std::size_t end,
  std::size_t codeBase)
{
  std::vector<TextCall> calls;
  end = std::min(end, image.size());
  for (auto start = begin; start + 16 < end; ++start)
  {
    if (image[start] != 0x1e)
      continue; // push DS
    auto p = start + 1;
    const auto argument = [&]() -> std::optional<int> {
      int value;
      if (p + 4 <= end && image[p] == 0xb8)
      { // mov AX, immediate
        value = word(image, p + 1);
        p += 3;
      }
      else if (p + 3 <= end && image[p] == 0x33 && image[p + 1] == 0xc0)
      {
        value = 0;
        p += 2;
      }
      else if (p < end && image[p] == 0x50)
      {
        // Reusing AX is handled explicitly by the caller.
        return std::nullopt;
      }
      else
        return std::nullopt;
      if (p >= end || image[p++] != 0x50)
        return std::nullopt;
      return value;
    };
    const auto offset = argument();
    const auto column = argument();
    std::optional<int> row;
    if (column && p < end && image[p] == 0x50)
    {
      row = *column;
      ++p;
    }
    else
      row = argument();
    if (!offset || !column || !row || *column > 39 || *row > 24 || p + 4 > end)
      continue;
    std::size_t target;
    std::size_t callEnd;
    if (p + 5 <= end && image[p] == 0x9a)
    {
      target = std::size_t(word(image, p + 3)) * 16 + word(image, p + 1);
      callEnd = p + 5;
    }
    else if (image[p] == 0x0e && image[p + 1] == 0xe8)
    {
      callEnd = p + 4;
      target =
        codeBase +
        static_cast<std::uint16_t>(
          callEnd - codeBase + static_cast<std::int16_t>(word(image, p + 2)));
    }
    else
      continue;
    if (target >= image.size())
      continue;
    calls.push_back({start, target, callEnd, {*offset, *column, *row}});
  }
  return calls;
}

void readTextLayout(
  const std::vector<std::uint8_t>& image,
  ExeText::TextLayout& layout)
{
  // The story routine has this instruction signature in the supplied DOS
  // builds. Recover its segment instead of assuming a shareware or registered
  // code base.
  std::optional<std::size_t> ui;
  std::size_t storyOffset = 0;
  for (std::size_t p = 0x62c; p + 16 < image.size(); ++p)
  {
    if ((p & 15) != 12 && (p & 15) != 14)
      continue;
    if (
      image[p] == 0xb8 && word(image, p + 1) == 20 && image[p + 3] == 0x50 &&
      image[p + 4] == 0x0e && image[p + 5] == 0xe8 && image[p + 8] == 0xb8 &&
      word(image, p + 9) == 0x1710 && image[p + 11] == 0x50 &&
      image[p + 12] == 0x0e && image[p + 13] == 0xe8)
    {
      if (ui)
        return; // ambiguous executable
      storyOffset = (p & 15) == 14 ? 0x62e : 0x62c;
      ui = p - storyOffset;
    }
  }
  if (!ui)
    return;
  const auto episodeOffset = storyOffset == 0x62e ? 0x26bf : 0x26d5;
  const auto readyOffset = storyOffset == 0x62e ? 0x1456 : 0x1454;
  const auto episode = textCalls(image, *ui + episodeOffset, *ui + 0x2900, *ui);
  if (episode.empty())
    return;
  const auto printer = episode.front().target;
  const auto calls = textCalls(image, *ui, *ui + 0x5000, *ui);
  std::vector<ExeText::TextDraw> page;
  for (const auto& call : calls)
  {
    if (
      call.target != printer || call.address < *ui + storyOffset ||
      call.address >= *ui + readyOffset)
      continue;
    if (call.draw.row == 0 && !page.empty())
    {
      layout.introPages.push_back(std::move(page));
      page.clear();
      if (layout.introPages.size() == 2)
        break;
    }
    page.push_back(call.draw);
  }
  if (layout.introPages.size() < 2 && !page.empty())
    layout.introPages.push_back(std::move(page));
  if (layout.introPages.size() == 2)
  {
    auto next = *ui + episodeOffset;
    for (const auto& call : episode)
    {
      if (call.target != printer || call.address != next)
        break;
      layout.introPages[1].push_back(call.draw);
      next = call.end;
    }
  }
  // These three literal-dialogue shapes distinguish the two portraits and ready
  // prompt. Their text addresses vary with the supplied EXE, so retain no
  // strings.
  for (std::size_t i = 0; i < calls.size(); ++i)
  {
    const auto& first = calls[i];
    if (first.target != printer)
      continue;
    int kind = -1;
    if (first.draw.row == 19 && first.draw.column == 1)
      kind = 0;
    if (first.draw.row == 19 && first.draw.column == 11)
      kind = 1;
    if (
      first.draw.row == 11 && first.draw.column == 3 &&
      first.address >= *ui + readyOffset && first.address < *ui + 0x1500)
      kind = 2;
    if (kind < 0)
      continue;
    std::vector<ExeText::TextDraw> draws;
    auto nextAddress = first.address;
    for (std::size_t j = i; j < calls.size() && draws.size() < 4; ++j)
    {
      const auto& next = calls[j];
      if (
        next.address != nextAddress || next.target != printer ||
        next.draw.column != first.draw.column ||
        next.draw.row != first.draw.row + static_cast<int>(draws.size()))
        break;
      draws.push_back(next.draw);
      nextAddress = next.end;
    }
    if (layout.dialogue[std::size_t(kind)].empty())
      layout.dialogue[std::size_t(kind)] = std::move(draws);
    else if (kind != 2)
      layout.ending.push_back({kind == 1, std::move(draws)});
  }
}

// LZEXE's bit reader: bits come least significant first from 16 bit words
struct Bits
{
  const std::vector<std::uint8_t>& data;
  std::size_t pos;
  std::uint16_t buffer = 0;
  int count = 16;

  Bits(const std::vector<std::uint8_t>& d, const std::size_t start)
    : data(d)
    , pos(start)
  {
    refill();
  }
  void refill()
  {
    if (pos + 1 < data.size())
      buffer = word(data, pos);
    pos += 2;
  }
  int bit()
  {
    const int b = buffer & 1;
    buffer >>= 1;
    if (--count == 0)
    {
      refill();
      count = 16;
    }
    return b;
  }
  int byte() { return pos < data.size() ? data[pos++] : 0; }
};

} // namespace

std::optional<std::vector<std::uint8_t>>
  unpackLzexe91(const std::vector<std::uint8_t>& exe)
{
  if (exe.size() < 0x40 || exe[0] != 'M' || exe[1] != 'Z')
    return std::nullopt;
  const std::size_t headerParas = word(exe, 8);
  const std::size_t initialCs = word(exe, 0x16);
  if (word(exe, 0x18) != 0x1c || std::memcmp(&exe[0x1c], "LZ91", 4) != 0)
    return std::nullopt;

  // The packer's stub sits at the entry point; its first words describe the
  // compressed block
  const std::size_t stub = (headerParas + initialCs) << 4;
  if (stub + 16 > exe.size())
    return std::nullopt;
  const std::size_t compressedParas = word(exe, stub + 8);
  const std::size_t start = (headerParas + initialCs - compressedParas) << 4;
  if (start >= exe.size())
    return std::nullopt;

  std::vector<std::uint8_t> out;
  out.reserve(0x30000);
  Bits bits{exe, start};
  for (;;)
  {
    if (bits.pos > exe.size() + 4 || out.size() > 0x100000)
      return std::nullopt;
    if (bits.bit())
    {
      out.push_back(static_cast<std::uint8_t>(bits.byte()));
      continue;
    }
    int length;
    int span;
    if (!bits.bit())
    {
      length = bits.bit() << 1;
      length |= bits.bit();
      length += 2;
      span = bits.byte() | 0xff00;
    }
    else
    {
      span = bits.byte();
      length = bits.byte();
      span |= ((length & ~0x07) << 5) | 0xe000;
      length = (length & 0x07) + 2;
      if (length == 2)
      {
        length = bits.byte();
        if (length == 0)
          break; // end of the packed module
        if (length == 1)
          continue; // segment change: nothing to do for a flat image
        ++length;
      }
    }
    const auto distance = static_cast<int>(static_cast<std::int16_t>(span));
    for (; length > 0; --length)
    {
      if (static_cast<int>(out.size()) + distance < 0)
        return std::nullopt;
      out.push_back(
        out[static_cast<std::size_t>(static_cast<int>(out.size()) + distance)]);
    }
  }
  return out;
}

std::optional<ExeText>
  ExeText::load(const std::filesystem::path& dataDir, const int episode)
{
  if (episode < 0 || episode > 2)
    return std::nullopt;
  try
  {
    const auto exe =
      read_file(dataDir, "DN" + std::to_string(episode + 1) + ".EXE");
    auto image = unpackLzexe91(exe);
    return image ? fromImage(std::move(*image), episode) : std::nullopt;
  }
  catch (const std::exception&)
  {
    return std::nullopt;
  }
}

std::optional<ExeText>
  ExeText::fromImage(std::vector<std::uint8_t> image, const int episode)
{
  if (episode < 0 || episode > 2 || image.size() < 0x20)
    return std::nullopt;
  // Borland's startup supplies DS in DX before the DOS-version call.
  if (
    image[0] != 0xba || image[3] != 0x2e || image[0x14] != 0x8e ||
    image[0x15] != 0xda)
    return std::nullopt;
  ExeText text;
  text.image_ = std::move(image);
  text.dataSegment_ = std::size_t(word(text.image_, 1)) * 16;
  text.episode_ = episode;
  if (text.dataSegment_ >= text.image_.size())
    return std::nullopt;

  const auto resourceAt =
    [&](const std::size_t table, const std::size_t index) {
      const auto p = text.dataSegment_ + table + index * 4;
      if (p + 4 > text.image_.size())
        return std::string{};
      const auto address =
        std::size_t(word(text.image_, p + 2)) * 16 + word(text.image_, p);
      if (address >= text.image_.size())
        return std::string{};
      std::string name;
      for (auto i = address; i < text.image_.size() && name.size() < 13; ++i)
      {
        if (text.image_[i] == 0)
          return upper(name);
        name.push_back(static_cast<char>(text.image_[i]));
      }
      return std::string{};
    };
  const auto suffix = ".DN" + std::to_string(episode + 1);
  for (std::size_t i = 0; i < 64; ++i)
  {
    const auto name = resourceAt(LEVEL_TABLE, i);
    if (
      name.size() != 12 || name.compare(0, 7, "WORLDAL") != 0 ||
      name.substr(8) != suffix ||
      !((name[7] >= '1' && name[7] <= '9') ||
        (name[7] >= 'A' && name[7] <= 'C')))
      break;
    text.sequence_.push_back({name, {}, static_cast<int>(i), 0});
  }
  if (
    text.sequence_.size() < 3 || text.sequence_.size() == 64 ||
    text.sequence_.size() % 2 == 0)
    return std::nullopt;
  const auto hallway = text.sequence_[1].filename;
  for (std::size_t i = 0; i < text.sequence_.size(); i += 2)
  {
    auto& stage = text.sequence_[i];
    for (std::size_t j = 0; j < 2; ++j)
    {
      const auto drop = resourceAt(BACKDROP_TABLE, i + j);
      if (
        drop.size() < 9 || drop.size() > 10 ||
        drop.compare(0, 4, "DROP") != 0 ||
        drop.substr(drop.size() - 4) != suffix ||
        !std::all_of(drop.begin() + 4, drop.end() - 4, [](unsigned char c) {
          return std::isdigit(c);
        }))
        return std::nullopt;
      stage.backdrops[j] = drop;
    }
    if (i + 1 < text.sequence_.size())
    {
      auto& room = text.sequence_[i + 1];
      if (room.filename != hallway || stage.filename == hallway)
        return std::nullopt;
      room.backdrops = stage.backdrops;
      stage.intermissionMessage = static_cast<int>(i + 1);
      text.levels_.push_back(stage);
    }
  }
  readTextLayout(text.image_, text.layout_);
  text.layout_.tipTable = 0xba1;
  // The message table terminates when its pointers leave this data segment.
  // Four pointers form a message; incomplete groups are never exposed.
  for (int group = 0; group < 64; ++group)
  {
    bool valid = true;
    for (int row = 0; row < 4; ++row)
    {
      const auto at =
        text.dataSegment_ + 0xba1 + std::size_t(group * 16 + row * 4);
      if (
        at + 4 > text.image_.size() ||
        std::size_t(word(text.image_, at + 2)) * 16 != text.dataSegment_ ||
        text.dataSegment_ + word(text.image_, at) >= text.image_.size())
      {
        valid = false;
        break;
      }
    }
    if (!valid)
      break;
    ++text.layout_.tipCount;
  }
  if (!text.plausible())
    return std::nullopt;
  return text;
}

std::string ExeText::assetName(const std::string& stem) const
{
  return asset_name(stem, episode_);
}

std::optional<std::array<std::string, 2>>
  ExeText::backdropPair(const std::string& filename) const
{
  auto name = upper(filename);
  if (std::filesystem::path(name).extension().empty())
    name += ".DN" + std::to_string(episode_ + 1);
  std::optional<std::array<std::string, 2>> pair;
  for (const auto& stage : sequence_)
  {
    if (stage.filename != name)
      continue;
    if (pair && *pair != stage.backdrops)
      return std::nullopt;
    pair = stage.backdrops;
  }
  return pair;
}

bool ExeText::plausible() const
{
  return !levels_.empty() && dataSegment_ < image_.size() &&
    std::size_t(layout_.tipTable + layout_.tipCount * 16) <=
    image_.size() - dataSegment_;
}

std::string ExeText::at(const int dsOffset) const
{
  const auto start = static_cast<std::size_t>(dataSegment_ + dsOffset);
  if (dsOffset < 0 || start >= image_.size())
    return {};
  std::string s;
  for (std::size_t i = start;
       i < image_.size() && image_[i] != 0 && s.size() < 200;
       ++i)
    s.push_back(static_cast<char>(image_[i]));
  return s;
}

std::vector<std::string>
  ExeText::rows(const int dsOffset, const int count, const int stride) const
{
  std::vector<std::string> out;
  for (int i = 0; i < count; ++i)
  {
    auto row = at(dsOffset + i * stride);
    while (!row.empty() && row.back() == ' ')
      row.pop_back();
    out.push_back(std::move(row));
  }
  return out;
}

std::vector<std::string> ExeText::tipMessage(const int index) const
{
  std::vector<std::string> lines;
  const auto entry =
    static_cast<std::size_t>(dataSegment_ + layout_.tipTable + index * 16);
  if (index < 0 || index >= layout_.tipCount || entry + 16 > image_.size())
    return lines;
  for (int i = 0; i < 4; ++i)
  {
    const auto pointer = entry + static_cast<std::size_t>(i) * 4;
    if (std::size_t(word(image_, pointer + 2)) * 16 != dataSegment_)
      return {};
    const int offset = word(image_, pointer);
    auto line = at(offset);
    while (!line.empty() && line.back() == ' ')
      line.pop_back();
    lines.push_back(std::move(line));
  }
  // Trailing empty rows are not part of the message
  while (!lines.empty() && lines.back().empty())
    lines.pop_back();
  return lines;
}

} // namespace duke1
