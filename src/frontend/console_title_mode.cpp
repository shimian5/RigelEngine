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

#include "console_title_mode.hpp"

#include "console_menu_mode.hpp"

#include "console_launcher_mode.hpp"
#include "duke1_mode.hpp"
#include "frontend/user_profile.hpp"

#include <string_view>

#include "frontend/game_service_provider.hpp"
#include "ui/console_menu.hpp"
#include "ui/menu_navigation.hpp"


#include <cstdlib>

namespace rigel
{

ConsoleTitleMode::ConsoleTitleMode(Context context)
  : mContext(context)
{
  mContext.mpServiceProvider->playMusic("DUKEIIA.IMF");
}


std::unique_ptr<GameMode> ConsoleTitleMode::updateAndRender(
  const engine::TimeDelta dt,
  const std::vector<SDL_Event>& events)
{
  mElapsed += dt;

#ifdef RIGEL_DEV_SCREENSHOT
  if (const auto* pLauncher = std::getenv("RIGEL_DEV_LAUNCHER"))
  {
    if (std::string_view{pLauncher} == "dn1")
    {
      if (const auto dir = Duke1Mode::findDataDir())
      {
        return std::make_unique<Duke1Mode>(mContext, *dir);
      }
    }

    return std::make_unique<ConsoleLauncherMode>(mContext);
  }
  if (std::getenv("RIGEL_DEV_MENU_KEYS"))
  {
    return createMainMenuMode(mContext);
  }
#endif

  for (const auto& event : events)
  {
    // Any confirm button/key (A, Enter, Space, Start) continues
    const auto startPressed = event.type == SDL_CONTROLLERBUTTONDOWN &&
      event.cbutton.button == SDL_CONTROLLER_BUTTON_START;
    if (ui::isMenuConfirmButton(event) || startPressed)
    {
      return std::make_unique<ConsoleLauncherMode>(mContext);
    }
  }

  ui::drawConsoleTitleScreen("DN COLLECTION", "START", mElapsed);
  return nullptr;
}

} // namespace rigel
