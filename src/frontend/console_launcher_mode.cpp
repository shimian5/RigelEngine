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


#include "console_launcher_mode.hpp"

#include "console_menu_mode.hpp"
#include "duke1_mode.hpp"

#include "data/game_session_data.hpp"
#include "duke1_saves.hpp"
#include "frontend/game_service_provider.hpp"
#include "frontend/replay_save.hpp"
#include "frontend/user_profile.hpp"
#include "game_session_mode.hpp"
#include "sdl_utils/platform.hpp"

#include <filesystem>


namespace rigel
{

namespace
{

std::vector<std::string> dn2SlotNames()
{
  std::vector<std::string> names = {"quicksave", "autosave"};
  for (int i = 0; i < NUM_SAVE_SLOTS_CONSOLE; ++i)
  {
    names.push_back(consoleSlotName(i));
  }
  return names;
}


std::vector<std::string> dn1SlotNames()
{
  std::vector<std::string> names = {"dn1_quicksave", "dn1_autosave"};
  for (int i = 0; i < NUM_DUKE1_SLOTS; ++i)
  {
    names.push_back(duke1SlotName(i));
  }
  return names;
}


std::string slotLabel(const std::string& slot)
{
  if (slot == "quicksave" || slot == "dn1_quicksave")
  {
    return "QUICK SAVE";
  }
  if (slot == "autosave" || slot == "dn1_autosave")
  {
    return "AUTOSAVE";
  }
  return "SLOT " + slot.substr(slot.size() - 1);
}


std::string describeDn1Save(const std::string& slot)
{
  const auto oSave = readDuke1Save(slot);
  if (!oSave)
  {
    return {};
  }
  return "DN1   " + slotLabel(slot) + "   E" +
    std::to_string(oSave->mEpisode + 1) + " LEVEL " +
    std::to_string(oSave->mLevelIndex + 1) + "   " +
    ui::formatScoreWithCommas(oSave->mCarry.score);
}


std::string describeDn2Save(const std::string& slot)
{
  const auto oSummary = readSaveSummary(slot);
  if (!oSummary)
  {
    return {};
  }
  return "DN2   " + slotLabel(slot) + "   E" +
    std::to_string(oSummary->mEpisode) + " L" +
    std::to_string(oSummary->mLevel) + "   " +
    ui::formatScoreWithCommas(oSummary->mScore);
}

} // namespace


ConsoleLauncherMode::ConsoleLauncherMode(Context context)
  : mContext(context)
{
  if (duke2Available())
  {
    mContext.mpServiceProvider->playMusic("DUKEIIA.IMF");
  }
  enter(Screen::Games);
}


std::optional<ConsoleLauncherMode::SaveItem>
  ConsoleLauncherMode::newestSave() const
{
  std::optional<SaveItem> newest;
  std::filesystem::file_time_type newestTime{};

  auto consider = [&](const bool isDn1, const std::string& slot) {
    const auto oPath = saveFilePath(slot);
    std::error_code ec;
    if (!oPath || !std::filesystem::exists(*oPath, ec))
    {
      return;
    }
    if (
      isDn1 ? !readDuke1Save(slot).has_value()
            : !readSaveSummary(slot).has_value())
    {
      return;
    }
    const auto time = std::filesystem::last_write_time(*oPath, ec);
    if (!ec && (!newest || time > newestTime))
    {
      newest = SaveItem{isDn1, slot};
      newestTime = time;
    }
  };

  if (Duke1Mode::findDataDir())
  {
    for (const auto& slot : dn1SlotNames())
    {
      consider(true, slot);
    }
  }
  if (duke2Available())
  {
    for (const auto& slot : dn2SlotNames())
    {
      consider(false, slot);
    }
  }
  return newest;
}


std::unique_ptr<GameMode> ConsoleLauncherMode::startSave(const SaveItem& item)
{
  if (item.mIsDn1)
  {
    if (const auto dataDir = Duke1Mode::findDataDir())
    {
      mContext.mpServiceProvider->fadeOutScreen();
      return std::make_unique<Duke1Mode>(mContext, *dataDir, item.mSlot);
    }
    return nullptr;
  }

  if (const auto oSave = readReplaySaveFile(item.mSlot))
  {
    const auto locked = mContext.mpServiceProvider->isSharewareVersion() &&
      oSave->mSessionId.needsRegisteredVersion();
    if (!locked)
    {
      mContext.mpServiceProvider->fadeOutScreen();
      return std::make_unique<GameSessionMode>(*oSave, mContext);
    }
  }
  return nullptr;
}


void ConsoleLauncherMode::enter(const Screen screen)
{
  mScreen = screen;

  switch (screen)
  {
    case Screen::Games:
      mDn1DataAvailable = Duke1Mode::findDataDir().has_value();
      // Duke Nukem 1 is only available when its original data files are found
      mList.setItems(
        {{"CONTINUE", newestSave().has_value()},
         {"DUKE NUKEM", mDn1DataAvailable},
         {"DUKE NUKEM II", duke2Available()},
         {"LOAD GAME", newestSave().has_value()},
         {"OPTIONS", true},
#ifdef __ANDROID__
         {"GAME FILES", true},
#endif
         {"CREDITS", true},
         {"QUIT", true}});
      if (!mList.selectedIsEnabled())
      {
        mList.move(1);
      }
      break;

    case Screen::Dn1Menu:
      {
        bool anySave = false;
        for (const auto& slot : dn1SlotNames())
        {
          anySave = anySave || readDuke1Save(slot).has_value();
        }
        mList.setItems(
          {{"NEW GAME", true},
           {"CONTINUE", anySave},
           {"LOAD GAME", anySave},
           {"BACK", true}});
        break;
      }

    case Screen::Dn1Episodes:
      {
        const auto dir = Duke1Mode::findDataDir();
        mDn1Episodes =
          dir ? Duke1Mode::availableEpisodes(*dir) : std::vector<int>{};
        std::vector<ui::ConsoleMenuItem> items;
        for (const auto episode : mDn1Episodes)
          items.push_back({"EPISODE " + std::to_string(episode + 1), true});
        items.push_back({"BACK", true});
        mList.setItems(std::move(items));
        break;
      }

    case Screen::Dn1Levels:
      {
        const auto dir = Duke1Mode::findDataDir();
        const auto episode =
          dir ? duke1::ExeText::load(*dir, mDn1Episode) : std::nullopt;
        mDn1LevelCount = episode ? episode->levels().size() : 0;
        std::vector<ui::ConsoleMenuItem> items;
        for (std::size_t i = 0; i < mDn1LevelCount; ++i)
        {
          items.push_back({"LEVEL " + std::to_string(i + 1), true});
        }
        items.push_back({"BACK", true});
        mList.setItems(std::move(items));
        break;
      }

    case Screen::LoadAll:
      {
        // Every save of both games, newest-first is not needed: grouped by
        // game, quick/auto first
        mSaveItems.clear();
        std::vector<ui::ConsoleMenuItem> items;
        if (Duke1Mode::findDataDir())
        {
          for (const auto& slot : dn1SlotNames())
          {
            const auto text = describeDn1Save(slot);
            if (!text.empty())
            {
              mSaveItems.push_back({true, slot});
              items.push_back({text, true});
            }
          }
        }
        for (const auto& slot :
             duke2Available() ? dn2SlotNames() : std::vector<std::string>{})
        {
          const auto text = describeDn2Save(slot);
          if (!text.empty())
          {
            mSaveItems.push_back({false, slot});
            items.push_back({text, true});
          }
        }
        if (items.empty())
        {
          items.push_back({"NO SAVED GAMES", false});
        }
        mList.setItems(std::move(items));
        break;
      }

    case Screen::Credits:
      mList.setItems({{"BACK", true}});
      break;

    case Screen::QuitConfirm:
      mQuitMessage = ui::randomQuitMessage();
      mList.setItems({{"NO, KEEP PLAYING", true}, {"YES, QUIT", true}});
      break;
  }
}


std::unique_ptr<GameMode> ConsoleLauncherMode::activateSelected()
{
  if (!mList.selectedIsEnabled())
  {
    return nullptr;
  }

  const auto selected = mList.selected();
  if (mScreen == Screen::Credits)
  {
    enter(Screen::Games);
    return nullptr;
  }

  if (mScreen == Screen::QuitConfirm)
  {
    if (selected == 1)
    {
      mContext.mpServiceProvider->scheduleGameQuit();
    }
    else
    {
      enter(Screen::Games);
    }
    return nullptr;
  }

  if (mScreen == Screen::LoadAll)
  {
    if (std::size_t(selected) < mSaveItems.size())
    {
      return startSave(mSaveItems[std::size_t(selected)]);
    }
    return nullptr;
  }

  if (mScreen == Screen::Dn1Episodes)
  {
    if (std::size_t(selected) >= mDn1Episodes.size())
    {
      enter(Screen::Dn1Menu);
      return nullptr;
    }
    mDn1Episode = mDn1Episodes[std::size_t(selected)];
    enter(Screen::Dn1Levels);
    return nullptr;
  }

  if (mScreen == Screen::Dn1Levels)
  {
    if (std::size_t(selected) >= mDn1LevelCount)
    {
      enter(mDn1Episodes.size() > 1 ? Screen::Dn1Episodes : Screen::Dn1Menu);
      return nullptr;
    }
    if (const auto dataDir = Duke1Mode::findDataDir())
    {
      mContext.mpServiceProvider->fadeOutScreen();
      return std::make_unique<Duke1Mode>(
        mContext, *dataDir, std::nullopt, mDn1Episode, std::size_t(selected));
    }
    return nullptr;
  }

  if (mScreen == Screen::Dn1Menu)
  {
    const auto dataDir = Duke1Mode::findDataDir();
    switch (selected)
    {
      case 0:
        if (dataDir)
        {
          mDn1Episodes = Duke1Mode::availableEpisodes(*dataDir);
          if (mDn1Episodes.size() == 1)
          {
            mDn1Episode = mDn1Episodes.front();
            enter(Screen::Dn1Levels);
          }
          else
          {
            enter(Screen::Dn1Episodes);
          }
        }
        break;
      case 1:
        if (const auto name = newestDuke1Save())
        {
          return startSave({true, *name});
        }
        break;
      case 2:
        // Only DN1 saves: reuse the shared list, filtered
        enter(Screen::LoadAll);
        {
          std::vector<SaveItem> kept;
          std::vector<ui::ConsoleMenuItem> items;
          for (const auto& item : mSaveItems)
          {
            if (item.mIsDn1)
            {
              kept.push_back(item);
              items.push_back({describeDn1Save(item.mSlot), true});
            }
          }
          mSaveItems = std::move(kept);
          mList.setItems(std::move(items));
        }
        break;
      case 3:
        enter(Screen::Games);
        break;
    }
    return nullptr;
  }

  switch (selected)
  {
    case 0:
      if (const auto newest = newestSave())
      {
        return startSave(*newest);
      }
      break;

    case 1:
      if (Duke1Mode::findDataDir())
      {
        enter(Screen::Dn1Menu);
      }
      break;

    case 2:
      return createMainMenuMode(mContext);

    case 3:
      enter(Screen::LoadAll);
      break;

    case 4:
      mOptionsMenu.emplace(
        mContext.mpUserProfile,
        mContext.mpServiceProvider,
        mContext.mpRenderer,
        ui::OptionsMenu::Type::Main);
      break;

#ifdef __ANDROID__
    case 5:
      sdl_utils::openAndroidGameDataSetup();
      break;

    case 6:
#else
    case 5:
#endif
      enter(Screen::Credits);
      break;

#ifdef __ANDROID__
    case 7:
#else
    case 6:
#endif
      enter(Screen::QuitConfirm);
      break;
  }

  return nullptr;
}


std::unique_ptr<GameMode> ConsoleLauncherMode::updateAndRender(
  const engine::TimeDelta dt,
  const std::vector<SDL_Event>& events)
{
  for (const auto& event : events)
  {
    if (mOptionsMenu)
    {
      mOptionsMenu->handleEvent(event);
      continue;
    }

    switch (mNavigationHelper.convert(event))
    {
      case ui::NavigationEvent::NavigateUp:
        mList.move(-1);
        break;
      case ui::NavigationEvent::NavigateDown:
        mList.move(1);
        break;
      case ui::NavigationEvent::Confirm:
        if (auto next = activateSelected())
        {
          return next;
        }
        break;
      case ui::NavigationEvent::Cancel:
        if (mScreen != Screen::Games)
        {
          if (mScreen == Screen::Dn1Levels)
          {
            enter(
              mDn1Episodes.size() > 1 ? Screen::Dn1Episodes : Screen::Dn1Menu);
          }
          else
          {
            enter(
              mScreen == Screen::Dn1Episodes ? Screen::Dn1Menu : Screen::Games);
          }
        }
        break;
      default:
        break;
    }
  }

  if (mOptionsMenu)
  {
    mOptionsMenu->updateAndRender(dt);
    if (mOptionsMenu->isFinished())
    {
      mOptionsMenu.reset();
    }
    return nullptr;
  }

  render();
  return nullptr;
}


void ConsoleLauncherMode::render()
{
  ui::ConsoleMenuView view;
  view.mpList = &mList;
  view.mDrawMainBackdrop = true;
  view.mDimAlpha = 255;

  if (mScreen == Screen::Dn1Menu)
  {
    view.mTitle = "DUKE NUKEM";
    view.mBackLabel = "BACK";
  }
  else if (mScreen == Screen::Dn1Episodes)
  {
    view.mTitle = "EPISODE SELECT";
    view.mBackLabel = "BACK";
  }
  else if (mScreen == Screen::Dn1Levels)
  {
    view.mTitle = "LEVEL SELECT";
    view.mSubtitle = "EPISODE " + std::to_string(mDn1Episode + 1);
    view.mBackLabel = "BACK";
    view.mConfirmLabel = "START";
    view.mInfoLines = {
      {"NEW GAME", true}, {"START WITH DEFAULT EQUIPMENT", false}};
  }
  else if (mScreen == Screen::LoadAll)
  {
    view.mTitle = "LOAD GAME";
    view.mConfirmLabel = "LOAD";
    view.mBackLabel = "BACK";
  }
  else if (mScreen == Screen::Games)
  {
    view.mTitle = "SELECT GAME";
    if (!mDn1DataAvailable)
    {
      view.mSubtitle = "DN1: " + Duke1Mode::lastLookupProblem();
    }
  }
  else if (mScreen == Screen::Credits)
  {
    view.mTitle = "CREDITS";
    view.mBackLabel = "BACK";
    view.mInfoLines = {
      {"RIGELENGINE", true},
      {"NIKOLAI WUTTKE", false},
      {"AND CONTRIBUTORS", false},
      {"DUKE NUKEM I ENGINE", true},
      {"SHIMIAN5", false},
      {"ORIGINAL GAMES", true},
      {"APOGEE / 3D REALMS", false},
      {"GAME DATA SUPPLIED BY PLAYER", false}};
  }
  else
  {
    view.mTitle = "QUIT?";
    view.mSubtitle = mQuitMessage;
    view.mBackLabel = "BACK";
  }

  ui::drawConsoleMenu(view);
}

} // namespace rigel
