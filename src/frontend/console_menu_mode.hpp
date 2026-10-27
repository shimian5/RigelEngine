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

#include "data/game_session_data.hpp"
#include "frontend/game_mode.hpp"
#include "ui/console_menu.hpp"
#include "ui/intro_movie.hpp"
#include "ui/menu_navigation.hpp"
#include "ui/options_menu.hpp"

#include <memory>
#include <optional>


namespace rigel
{

/** Console-style replacement for the original DOS main menu */
class ConsoleMenuMode : public GameMode
{
public:
  explicit ConsoleMenuMode(Context context);

  std::unique_ptr<GameMode> updateAndRender(
    engine::TimeDelta dt,
    const std::vector<SDL_Event>& events) override;

private:
  enum class Screen
  {
    Main,
    Episodes,
    Difficulty,
    LoadGame,
    StatsEpisodes,
    Stats,
    Options,
    QuitConfirm,
    Story
  };

  void enter(Screen screen);
  void startStory();
  std::unique_ptr<GameMode> activateSelected();
  std::unique_ptr<GameMode> goBack();
  std::unique_ptr<GameMode> handleEvent(const SDL_Event& event);
  void render();

  Context mContext;
  ui::MenuNavigationHelper mNavigationHelper;
  std::optional<ui::OptionsMenu> mOptionsMenu;

  Screen mScreen = Screen::Main;
  ui::ConsoleList mList;
  int mChosenEpisode = 0;
  std::vector<std::string> mSlotNames; // for the load screen
  std::string mQuitMessage;
  std::optional<data::GameSessionId> mPendingSession;
  std::optional<ui::IntroMovie> mIntroMovie; // plays before the story
};


/** Creates the main menu: console-style or the original, depending on options */
std::unique_ptr<GameMode> createMainMenuMode(GameMode::Context context);

} // namespace rigel
