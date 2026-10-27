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

#include "ui/console_results.hpp"
#include "duke1/core.hpp"
#include "duke1/play.hpp"
#include "frontend/duke1_sound.hpp"
#include "frontend/duke1_story.hpp"
#include "frontend/game_mode.hpp"
#include "renderer/texture.hpp"
#include "frontend/duke1_saves.hpp"
#include "ui/console_menu.hpp"
#include "ui/hud_skin.hpp"
#include "ui/menu_navigation.hpp"

#include <array>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>


namespace rigel
{

/** Duke Nukem 1, played from the user's original DOS data files.
 *
 * Independent from the Duke II game logic: uses the duke1 library for map
 * rendering and the deterministic player core.
 */
class Duke1Mode : public GameMode
{
public:
  Duke1Mode(
    Context context,
    const std::filesystem::path& dataDir,
    std::optional<std::string> loadSlot = std::nullopt);

  std::unique_ptr<GameMode> updateAndRender(
    engine::TimeDelta dt,
    const std::vector<SDL_Event>& events) override;

  /** The Duke II game path in effect (set once at startup) */
  static void setGamePath(const std::filesystem::path& gamePath);

  /** Where the DOS data lives relative to the game path, if present */
  static std::optional<std::filesystem::path> findDataDir();

  /** Human readable reason for the most recent findDataDir() failure */
  static const std::string& lastLookupProblem();

private:
  void handleEvent(const SDL_Event& event);
  void submitOverlayHud();

  Context mContext;
  std::filesystem::path mDataDir;
  std::unique_ptr<duke1::PlayWorld> mpWorld;
  std::unique_ptr<Duke1Speaker> mpSpeaker;
  std::optional<duke1::ExeText> mText;
  std::unique_ptr<Duke1Screens> mpScreens;
  std::vector<std::vector<std::uint8_t>> mPages; // story/message pictures being shown, 320x200 RGBA
  std::size_t mPageIndex = 0;
  bool showPages(std::vector<std::vector<std::uint8_t>> pages);
  void drawPicture(const std::vector<std::uint8_t>& rgba);
  std::size_t mLevelIndex = 0;
  void startLevel(std::size_t index, const duke1::PlayWorld::Carry* carry);
  duke1::TickGate mGate;
  duke1::Input mInput;
  double mTimerRemainder = 0.0;
  double mSinceUpdate = 0.0;
  double mUpdateInterval = 0.0625;
  double mCamX = 0.0;
  double mCamY = 0.0;
  double mPrevCamX = 0.0;
  double mPrevCamY = 0.0;
  bool mCamStepPending = false;
  bool mCamInitialized = false;
  bool mFadeInPending = false;

  // Pause menu and saves
  enum class PauseScreen
  {
    None,
    Main,
    SaveSlots,
    LoadSlots,
    Stats,
    Debug,
    DebugLevels,
    DebugFeatures
  };
  PauseScreen mPause = PauseScreen::None;
  ui::ConsoleList mPauseList;
  ui::MenuNavigationHelper mNavigation;
  std::vector<std::uint8_t> mInputLog;
  duke1::PlayWorld::Carry mLevelStartCarry;
  bool mPauseRequested = false;
  bool mDebugUsed = false;
  bool mWide = false; // simulation setting: wide active window (saved with the game)
  bool mStartJinglePending = false;
  duke1::PlayWorld::Carry mStatsCarry;
  std::unique_ptr<ui::ConsoleResultsScreen> mpResults;
  bool mAutosaveOnStart = true;
  bool mBackHeld = false;
  bool mQuickSaveRequested = false;
  bool mQuickLoadRequested = false;
  std::string mToast;
  double mToastSeconds = 0.0;

  void enterPauseScreen(PauseScreen screen);
  void activatePauseSelection();
  bool saveTo(const std::string& slot);
  bool loadFrom(const std::string& slot);
  void drawPauseMenu();
  static std::string levelLabel(std::size_t index);
  std::array<std::shared_ptr<renderer::Texture>, 8> mKeyIcons;
  std::optional<ui::HudSkin> mHudSkin;
  std::optional<ui::HudSkin> mGeneratedHudSkin;
  std::optional<ui::HudTier> mHudSkinTierTried;
  bool mQuitRequested = false;
  bool mDpad[4] = {};
  float mStickX = 0.0f;
  float mStickY = 0.0f;
};

} // namespace rigel
