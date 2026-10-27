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

#include "base/warnings.hpp"
#include "engine/timing.hpp"
#include "frontend/game_mode.hpp"
#include "ui/console_menu.hpp"
#include "ui/menu_navigation.hpp"
#include "ui/options_menu.hpp"

RIGEL_DISABLE_WARNINGS
#include <SDL.h>
RIGEL_RESTORE_WARNINGS

#include <optional>
#include <string>
#include <vector>


namespace rigel::ui
{

/** Console-style pause menu (quick save, load, options...)
 *
 * Drawn with Dear ImGui at native resolution on top of the paused game.
 * The menu itself doesn't perform any saving/loading, it reports what the
 * player selected via takeAction(), which the game runner then executes.
 */
class PauseMenu
{
public:
  enum class Action
  {
    None,
    Resume,
    QuickSave,
    LoadLastSave,
    SaveToSlot,
    LoadFromSlot,
    Quit
  };

  struct Info
  {
    int mEpisode = 1;
    int mLevel = 1;
    int mScore = 0;
    double mPlayTimeSeconds = 0.0;
  };

  explicit PauseMenu(GameMode::Context context);

  /** Whether the event should open the pause menu (Esc, Start, Android back) */
  static bool isOpenTrigger(const SDL_Event& event);

  void open(const Info& info);
  bool isActive() const { return mActive; }

  void handleEvent(const SDL_Event& event);
  void updateAndRender(engine::TimeDelta dt);

  /** Returns the pending action (if any) and clears it */
  Action takeAction(int* pSlotIndex = nullptr);

private:
  enum class Screen
  {
    Main,
    SaveSlots,
    LoadSlots,
    Options,
    Controls,
    QuitConfirm
  };

  void enterScreen(Screen screen);
  void activateSelected();
  void goBack();

  GameMode::Context mContext;
  MenuNavigationHelper mNavigationHelper;
  std::optional<OptionsMenu> mOptionsMenu;

  Screen mScreen = Screen::Main;
  ConsoleList mList;
  Info mInfo;
  std::string mQuitMessage;
  bool mActive = false;
  bool mHasAnySave = false;
  double mElapsed = 0.0;

  Action mAction = Action::None;
  int mActionSlot = 0;
};

} // namespace rigel::ui
