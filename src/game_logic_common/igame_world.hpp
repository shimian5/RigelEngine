/* Copyright (C) 2022, Nikolai Wuttke. All rights reserved.
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
#include "data/bonus.hpp"
#include "engine/timing.hpp"
#include "game_logic_common/input.hpp"

#include <cstdint>
#include <iosfwd>
#include <optional>
#include <set>


namespace rigel::game_logic
{

// Update game logic at 15 FPS. This is not exactly the speed at which the
// game runs on period-appropriate hardware, but it's very close, and it nicely
// fits into 60 FPS, giving us 4 render frames for 1 logic update.
//
// On a 486 with a fast graphics card, the game runs at roughly 15.5 FPS, with
// a slower (non-VLB) graphics card, it's roughly 14 FPS. On a fast 386 (40
// MHz), it's roughly 13 FPS. With 15 FPS, the feel should therefore be very
// close to playing the game on a 486 at the default game speed setting.
constexpr auto GAME_LOGIC_UPDATE_DELAY = 1.0 / 15.0;

constexpr auto BOSS_LEVEL_INTRO_MUSIC = "CALM.IMF";


struct IGameWorld
{
  virtual ~IGameWorld() = default;

  virtual bool levelFinished() const = 0;
  virtual std::set<data::Bonus> achievedBonuses() const = 0;

  /** The bonuses this level offers at all (those with something to find) */
  virtual std::set<data::Bonus> availableBonuses() const
  {
    return {
      data::Bonus::DestroyedAllCameras,
      data::Bonus::NoDamageTaken,
      data::Bonus::CollectedEveryWeapon,
      data::Bonus::CollectedAllMerchandise,
      data::Bonus::DestroyedAllSpinningLaserTurrets,
      data::Bonus::DestroyedAllFireBombs,
      data::Bonus::ShotAllBonusGlobes};
  }

  /** Time spent in the level; negative if not tracked */
  virtual double levelSeconds() const { return -1.0; }
  virtual bool needsPerElementUpscaling() const = 0;
  virtual void updateGameLogic(const PlayerInput& input) = 0;
  virtual void render(float interpolationFactor = 0.0f) = 0;
  virtual void processEndOfFrameActions() = 0;
  virtual void updateBackdropAutoScrolling(engine::TimeDelta dt) = 0;
  virtual bool isPlayerInShip() const = 0;
  virtual void toggleGodMode() = 0;
  virtual bool isGodModeOn() const = 0;
  virtual void activateFullHealthCheat() = 0;
  virtual void activateGiveItemsCheat() = 0;
  virtual void quickSave() = 0;
  virtual void quickLoad() = 0;
  virtual bool canQuickLoad() const = 0;

  // Replay save support: see frontend/replay_save.hpp
  /** The viewport (in tiles) the game logic currently simulates with */
  virtual base::Size simulationViewport() const = 0;
  /** Force a viewport for game logic, e.g. to reproduce a recorded session */
  virtual void
    setSimulationViewportOverride(const std::optional<base::Size>& size) = 0;
  /** Hash of the gameplay-relevant state, for verifying determinism */
  virtual std::uint64_t debugStateHash() const = 0;

  virtual void debugToggleBoundingBoxDisplay() = 0;
  virtual void debugToggleWorldCollisionDataDisplay() = 0;
  virtual void debugToggleGridDisplay() = 0;
  virtual void printDebugText(std::ostream& stream) const = 0;
};

} // namespace rigel::game_logic
