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

#include "replay_save.hpp"

#include "frontend/user_profile.hpp"

#include <loguru.hpp>

#ifdef __ANDROID__
  #include <SDL_log.h>
#endif

#include <chrono>
#include <fstream>
#include <iterator>
#include <limits>


namespace rigel
{

namespace
{

constexpr char MAGIC[4] = {'R', 'G', 'L', 'S'};


void putU16(std::vector<std::uint8_t>& out, const std::uint16_t v)
{
  out.push_back(std::uint8_t(v & 0xFF));
  out.push_back(std::uint8_t(v >> 8));
}


void putU32(std::vector<std::uint8_t>& out, const std::uint32_t v)
{
  for (auto i = 0; i < 4; ++i)
  {
    out.push_back(std::uint8_t((v >> (8 * i)) & 0xFF));
  }
}


struct Reader
{
  const std::uint8_t* mpData;
  const std::uint8_t* mpEnd;

  bool u8(std::uint8_t& v)
  {
    if (mpData >= mpEnd)
    {
      return false;
    }
    v = *mpData++;
    return true;
  }

  bool u16(std::uint16_t& v)
  {
    std::uint8_t lo, hi;
    if (!u8(lo) || !u8(hi))
    {
      return false;
    }
    v = std::uint16_t(lo | (hi << 8));
    return true;
  }

