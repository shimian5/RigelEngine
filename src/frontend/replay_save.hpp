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

#include "base/spatial_types.hpp"
#include "data/game_session_data.hpp"
#include "data/player_model.hpp"
#include "game_logic_common/input.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>


namespace rigel
{

/** A saved game that works by replaying the player's input
 *
 * Game logic is deterministic, so instead of serializing the entire world, a
 * save consists of the parameters the level was started with, plus the input
 * of every logic update since. Loading re-creates the level and fast-forwards
 * through the recorded input.
 */
struct ReplaySave
{
  static constexpr std::uint16_t CURRENT_VERSION = 1;

  data::GameSessionId mSessionId;
  data::PersistentPlayerState mPlayerStateAtStart;
  std::optional<base::Vec2> mPlayerPositionOverride;
  bool mShowWelcomeMessage = false;
  std::uint8_t mGameplayStyle = 0;

  // The viewport (in tiles) the simulation used while recording. Needed to
  // reproduce which enemies/objects were considered active.
  base::Size mSimulationViewport;

  std::vector<std::uint16_t> mInputs;

  // Summary for display in menus, not needed for loading
  int mScore = 0;
  int mHealth = 0;

  // Hash of the world state at the time of saving, used to verify that
  // replaying reproduced the same state. 0 if unknown.
  std::uint64_t mStateHash = 0;
};


std::uint16_t packPlayerInput(const game_logic::PlayerInput& input);
game_logic::PlayerInput unpackPlayerInput(std::uint16_t packed);

std::vector<std::uint8_t> serializeReplaySave(const ReplaySave& save);
std::optional<ReplaySave>
  deserializeReplaySave(const std::vector<std::uint8_t>& bytes);

/** Where save files live, e.g. <prefs>/saves/duke2/quicksave.dn2 */
std::optional<std::filesystem::path> saveFilePath(const std::string& slotName);

/** The picture that goes with a save slot: the same name with a .png extension
 */
std::optional<std::filesystem::path>
  thumbnailFilePath(const std::string& slotName);

class IGameServiceProvider;

/** Takes a picture of the frame being shown and stores it for the slot. Call it
 * where the save is written, while no menu is on screen. */
void saveThumbnail(
  IGameServiceProvider* pServices,
  const std::string& slotName);

/** Writes the bytes of a save into the slot's file. The previous save survives
 * a crash while writing. Used by both games' saves. */
bool writeSaveBytes(
  const std::string& slotName,
  const std::vector<std::uint8_t>& bytes);

/** The slot among the given ones whose file was written last */
std::optional<std::string>
  newestSaveAmong(const std::vector<std::string>& slotNames);

bool writeReplaySaveFile(const std::string& slotName, const ReplaySave& save);

constexpr int NUM_SAVE_SLOTS_CONSOLE = 8;
std::string consoleSlotName(int index); // index 0..7 -> "slot1".."slot8"

/** Short description of a save, for menus */
struct SaveSummary
{
  int mEpisode = 0;
  int mLevel = 0;
  int mScore = 0;
  int mHealth = 0;
  double mPlayTimeSeconds = 0.0;
  std::int64_t mModifiedTime = 0; // opaque, only for ordering saves
};

std::optional<SaveSummary> readSaveSummary(const std::string& slotName);

/** Name of the most recently written save (quick, auto or numbered slot) */
std::optional<std::string> newestSaveSlotName();
std::optional<ReplaySave> readReplaySaveFile(const std::string& slotName);

} // namespace rigel
