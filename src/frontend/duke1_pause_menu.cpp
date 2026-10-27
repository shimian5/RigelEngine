// Pause menu, results page and debug menu of Duke Nukem 1.


#include "duke1_mode.hpp"

#include "console_launcher_mode.hpp"

#include "assets/resource_loader.hpp"
#include "base/image.hpp"
#include "frontend/game_service_provider.hpp"
#include "frontend/user_profile.hpp"
#include "renderer/texture.hpp"
#include "renderer/upscaling.hpp"
#include "ui/console_menu.hpp"
#include "ui/hud_skin_generator.hpp"
#include "ui/overlay_hud.hpp"

#include <imgui.h>
#include <loguru.hpp>

#include <algorithm>
#include <cmath>
#include <string_view>

namespace rigel
{

namespace
{

struct DebugFeature
{
  const char* mName;
  const char* mLevel;
  int mCol;
  int mRow;
};

const DebugFeature DEBUG_FEATURES[] = {
  {"FINAL BOSS (LEVEL 10)", "WORLDALB", 72, 40},
  {"ELECTRIC COLUMN (LEVEL 1 END)", "WORLDAL1", 76, 36},
  {"FALLING FLOOR (LEVEL 1)", "WORLDAL1", 27, 51},
  {"FALLING FLOOR (LEVEL 4)", "WORLDAL5", 50, 12},
  {"FALLING FLOOR (LEVEL 5)", "WORLDAL6", 8, 16},
  {"FALLING FLOOR (LEVEL 6)", "WORLDAL7", 36, 41},
  {"FALLING FLOOR (LEVEL 8)", "WORLDAL9", 64, 21},
  {"FALLING FLOOR (LEVEL 9)", "WORLDALA", 55, 13},
  {"FALLING FLOOR (LEVEL 10)", "WORLDALB", 98, 72},
  {"ORB CHAIN (LEVEL 4)", "WORLDAL5", 50, 6},
  {"ORB CHAIN (LEVEL 7)", "WORLDAL8", 16, 8},
  {"ORB CHAIN (LEVEL 8)", "WORLDAL9", 40, 31},
  {"ORB CHAIN (LEVEL 10)", "WORLDALB", 29, 60},
  {"ACCESS CARD (LEVEL 3)", "WORLDAL4", 81, 37},
  {"CARD READER + FORCE FIELD (LEVEL 3)", "WORLDAL4", 109, 81},
  {"WALL MACHINE (LEVEL 6)", "WORLDAL7", 10, 51},
  {"WALL MACHINE (LEVEL 7)", "WORLDAL8", 64, 32},
  {"WALL MACHINE (LEVEL 9)", "WORLDALA", 20, 50},
  {"WALL MACHINE (LEVEL 10)", "WORLDALB", 64, 58},
  {"RED BALLOONS (LEVEL 10)", "WORLDALB", 52, 47},
};


} // namespace

void Duke1Mode::enterPauseScreen(const PauseScreen screen)
{
  mPause = screen;
  std::vector<ui::ConsoleMenuItem> items;

  switch (screen)
  {
    case PauseScreen::Main:
      items = {
        {"RESUME", true},
        {"QUICK SAVE", true},
        {"LOAD LAST SAVE", newestDuke1Save().has_value()},
        {"SAVE GAME", true},
        {"LOAD GAME", true},
        {"DEBUG", true},
        {"QUIT TO MENU", true}};
      break;

    case PauseScreen::Stats:
      items = {{"CONTINUE", true}};
      break;

    case PauseScreen::Debug:
      items = {
        {"SKIP TO LEVEL", true},
        {"GO TO FEATURE", true},
        {"GIVE ALL ITEMS", true},
        {"FINISH LEVEL", true}};
      break;

    case PauseScreen::DebugLevels:
      for (std::size_t i = 0; i < mText->levels().size(); ++i)
        items.push_back({levelLabel(i), true});
      break;

    case PauseScreen::DebugFeatures:
      for (const auto& f : DEBUG_FEATURES)
        items.push_back({f.mName, mEpisode == 0});
      break;

    case PauseScreen::SaveSlots:
    case PauseScreen::LoadSlots:
      for (int i = 0; i < NUM_DUKE1_SLOTS; ++i)
      {
        const auto oSave = readDuke1Save(duke1SlotName(i));
        std::string label = "SLOT " + std::to_string(i + 1) + "   ";
        label += oSave ? "E" + std::to_string(oSave->mEpisode + 1) + " " +
            levelLabel(std::size_t(std::max(0, oSave->mLevelIndex))) + "   " +
            ui::formatScoreWithCommas(oSave->mCarry.score)
                       : "EMPTY";
        items.push_back(
          {label, screen == PauseScreen::SaveSlots || oSave.has_value()});
      }
      break;

    case PauseScreen::None:
      break;
  }

  mPauseList.setItems(std::move(items));
}


void Duke1Mode::activatePauseSelection()
{
  if (!mPauseList.selectedIsEnabled())
  {
    return;
  }

  const auto selected = mPauseList.selected();
  switch (mPause)
  {
    case PauseScreen::Main:
      switch (selected)
      {
        case 0:
          mPause = PauseScreen::None;
          break;
        case 1:
          mToast = saveTo("dn1_quicksave") ? "QUICK SAVED" : "SAVE FAILED";
          mToastSeconds = 1.5;
          mPause = PauseScreen::None;
          break;
        case 2:
          if (const auto newest = newestDuke1Save())
          {
            mToast = loadFrom(*newest) ? "LOADED" : "LOAD FAILED";
            mToastSeconds = 1.5;
          }
          break;
        case 3:
          enterPauseScreen(PauseScreen::SaveSlots);
          break;
        case 4:
          enterPauseScreen(PauseScreen::LoadSlots);
          break;
        case 5:
          enterPauseScreen(PauseScreen::Debug);
          break;
        case 6:
          mQuitRequested = true;
          break;
        default:
          break;
      }
      break;

    case PauseScreen::Stats:
      {
        if (mpResults && !mpResults->confirm())
        {
          break; // the first press only finishes the tally
        }
        mpResults.reset();
        mPause = PauseScreen::None;
        mContext.mpServiceProvider->fadeOutScreen();
        if (mLevelIndex + 1 >= mText->levels().size())
        {
          mEpisodeFinished = true;
          if (mpScreens && mpScreens->valid())
            showPages(mpScreens->endingPages());
          break;
        }
        startLevel(mLevelIndex + 1, &mStatsCarry);
        mFadeInPending = true;
        break;
      }

    case PauseScreen::Debug:
      switch (selected)
      {
        case 0:
          enterPauseScreen(PauseScreen::DebugLevels);
          break;
        case 1:
          enterPauseScreen(PauseScreen::DebugFeatures);
          break;
        case 2:
          mpWorld->grantAll();
          mDebugUsed = true;
          mToast = "ALL ITEMS";
          mToastSeconds = 1.5;
          mPause = PauseScreen::None;
          break;
        case 3:
          mpWorld->debugFinishLevel();
          mPause = PauseScreen::None;
          break;
        default:
          break;
      }
      break;

    case PauseScreen::DebugLevels:
      {
        const auto carry = mpWorld->carry();
        mPause = PauseScreen::None;
        startLevel(std::size_t(selected), &carry);
        mDebugUsed = true;
        mFadeInPending = true;
        break;
      }

    case PauseScreen::DebugFeatures:
      {
        const auto& feature = DEBUG_FEATURES[std::size_t(selected)];
        auto carry = mpWorld->carry();
        mPause = PauseScreen::None;
        std::size_t index = 0;
        for (std::size_t i = 0; i < mText->levels().size(); ++i)
          if (
            std::filesystem::path(mText->levels()[i].filename).stem() ==
            feature.mLevel)
            index = i;
        startLevel(index, &carry);
        mpWorld->teleportToCell(feature.mCol, feature.mRow);
        mpWorld->grantAll();
        mDebugUsed = true;
        mFadeInPending = true;
        break;
      }

    case PauseScreen::SaveSlots:
      mToast = saveTo(duke1SlotName(selected)) ? "GAME SAVED" : "SAVE FAILED";
      mToastSeconds = 1.5;
      mPause = PauseScreen::None;
      break;

    case PauseScreen::LoadSlots:
      mToast =
        loadFrom(duke1SlotName(selected)) ? "GAME LOADED" : "LOAD FAILED";
      mToastSeconds = 1.5;
      break;

    case PauseScreen::None:
      break;
  }
}


void Duke1Mode::drawPauseMenu()
{
  ui::ConsoleMenuView view;
  view.mpList = &mPauseList;
  view.mDimAlpha = 168;
  switch (mPause)
  {
    case PauseScreen::Main:
      view.mTitle = "GAME PAUSED";
      view.mStartLabel = "RESUME";
      view.mInfoLines = {
        {levelLabel(mLevelIndex), false},
        {"CURRENT SCORE", true},
        {ui::formatScoreWithCommas(mpWorld->score()), false}};
      break;
    case PauseScreen::SaveSlots:
      view.mTitle = "SAVE GAME";
      view.mConfirmLabel = "SAVE";
      view.mBackLabel = "BACK";
      break;
    case PauseScreen::LoadSlots:
      view.mTitle = "LOAD GAME";
      view.mConfirmLabel = "LOAD";
      view.mBackLabel = "BACK";
      break;
    case PauseScreen::Stats:
      if (mpResults)
      {
        mpResults->draw();
      }
      return;
    case PauseScreen::Debug:
      view.mTitle = "DEBUG";
      view.mBackLabel = "BACK";
      break;
    case PauseScreen::DebugLevels:
      view.mTitle = "SKIP TO LEVEL";
      view.mBackLabel = "BACK";
      break;
    case PauseScreen::DebugFeatures:
      view.mTitle = "GO TO FEATURE";
      view.mBackLabel = "BACK";
      break;
    case PauseScreen::None:
      return;
  }
  ui::drawConsoleMenu(view);
}

} // namespace rigel