  bool u32(std::uint32_t& v)
  {
    v = 0;
    for (auto i = 0; i < 4; ++i)
    {
      std::uint8_t b;
      if (!u8(b))
      {
        return false;
      }
      v |= std::uint32_t(b) << (8 * i);
    }
    return true;
  }
};

} // namespace


std::uint16_t packPlayerInput(const game_logic::PlayerInput& input)
{
  std::uint16_t v = 0;
  auto set = [&v](const int bit, const bool value) {
    v = std::uint16_t(v | (value ? (1u << bit) : 0u));
  };

  set(0, input.mLeft);
  set(1, input.mRight);
  set(2, input.mUp);
  set(3, input.mDown);
  set(4, input.mInteract.mIsPressed);
  set(5, input.mInteract.mWasTriggered);
  set(6, input.mJump.mIsPressed);
  set(7, input.mJump.mWasTriggered);
  set(8, input.mFire.mIsPressed);
  set(9, input.mFire.mWasTriggered);
  return v;
}


game_logic::PlayerInput unpackPlayerInput(const std::uint16_t v)
{
  auto get = [v](const int bit) {
    return (v & (1u << bit)) != 0;
  };

  game_logic::PlayerInput input;
  input.mLeft = get(0);
  input.mRight = get(1);
  input.mUp = get(2);
  input.mDown = get(3);
  input.mInteract = {get(4), get(5)};
  input.mJump = {get(6), get(7)};
  input.mFire = {get(8), get(9)};
  return input;
}


std::vector<std::uint8_t> serializeReplaySave(const ReplaySave& save)
{
  std::vector<std::uint8_t> out;
  out.insert(out.end(), std::begin(MAGIC), std::end(MAGIC));
  putU16(out, ReplaySave::CURRENT_VERSION);
  out.push_back(save.mGameplayStyle);
  out.push_back(std::uint8_t(
    (save.mShowWelcomeMessage ? 1 : 0) | (save.mSessionId.mIsDemo ? 2 : 0) |
    (save.mPlayerPositionOverride ? 4 : 0)));
  putU32(out, std::uint32_t(save.mSessionId.mEpisode));
  putU32(out, std::uint32_t(save.mSessionId.mLevel));
  putU32(out, std::uint32_t(save.mSessionId.mDifficulty));
  putU32(out, std::uint32_t(save.mSimulationViewport.width));
  putU32(out, std::uint32_t(save.mSimulationViewport.height));
  putU32(out, std::uint32_t(save.mScore));
  putU32(out, std::uint32_t(save.mHealth));
  putU32(out, std::uint32_t(save.mStateHash & 0xFFFFFFFFu));
  putU32(out, std::uint32_t(save.mStateHash >> 32));
  if (save.mPlayerPositionOverride)
  {
    putU32(out, std::uint32_t(save.mPlayerPositionOverride->x));
    putU32(out, std::uint32_t(save.mPlayerPositionOverride->y));
  }

  save.mPlayerStateAtStart.serialize(out);

  // Input log, run-length encoded as (count, value) pairs
  std::vector<std::pair<std::uint32_t, std::uint16_t>> runs;
  for (const auto value : save.mInputs)
  {
    if (
      !runs.empty() && runs.back().second == value &&
      runs.back().first < 0xFFFF)
    {
      ++runs.back().first;
    }
    else
    {
      runs.emplace_back(1u, value);
    }
  }

  putU32(out, std::uint32_t(save.mInputs.size()));
  putU32(out, std::uint32_t(runs.size()));
  for (const auto& [count, value] : runs)
  {
    putU16(out, std::uint16_t(count));
    putU16(out, value);
  }

  return out;
}


std::optional<ReplaySave>
  deserializeReplaySave(const std::vector<std::uint8_t>& bytes)
{
  Reader r{bytes.data(), bytes.data() + bytes.size()};

  for (const auto expected : MAGIC)
  {
    std::uint8_t b;
    if (!r.u8(b) || b != std::uint8_t(expected))
    {
      return std::nullopt;
    }
  }

  std::uint16_t version;
  std::uint8_t style, flags;
  std::uint32_t episode, level, difficulty, vpW, vpH, score, health, hashLo,
    hashHi;
  if (
    !r.u16(version) || version != ReplaySave::CURRENT_VERSION || !r.u8(style) ||
    !r.u8(flags) || !r.u32(episode) || !r.u32(level) || !r.u32(difficulty) ||
    !r.u32(vpW) || !r.u32(vpH) || !r.u32(score) || !r.u32(health) ||
    !r.u32(hashLo) || !r.u32(hashHi))
  {
    return std::nullopt;
  }

  ReplaySave save;
  save.mGameplayStyle = style;
  save.mShowWelcomeMessage = (flags & 1) != 0;
  save.mSessionId = data::GameSessionId{
    int(episode),
    int(level),
    static_cast<data::Difficulty>(difficulty),
    (flags & 2) != 0};
  save.mSimulationViewport = {int(vpW), int(vpH)};
  save.mScore = int(score);
  save.mHealth = int(health);
  save.mStateHash = std::uint64_t(hashLo) | (std::uint64_t(hashHi) << 32);

  if (flags & 4)
  {
    std::uint32_t x, y;
    if (!r.u32(x) || !r.u32(y))
    {
      return std::nullopt;
    }
    save.mPlayerPositionOverride = base::Vec2{int(x), int(y)};
  }

  auto oState = data::PersistentPlayerState::deserialize(r.mpData, r.mpEnd);
  if (!oState)
  {
    return std::nullopt;
  }
  save.mPlayerStateAtStart = *oState;

  std::uint32_t numInputs, numRuns;
  if (!r.u32(numInputs) || !r.u32(numRuns) || numInputs > 50'000'000)
  {
    return std::nullopt;
  }

  save.mInputs.reserve(numInputs);
  for (auto i = 0u; i < numRuns; ++i)
  {
    std::uint16_t count, value;
    if (!r.u16(count) || !r.u16(value))
    {
      return std::nullopt;
    }
    save.mInputs.insert(save.mInputs.end(), count, value);
  }

  if (save.mInputs.size() != numInputs)
  {
    return std::nullopt;
  }

  return save;
}


std::optional<std::filesystem::path> saveFilePath(const std::string& slotName)
{
  const auto oPrefs = createOrGetPreferencesPath();
  if (!oPrefs)
  {
    return std::nullopt;
  }

  const auto dir = *oPrefs / "saves";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  if (ec)
  {
    return std::nullopt;
  }

  return dir / (slotName + ".dn2");
}


bool writeReplaySaveFile(const std::string& slotName, const ReplaySave& save)
{
  const auto oPath = saveFilePath(slotName);
  if (!oPath)
  {
    return false;
  }

  const auto bytes = serializeReplaySave(save);

  // Write to a temporary file first, so that a crash/kill while saving can't
  // destroy the previous save
  auto tmpPath = *oPath;
  tmpPath += ".tmp";
  {
    std::ofstream file{tmpPath, std::ios::binary | std::ios::trunc};
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
  std::filesystem::rename(tmpPath, *oPath, ec);
  if (ec)
  {
    LOG_F(WARNING, "Failed to write save file %s", oPath->u8string().c_str());
    return false;
  }

  LOG_F(
    INFO, "Wrote save %s (%zu bytes)", oPath->u8string().c_str(), bytes.size());
#ifdef __ANDROID__
  SDL_Log(
    "RIGEL save written: %s (%zu bytes)",
    oPath->u8string().c_str(),
    bytes.size());
#endif
  return true;
}


std::optional<ReplaySave> readReplaySaveFile(const std::string& slotName)
{
  const auto oPath = saveFilePath(slotName);
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
  return deserializeReplaySave(bytes);
}


std::string consoleSlotName(const int index)
{
  return "slot" + std::to_string(index + 1);
}


std::optional<SaveSummary> readSaveSummary(const std::string& slotName)
{
  const auto oPath = saveFilePath(slotName);
  if (!oPath)
  {
    return std::nullopt;
  }

  const auto oSave = readReplaySaveFile(slotName);
  if (!oSave)
  {
    return std::nullopt;
  }

  SaveSummary summary;
  summary.mEpisode = oSave->mSessionId.mEpisode + 1;
  summary.mLevel = oSave->mSessionId.mLevel + 1;
  summary.mScore = oSave->mScore;
  summary.mHealth = oSave->mHealth;
  summary.mPlayTimeSeconds = double(oSave->mInputs.size()) / 15.0;

  std::error_code ec;
  const auto time = std::filesystem::last_write_time(*oPath, ec);
  if (!ec)
  {
    // Only used for ordering saves, so the raw tick count is sufficient (the
    // epoch of the file clock is implementation defined)
    summary.mModifiedTime = std::int64_t(time.time_since_epoch().count());
  }

  return summary;
}


std::optional<std::string> newestSaveSlotName()
{
  std::vector<std::string> names = {"quicksave", "autosave"};
  for (auto i = 0; i < NUM_SAVE_SLOTS_CONSOLE; ++i)
  {
    names.push_back(consoleSlotName(i));
  }

  std::optional<std::string> best;
  std::int64_t bestTime = std::numeric_limits<std::int64_t>::min();
  for (const auto& name : names)
  {
    if (const auto oSummary = readSaveSummary(name))
    {
      if (!best || oSummary->mModifiedTime > bestTime)
      {
        bestTime = oSummary->mModifiedTime;
        best = name;
      }
    }
  }

  return best;
}

} // namespace rigel
