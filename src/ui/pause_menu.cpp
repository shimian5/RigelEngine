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

#include "pause_menu.hpp"

#include "assets/png_image.hpp"
#include "frontend/game_service_provider.hpp"
#include "frontend/replay_save.hpp"
#include <cstdio>
#include <cstdlib>


namespace rigel::ui
{

SaveSlotSource SaveSlotSource::duke2()
{
  SaveSlotSource source;
  source.mNumSlots = NUM_SAVE_SLOTS_CONSOLE;
  source.mSummary = [](const int slot) -> std::optional<SaveSlotInfo> {
    if (const auto oSummary = readSaveSummary(consoleSlotName(slot)))
    {
      return SaveSlotInfo{
        oSummary->mEpisode,
        oSummary->mLevel,
        oSummary->mScore,
        oSummary->mPlayTimeSeconds};
    }
    return std::nullopt;
  };
  source.mHasAnySave = [] {
    return newestSaveSlotName().has_value();
  };
  source.mSlotName = [](const int slot) {
    return consoleSlotName(slot);
  };
  return source;
}


PauseMenu::PauseMenu(
  GameMode::Context context,
  SaveSlotSource slots,
  const bool debugEntry)
  : mContext(context)
  , mSlots(std::move(slots))
  , mDebugEntry(debugEntry)
{
}


bool PauseMenu::isOpenTrigger(const SDL_Event& event)
{
  if (event.type == SDL_KEYDOWN && event.key.repeat == 0)
  {
    return event.key.keysym.sym == SDLK_ESCAPE ||
      event.key.keysym.sym == SDLK_AC_BACK;
  }

  return event.type == SDL_CONTROLLERBUTTONDOWN &&
    event.cbutton.button == SDL_CONTROLLER_BUTTON_START;
}


void PauseMenu::open(const Info& info)
{
  mInfo = info;
  mActive = true;
  mAction = Action::None;
  mElapsed = 0.0;
  mOptionsMenu.reset();
  enterScreen(Screen::Main);

#ifdef RIGEL_DEV_SCREENSHOT
  // RIGEL_DEV_PAUSE_KEYS="d,d,a" applies navigation steps right away
  if (const auto* pKeys = std::getenv("RIGEL_DEV_PAUSE_KEYS"))
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
          goBack();
          break;
        default:
          break;
      }
    }
  }
#endif
}


PauseMenu::Action PauseMenu::takeAction(int* pSlotIndex)
{
  const auto action = mAction;
  if (pSlotIndex)
  {
    *pSlotIndex = mActionSlot;
  }
  mAction = Action::None;
  return action;
}


void PauseMenu::enterScreen(const Screen screen)
{
  mScreen = screen;
  mThumbnailSlot = -1;
  mThumbnail.reset();

  switch (screen)
  {
    case Screen::Main:
      {
        mHasAnySave = mSlots.mHasAnySave();
        std::vector<ConsoleMenuItem> items{
          {"QUICK SAVE", true},
          {"LOAD LAST SAVE", mHasAnySave},
          {"SAVE GAME", true},
          {"LOAD GAME", true},
          {"OPTIONS", true},
          {"CONTROLS", true}};
        if (mDebugEntry)
        {
          items.push_back({"DEBUG", true});
        }
        items.push_back({"QUIT GAME", true});
        mList.setItems(std::move(items));
        break;
      }

    case Screen::SaveSlots:
    case Screen::LoadSlots:
      {
        std::vector<ConsoleMenuItem> items;
        for (auto i = 0; i < mSlots.mNumSlots; ++i)
        {
          const auto oSummary = mSlots.mSummary(i);
          char buffer[128];
          if (oSummary)
          {
            const auto time = oSummary->mPlayTimeSeconds
              ? "   " + formatPlayTime(*oSummary->mPlayTimeSeconds)
              : std::string{};
            std::snprintf(
              buffer,
              sizeof(buffer),
              "SLOT %d   E%d L%d   %s%s",
              i + 1,
              oSummary->mEpisode,
              oSummary->mLevel,
              formatScoreWithCommas(oSummary->mScore).c_str(),
              time.c_str());
          }
          else
          {
            std::snprintf(buffer, sizeof(buffer), "SLOT %d   EMPTY", i + 1);
          }

          // Saving can overwrite anything, loading needs an existing save
          items.push_back(
            {buffer, screen == Screen::SaveSlots || oSummary.has_value()});
        }
        mList.setItems(std::move(items));
        break;
      }

    case Screen::Controls:
      mList.setItems(
        {{"MOVE:  D-PAD / LEFT STICK", true},
         {"JUMP:  A / B / LEFT TRIGGER", true},
         {"FIRE:  X / Y / RIGHT TRIGGER", true},
         {"LOOK / ENTER:  UP", true},
         {"DUCK:  DOWN", true},
         {"QUICK SAVE:  BACK + X / Y", true},
         {"QUICK LOAD:  BACK + A / B", true}});
      break;

    case Screen::QuitConfirm:
      mQuitMessage = randomQuitMessage();
      mList.setItems({{"NO, KEEP PLAYING", true}, {"YES, QUIT TO MENU", true}});
      break;

    case Screen::Options:
      mOptionsMenu.emplace(
        mContext.mpUserProfile,
        mContext.mpServiceProvider,
        mContext.mpRenderer,
        OptionsMenu::Type::InGame);
      break;
  }
}


