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
    case PauseScreen::Shared:
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

    case PauseScreen::None:
      break;
  }

  mPauseList.setItems(std::move(items));
}


ui::SaveSlotSource Duke1Mode::saveSlotSource()
{
  ui::SaveSlotSource source;
  source.mNumSlots = NUM_DUKE1_SLOTS;
  source.mSummary = [](const int slot) -> std::optional<ui::SaveSlotInfo> {
    if (const auto oSave = readDuke1Save(duke1SlotName(slot)))
    {
      return ui::SaveSlotInfo{
        oSave->mEpisode + 1,
        std::max(0, oSave->mLevelIndex) + 1,
        oSave->mCarry.score,
        std::nullopt};
    }
    return std::nullopt;
  };
  source.mHasAnySave = [] {
    return newestDuke1Save().has_value();
  };
  return source;
}


void Duke1Mode::openPauseMenu()
{
  mPause = PauseScreen::Shared;

  ui::PauseMenu::Info info;
  info.mEpisode = mEpisode + 1;
  info.mLevel = int(mLevelIndex) + 1;
  info.mScore = mpWorld->score();
  info.mPlayTimeSeconds = double(mpWorld->updateCount()) / 16.0;
  mPauseMenu.open(info);
}


void Duke1Mode::runPauseMenuAction()
{
  using Action = ui::PauseMenu::Action;

  const auto report = [this](const bool ok, const char* good, const char* bad) {
    mToast = ok ? good : bad;
    mToastSeconds = 1.5;
  };

  int slot = 0;
  switch (mPauseMenu.takeAction(&slot))
  {
    case Action::None:
      return; // the menu is still open

    case Action::Resume:
      mPause = PauseScreen::None;
      break;

    case Action::QuickSave:
      report(saveTo("dn1_quicksave"), "QUICK SAVED", "SAVE FAILED");
      mPause = PauseScreen::None;
      break;

    case Action::LoadLastSave:
      if (const auto newest = newestDuke1Save())
      {
        report(loadFrom(*newest), "LOADED", "LOAD FAILED");
      }
      mPause = PauseScreen::None;
      break;

    case Action::SaveToSlot:
      report(saveTo(duke1SlotName(slot)), "GAME SAVED", "SAVE FAILED");
      mPause = PauseScreen::None;
      break;

    case Action::LoadFromSlot:
      report(loadFrom(duke1SlotName(slot)), "GAME LOADED", "LOAD FAILED");
      mPause = PauseScreen::None;
      break;

    case Action::Debug:
      enterPauseScreen(PauseScreen::Debug);
      break;

    case Action::Quit:
      mQuitRequested = true;
      mPause = PauseScreen::None;
      break;
  }
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
    case PauseScreen::Shared:
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
    case PauseScreen::Shared:
      return;
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
