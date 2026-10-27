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

#include "frontend/game_mode.hpp"
#include "ui/console_menu.hpp"
#include "ui/menu_navigation.hpp"
#include "ui/options_menu.hpp"

#include <optional>
#include <string>
#include <vector>


namespace rigel
{

/** Game picker shown after the title screen: Continue / Duke Nukem / Duke Nukem II / Load / Options / Quit */
class ConsoleLauncherMode : public GameMode
{
public:
  explicit ConsoleLauncherMode(Context context);

  std::unique_ptr<GameMode> updateAndRender(
    engine::TimeDelta dt,
    const std::vector<SDL_Event>& events) override;

private:
  enum class Screen
  {
    Games,
    Dn1Menu,
    Dn1Episodes,
    LoadAll,
    Credits,
    QuitConfirm
  };

  void enter(Screen screen);
  std::unique_ptr<GameMode> activateSelected();
  void render();

  struct SaveItem
  {
    bool mIsDn1 = false;
    std::string mSlot;
  };

  std::unique_ptr<GameMode> startSave(const SaveItem& item);
  std::optional<SaveItem> newestSave() const;

  std::vector<SaveItem> mSaveItems;
  std::vector<int> mDn1Episodes;
  bool mDn1DataAvailable = false;
  Context mContext;
  ui::MenuNavigationHelper mNavigationHelper;
  std::optional<ui::OptionsMenu> mOptionsMenu;
  Screen mScreen = Screen::Games;
  ui::ConsoleList mList;
  std::string mQuitMessage;
};

} // namespace rigel