void PauseMenu::activateSelected()
{
  if (!mList.selectedIsEnabled())
  {
    return;
  }
  const auto selected = mList.selected();

  switch (mScreen)
  {
    case Screen::Main:
      switch (selected)
      {
        case 0:
          mAction = Action::QuickSave;
          mActive = false;
          break;
        case 1:
          mAction = Action::LoadLastSave;
          mActive = false;
          break;
        case 2:
          enterScreen(Screen::SaveSlots);
          break;
        case 3:
          enterScreen(Screen::LoadSlots);
          break;
        case 4:
          enterScreen(Screen::Options);
          break;
        case 5:
          enterScreen(Screen::Controls);
          break;
        default:
          if (mDebugEntry && selected == 6)
          {
            mAction = Action::Debug;
            mActive = false;
          }
          else
          {
            enterScreen(Screen::QuitConfirm);
          }
          break;
      }
      break;

    case Screen::Controls:
      enterScreen(Screen::Main);
      break;

    case Screen::QuitConfirm:
      if (selected == 1)
      {
        mAction = Action::Quit;
        mActive = false;
      }
      else
      {
        enterScreen(Screen::Main);
      }
      break;

    case Screen::SaveSlots:
      mAction = Action::SaveToSlot;
      mActionSlot = selected;
      mActive = false;
      break;

    case Screen::LoadSlots:
      mAction = Action::LoadFromSlot;
      mActionSlot = selected;
      mActive = false;
      break;

    case Screen::Options:
      break;
  }
}


void PauseMenu::goBack()
{
  if (mScreen == Screen::Main)
  {
    mAction = Action::Resume;
    mActive = false;
  }
  else
  {
    mOptionsMenu.reset();
    enterScreen(Screen::Main);
  }
}


void PauseMenu::handleEvent(const SDL_Event& event)
{
  if (!mActive)
  {
    return;
  }

  if (mScreen == Screen::Options && mOptionsMenu)
  {
    mOptionsMenu->handleEvent(event);
    return;
  }

  if (
    event.type == SDL_CONTROLLERBUTTONDOWN &&
    event.cbutton.button == SDL_CONTROLLER_BUTTON_START)
  {
    mAction = Action::Resume;
    mActive = false;
    return;
  }

  switch (mNavigationHelper.convert(event))
  {
    case NavigationEvent::NavigateUp:
      mList.move(-1);
      break;

    case NavigationEvent::NavigateDown:
      mList.move(1);
      break;

    case NavigationEvent::Confirm:
      activateSelected();
      break;

    case NavigationEvent::Cancel:
      goBack();
      break;

    default:
      break;
  }
}


void PauseMenu::updateAndRender(const engine::TimeDelta dt)
{
  if (!mActive)
  {
    return;
  }

  mElapsed += dt;

  switch (mScreen)
  {
    case Screen::Main:
      {
        ConsoleMenuView view;
        view.mTitle = "GAME PAUSED";
        view.mpList = &mList;
        view.mStartLabel = "RESUME";
        view.mInfoLines = {
          {"EPISODE " + std::to_string(mInfo.mEpisode), false},
          {"LEVEL " + std::to_string(mInfo.mLevel), false},
          {"CURRENT SCORE", true},
          {formatScoreWithCommas(mInfo.mScore), false},
          {"CURRENT TIME", true},
          {formatPlayTime(mInfo.mPlayTimeSeconds), false}};
        drawConsoleMenu(view);
        break;
      }

    case Screen::Controls:
      {
        ConsoleMenuView view;
        view.mTitle = "CONTROLS";
        view.mpList = &mList;
        view.mConfirmLabel = "BACK";
        view.mBackLabel = "BACK";
        drawConsoleMenu(view);
        break;
      }

    case Screen::QuitConfirm:
      {
        ConsoleMenuView view;
        view.mTitle = "QUIT TO MENU?";
        view.mSubtitle = mQuitMessage;
        view.mpList = &mList;
        view.mBackLabel = "BACK";
        drawConsoleMenu(view);
        break;
      }

    case Screen::SaveSlots:
    case Screen::LoadSlots:
      {
        ConsoleMenuView view;
        view.mTitle = mScreen == Screen::SaveSlots ? "SAVE GAME" : "LOAD GAME";
        view.mpList = &mList;
        view.mConfirmLabel = mScreen == Screen::SaveSlots ? "SAVE" : "LOAD";
        view.mBackLabel = "BACK";

        // The screenshot stored with the highlighted slot
        if (mList.selected() != mThumbnailSlot)
        {
          mThumbnailSlot = mList.selected();
          mThumbnail.reset();
          if (
            const auto oPath =
              thumbnailFilePath(mSlots.mSlotName(mThumbnailSlot)))
          {
            std::error_code ec;
            if (std::filesystem::exists(*oPath, ec))
            {
              if (const auto oImage = assets::loadPng(*oPath))
              {
                mThumbnail.emplace(mContext.mpRenderer, *oImage);
              }
            }
          }
        }
        if (mThumbnail)
        {
          view.mImageTexture = std::uint32_t(mThumbnail->data());
          view.mImageAspect =
            float(mThumbnail->width()) / float(mThumbnail->height());
        }
        drawConsoleMenu(view);
        break;
      }

    case Screen::Options:
      if (mOptionsMenu)
      {
        mOptionsMenu->updateAndRender(dt);
        if (mOptionsMenu->isFinished())
        {
          mOptionsMenu.reset();
          enterScreen(Screen::Main);
        }
      }
      break;
  }
}

} // namespace rigel::ui
