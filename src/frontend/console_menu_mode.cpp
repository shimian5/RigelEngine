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

#include "console_menu_mode.hpp"

#include "console_launcher_mode.hpp"
#include "game_session_mode.hpp"
#include "menu_mode.hpp"

#include "assets/resource_loader.hpp"
#include "data/game_session_data.hpp"
#include "data/high_score_list.hpp"
#include "frontend/game_service_provider.hpp"
#include "frontend/replay_save.hpp"
#include "frontend/user_profile.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>


namespace rigel
{

namespace
{

constexpr data::Difficulty DIFFICULTIES[] = {
  data::Difficulty::Easy,
  data::Difficulty::Medium,
  data::Difficulty::Hard};

} // namespace


ConsoleMenuMode::ConsoleMenuMode(Context context)
  : mContext(context)
{
  mContext.mpServiceProvider->playMusic("DUKEIIA.IMF");
  enter(Screen::Main);

#ifdef RIGEL_DEV_SCREENSHOT
  // RIGEL_DEV_MENU_KEYS="dda": d = down, u = up, a = confirm, b = back
  if (const auto* pKeys = std::getenv("RIGEL_DEV_MENU_KEYS"))
  {
    for (const auto* p = pKeys; *p; ++p)
    {
      switch (*p)
      {
        case 'd':
          mList.move(1);
          break;
        case 'u':
          mList.move(-1);
          break;
        case 'a':
          activateSelected();
          break;
        case 'b':
          (void)goBack();
          break;
        default:
          break;
      }
    }
  }
#endif
}


void ConsoleMenuMode::enter(const Screen screen)
{
  mScreen = screen;
  const auto shareware = mContext.mpServiceProvider->isSharewareVersion();

  switch (screen)
  {
    case Screen::Main:
      mList.setItems(
        {{"CONTINUE", newestSaveSlotName().has_value()},
         {"NEW GAME", true},
         {"EPISODE SELECT", true},
         {"LOAD GAME", true},
         {"STATS", true},
         {"GAME SELECT", true}});
      if (!mList.selectedIsEnabled())
      {
        mList.move(1);
      }
      break;

    case Screen::Episodes:
    case Screen::StatsEpisodes:
      {
        // Episodes that need the registered version are not offered
        std::vector<ui::ConsoleMenuItem> items;
        for (auto i = 0; i < data::NUM_EPISODES; ++i)
        {
          if (!(shareware && i > 0))
          {
            items.push_back({"EPISODE " + std::to_string(i + 1), true});
          }
        }
        mList.setItems(std::move(items));
        break;
      }

    case Screen::Difficulty:
      mList.setItems({{"EASY", true}, {"MEDIUM", true}, {"HARD", true}});
      break;

    case Screen::LoadGame:
      {
        mSlotNames.clear();
        std::vector<ui::ConsoleMenuItem> items;

        auto describe = [&](const std::string& slot, const char* label) {
          const auto oSummary = readSaveSummary(slot);
          char buffer[160];
          if (oSummary)
          {
            std::snprintf(
              buffer,
              sizeof(buffer),
              "%s   E%d L%d   %s   %s",
              label,
              oSummary->mEpisode,
              oSummary->mLevel,
              ui::formatScoreWithCommas(oSummary->mScore).c_str(),
              ui::formatPlayTime(oSummary->mPlayTimeSeconds).c_str());
          }
          else
          {
            std::snprintf(buffer, sizeof(buffer), "%s   EMPTY", label);
          }
          mSlotNames.push_back(slot);
          items.push_back({buffer, oSummary.has_value()});
        };

        describe("quicksave", "QUICK SAVE");
        describe("autosave", "AUTOSAVE");
        for (auto i = 0; i < NUM_SAVE_SLOTS_CONSOLE; ++i)
        {
          describe(
            consoleSlotName(i), ("SLOT " + std::to_string(i + 1)).c_str());
        }
        mList.setItems(std::move(items));
        break;
      }

    case Screen::Stats:
    case Screen::Story:
      mList.setItems({});
      break;

    case Screen::QuitConfirm:
      mQuitMessage = ui::randomQuitMessage();
      mList.setItems({{"NO, KEEP PLAYING", true}, {"YES, QUIT", true}});
      break;
  }
}


void ConsoleMenuMode::startStory()
{
  mIntroMovie.reset();
  mContext.mpServiceProvider->stopAllSounds();
  mContext.mpServiceProvider->fadeOutScreen();
  runScript(mContext, "&Story");
  mContext.mpServiceProvider->fadeInScreen();
}


std::unique_ptr<GameMode> ConsoleMenuMode::activateSelected()
{
  if (!mList.selectedIsEnabled())
  {
    return nullptr;
  }

  const auto selected = mList.selected();

  switch (mScreen)
  {
    case Screen::Main:
      switch (selected)
      {
        case 0:
          if (const auto oSlot = newestSaveSlotName())
          {
            return loadSlot(*oSlot);
          }
          break;
        case 1:
          mChosenEpisode = 0;
          enter(Screen::Difficulty);
          break;
        case 2:
          // With a single available episode there is nothing to choose
          if (mContext.mpServiceProvider->isSharewareVersion())
          {
            mChosenEpisode = 0;
            enter(Screen::Difficulty);
          }
          else
          {
            enter(Screen::Episodes);
          }
          break;
        case 3:
          enter(Screen::LoadGame);
          break;
        case 4:
          if (mContext.mpServiceProvider->isSharewareVersion())
          {
            mChosenEpisode = 0;
            enter(Screen::Stats);
          }
          else
          {
            enter(Screen::StatsEpisodes);
          }
          break;
        case 5:
          return std::make_unique<ConsoleLauncherMode>(mContext);
      }
      break;

    case Screen::QuitConfirm:
      if (selected == 1)
      {
        mContext.mpServiceProvider->scheduleGameQuit();
      }
      else
      {
        enter(Screen::Main);
      }
      break;

    case Screen::Episodes:
      mChosenEpisode = selected;
      enter(Screen::Difficulty);
      break;

    case Screen::Difficulty:
      {
        const data::GameSessionId sessionId{
          mChosenEpisode, 0, DIFFICULTIES[selected]};
        mContext.mpServiceProvider->fadeOutScreen();

        // Like the console release, play the story after choosing a difficulty
        // when starting a new game from the first level of an episode.
        if (!mContext.mpUserProfile->mOptions.mSkipIntro && mChosenEpisode == 0)
        {
          mPendingSession = sessionId;
          mScreen = Screen::Story;
          const char* movies[] = {
            "NUKEM2.F1", "NUKEM2.F2", "NUKEM2.F3", "NUKEM2.F4"};
          if (std::all_of(
                std::begin(movies), std::end(movies), [this](auto name) {
                  return mContext.mpResources->hasMovie(name);
                }))
          {
            mIntroMovie.emplace(mContext);
            mIntroMovie->startWithLogoOnly();
            mIntroMovie->updateAndRender(0.0);
            mContext.mpServiceProvider->fadeInScreen();
          }
          else
          {
            startStory();
          }
          return nullptr;
        }
        return std::make_unique<GameSessionMode>(sessionId, mContext);
      }

    case Screen::LoadGame:
      return loadSlot(mSlotNames[std::size_t(selected)]);

    case Screen::StatsEpisodes:
      mChosenEpisode = selected;
      enter(Screen::Stats);
      break;

    case Screen::Stats:
    case Screen::Story:
      break;
  }

  return nullptr;
}


std::unique_ptr<GameMode> ConsoleMenuMode::loadSlot(const std::string& slotName)
{
  if (const auto oSave = readReplaySaveFile(slotName))
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


std::unique_ptr<GameMode> ConsoleMenuMode::goBack()
{
  switch (mScreen)
  {
    case Screen::Main:
      return std::make_unique<ConsoleLauncherMode>(mContext);

    case Screen::Difficulty:
      enter(Screen::Main);
      break;

    case Screen::Stats:
      enter(
        mContext.mpServiceProvider->isSharewareVersion()
          ? Screen::Main
          : Screen::StatsEpisodes);
      break;

    default:
      enter(Screen::Main);
      break;
  }

  return nullptr;
}


std::unique_ptr<GameMode> ConsoleMenuMode::handleEvent(const SDL_Event& event)
{
  if (mScreen == Screen::Story)
  {
    // START skips the whole story, A/Enter/etc. advance page by page
    const auto startPressed = event.type == SDL_CONTROLLERBUTTONDOWN &&
      event.cbutton.button == SDL_CONTROLLER_BUTTON_START;
    if (startPressed)
    {
      mContext.mpServiceProvider->fadeOutScreen();
      return std::make_unique<GameSessionMode>(*mPendingSession, mContext);
    }

    if (mIntroMovie)
    {
      // Any confirm button skips the intro movie and goes on to the story
      if (ui::isMenuConfirmButton(event))
      {
        startStory();
      }
      return nullptr;
    }

    mContext.mpScriptRunner->handleEvent(event);
    return nullptr;
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
      return activateSelected();

    case ui::NavigationEvent::Cancel:
      return goBack();

    default:
      break;
  }

  return nullptr;
}


void ConsoleMenuMode::render()
{
  ui::ConsoleMenuView view;
  view.mpList = &mList;
  view.mDimAlpha = 255;
  view.mDrawMainBackdrop = true;

  switch (mScreen)
  {
    case Screen::Main:
      view.mTitle = "DUKE NUKEM II";
      break;

    case Screen::Episodes:
      view.mTitle = "EPISODE SELECT";
      view.mBackLabel = "BACK";
      break;

    case Screen::Difficulty:
      view.mTitle = "DIFFICULTY";
      view.mConfirmLabel = "START";
      view.mBackLabel = "BACK";
      break;

    case Screen::LoadGame:
      view.mTitle = "LOAD GAME";
      view.mConfirmLabel = "LOAD";
      view.mBackLabel = "BACK";
      break;

    case Screen::StatsEpisodes:
      view.mTitle = "STATS";
      view.mBackLabel = "BACK";
      break;

    case Screen::Stats:
      {
        view.mTitle =
          "EPISODE " + std::to_string(mChosenEpisode + 1) + " HIGH SCORES";
        view.mBackLabel = "BACK";
        const auto& scores =
          mContext.mpUserProfile->mHighScoreLists[std::size_t(mChosenEpisode)];
        for (const auto& entry : scores)
        {
          if (entry.mScore > 0)
          {
            view.mInfoLines.emplace_back(
              entry.mName + "  " + ui::formatScoreWithCommas(entry.mScore),
              false);
          }
        }
        if (view.mInfoLines.empty())
        {
          view.mInfoLines.emplace_back("NO SCORES YET", true);
        }
        break;
      }

    case Screen::QuitConfirm:
      view.mTitle = "QUIT GAME?";
      view.mSubtitle = mQuitMessage;
      view.mBackLabel = "BACK";
      break;

    case Screen::Story:
      return;
  }

  ui::drawConsoleMenu(view);
}


std::unique_ptr<GameMode> ConsoleMenuMode::updateAndRender(
  const engine::TimeDelta dt,
  const std::vector<SDL_Event>& events)
{
  for (const auto& event : events)
  {
    if (auto next = handleEvent(event))
    {
      return next;
    }
  }

  if (mScreen == Screen::Story && mIntroMovie)
  {
    mIntroMovie->updateAndRender(dt);
    if (mIntroMovie->isFinished())
    {
      startStory();
    }
    return nullptr;
  }

  if (mScreen == Screen::Story)
  {
    mContext.mpScriptRunner->updateAndRender(dt);
    if (mContext.mpScriptRunner->hasFinishedExecution())
    {
      mContext.mpServiceProvider->fadeOutScreen();
      return std::make_unique<GameSessionMode>(*mPendingSession, mContext);
    }
    return nullptr;
  }

  render();
  return nullptr;
}


std::unique_ptr<GameMode> createMainMenuMode(GameMode::Context context)
{
  if (context.mpUserProfile->mOptions.mConsoleMenus)
  {
    return std::make_unique<ConsoleMenuMode>(context);
  }

  return std::make_unique<MenuMode>(context);
}

} // namespace rigel
