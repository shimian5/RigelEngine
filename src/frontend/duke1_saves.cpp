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


#include "duke1_saves.hpp"

#include "frontend/replay_save.hpp"

#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>


namespace rigel
{

namespace
{

constexpr std::array<char, 4> MAGIC = {'D', 'N', '1', 'S'};
constexpr std::uint8_t VERSION =
  3; // bump whenever the simulation changes in a way that alters replays

void put32(std::vector<std::uint8_t>& out, std::int32_t v)
{
  for (int i = 0; i < 4; ++i)
  {
    out.push_back(static_cast<std::uint8_t>(
      (static_cast<std::uint32_t>(v) >> (8 * i)) & 0xFF));
  }
}

std::int32_t get32(const std::vector<std::uint8_t>& in, std::size_t pos)
{
  std::uint32_t v = 0;
  for (int i = 0; i < 4; ++i)
  {
    v |= static_cast<std::uint32_t>(in[pos + std::size_t(i)]) << (8 * i);
  }
  return static_cast<std::int32_t>(v);
}

} // namespace


std::uint8_t packDuke1Input(const duke1::Input& i)
{
  return static_cast<std::uint8_t>(
    (i.left ? 1 : 0) | (i.right ? 2 : 0) | (i.up ? 4 : 0) | (i.down ? 8 : 0) |
    (i.jump ? 16 : 0) | (i.fire ? 32 : 0));
}


duke1::Input unpackDuke1Input(const std::uint8_t p)
{
  duke1::Input i;
  i.left = p & 1;
  i.right = p & 2;
  i.up = p & 4;
  i.down = p & 8;
  i.jump = p & 16;
  i.fire = p & 32;
  return i;
}


std::string duke1SlotName(const int index)
{
  return "dn1_slot" + std::to_string(index + 1);
}


std::vector<std::uint8_t> serializeDuke1Save(const Duke1Save& save)
{
  std::vector<std::uint8_t> bytes(MAGIC.begin(), MAGIC.end());
  bytes.push_back(VERSION);
  put32(bytes, save.mLevelIndex);
  put32(bytes, save.mEpisode);
  put32(bytes, save.mCarry.score);
  put32(bytes, save.mCarry.health);
  put32(bytes, save.mCarry.boost);
  put32(bytes, save.mCarry.claw);
  put32(bytes, save.mCarry.capacity);
  put32(bytes, save.mWide ? 1 : 0);
  put32(bytes, static_cast<std::int32_t>(save.mInputs.size()));
  bytes.insert(bytes.end(), save.mInputs.begin(), save.mInputs.end());

  return bytes;
}

bool writeDuke1Save(const std::string& slot, const Duke1Save& save)
{
  const auto oPath = saveFilePath(slot);
  if (!oPath)
  {
    return false;
  }

  const auto bytes = serializeDuke1Save(save);

  auto tmp = *oPath;
  tmp += ".tmp";
  {
    std::ofstream file{tmp, std::ios::binary | std::ios::trunc};
    if (!file)
    {
      return false;
    }
    file.write(
      reinterpret_cast<const char*>(bytes.data()),
      std::streamsize(bytes.size()));
    if (!file)
    {
      return false;
    }
  }

  std::error_code ec;
  std::filesystem::rename(tmp, *oPath, ec);
  return !ec;
}


std::optional<Duke1Save> readDuke1Save(const std::string& slot)
{
  const auto oPath = saveFilePath(slot);
  if (!oPath)
  {
    return std::nullopt;
  }

  std::ifstream file{*oPath, std::ios::binary};
  if (!file)
  {
    return std::nullopt;
  }
  const std::vector<std::uint8_t> bytes{
    std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};

  return deserializeDuke1Save(bytes);
}

std::optional<Duke1Save>
  deserializeDuke1Save(const std::vector<std::uint8_t>& bytes)
{
  constexpr std::size_t LEGACY_HEADER = 5 + 8 * 4;
  if (
    bytes.size() < LEGACY_HEADER ||
    !std::equal(MAGIC.begin(), MAGIC.end(), bytes.begin()) ||
    (bytes[4] != 2 && bytes[4] != VERSION))
  {
    return std::nullopt;
  }

  const std::size_t shift = bytes[4] >= 3 ? 4 : 0;
  const std::size_t HEADER = LEGACY_HEADER + shift;
  if (bytes.size() < HEADER)
    return std::nullopt;
  Duke1Save save;
  save.mLevelIndex = get32(bytes, 5);
  save.mEpisode = shift ? get32(bytes, 9) : 0;
  save.mCarry.score = get32(bytes, 9 + shift);
  save.mCarry.health = get32(bytes, 13 + shift);
  save.mCarry.boost = get32(bytes, 17 + shift);
  save.mCarry.claw = get32(bytes, 21 + shift);
  save.mCarry.capacity = get32(bytes, 25 + shift);
  save.mWide = get32(bytes, 29 + shift) != 0;
  const auto rawCount = get32(bytes, 33 + shift);
  if (rawCount < 0 || std::size_t(rawCount) > bytes.size() - HEADER)
  {
    return std::nullopt;
  }
  const auto count = std::size_t(rawCount);
  const auto& carry = save.mCarry;
  if (
    save.mEpisode < 0 || save.mEpisode > 2 || save.mLevelIndex < 0 ||
    carry.score < 0 || carry.health < 0 || carry.health > 9 ||
    carry.boost < 0 || carry.boost > 1 || carry.claw < 0 || carry.claw > 1 ||
    carry.capacity < 1 || carry.capacity > 4)
  {
    return std::nullopt;
  }
  save.mInputs.assign(
    bytes.begin() + HEADER, bytes.begin() + HEADER + std::ptrdiff_t(count));
  return save;
}


std::optional<std::string> newestDuke1Save()
{
  std::optional<std::string> newest;
  std::filesystem::file_time_type newestTime{};
  std::vector<std::string> names = {"dn1_quicksave", "dn1_autosave"};
  for (int i = 0; i < NUM_DUKE1_SLOTS; ++i)
  {
    names.push_back(duke1SlotName(i));
  }

  for (const auto& name : names)
  {
    const auto oPath = saveFilePath(name);
    std::error_code ec;
    if (!oPath || !std::filesystem::exists(*oPath, ec))
    {
      continue;
    }
    const auto time = std::filesystem::last_write_time(*oPath, ec);
    if (
      !ec &&
      (!newest ||
       time.time_since_epoch().count() > newestTime.time_since_epoch().count()))
    {
      newest = name;
      newestTime = time;
    }
  }
  return newest;
}

} // namespace rigel
