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


#include "duke1_mode.hpp"

#include "console_launcher_mode.hpp"

#include "assets/resource_loader.hpp"
#include "base/image.hpp"
#include "frontend/game_service_provider.hpp"
#include "frontend/user_profile.hpp"
#include "renderer/texture.hpp"
#include "renderer/upscaling.hpp"
#include "ui/hud_skin_generator.hpp"
#include "ui/console_menu.hpp"
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

// The DOS game's timer interrupt rate (PIT divisor 0x2051), see re-notes
constexpr double TIMER_HZ = 1193182.0 / duke1::TickGate::pitDivisor;
constexpr float STICK_DEADZONE = 0.45f;
constexpr int WIDE_VIEW_HEIGHT = 200;

std::filesystem::path gGamePath;
std::string gLookupProblem;


} // namespace


void Duke1Mode::setGamePath(const std::filesystem::path& gamePath)
{
  gGamePath = gamePath;
}


const std::string& Duke1Mode::lastLookupProblem()
{
  return gLookupProblem;
}


std::optional<std::filesystem::path>
  Duke1Mode::findDataDirIn(const std::filesystem::path& gamePath)
{
  const auto previous = gGamePath;
  const auto previousProblem = gLookupProblem;
  gGamePath = gamePath;
  const auto result = findDataDir();
  gGamePath = previous;
  gLookupProblem = previousProblem;
  return result;
}


std::optional<std::filesystem::path> Duke1Mode::findDataDir()
{
  if (gGamePath.empty())
  {
    gLookupProblem = "no game path";
    return std::nullopt;
  }

  LOG_F(
    INFO,
    "Looking for Duke 1 data relative to '%s'",
    gGamePath.u8string().c_str());

  // Accept a trailing separator in the stored path
  const auto base =
    gGamePath.filename().empty() ? gGamePath.parent_path() : gGamePath;
  std::optional<std::filesystem::path> found;
  std::size_t episodeCount = 0;
  for (const auto& candidate :
       {base / "DN1",
        base / "dn1",
        base.parent_path() / "DN1",
        base / "DN1-full" / "pc",
        base.parent_path() / "DN1-full" / "pc",
        base})
  {
    std::error_code error;
    if (!std::filesystem::is_directory(candidate, error))
      continue;
    const auto count = availableEpisodes(candidate).size();
    if (count > episodeCount)
    {
      found = candidate;
      episodeCount = count;
    }
  }
  if (found)
  {
    gLookupProblem.clear();
    return found;
  }

  gLookupProblem =
    "no supported episode EXE and map (looked under " + base.u8string() + ")";
  return std::nullopt;
}


namespace
{

// DN1 had no music; each level borrows a song from Duke Nukem II when that
// game's data is present
const char* const LEVEL_MUSIC[] = {
  "RANGEA.IMF",
  "HESBACKA.IMF",
  "KICKBUTA.IMF",
  "BEGONEA.IMF",
  "SQUEAKA.IMF",
  "PROWLA.IMF",
  "DUKINA.IMF",
  "UBDEADA.IMF",
  "FIGHTA.IMF",
  "KINGDUKA.IMF"};


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


std::vector<int>
  Duke1Mode::availableEpisodes(const std::filesystem::path& dataDir)
{
  std::vector<int> episodes;
  for (int episode = 0; episode < 3; ++episode)
  {
    if (const auto executable = duke1::ExeText::load(dataDir, episode))
    {
      try
      {
        duke1::parse_level(
          duke1::read_file(dataDir, executable->levels().front().filename));
        episodes.push_back(episode);
      }
      catch (const std::exception&)
      {
      }
    }
  }
  return episodes;
}

bool Duke1Mode::selectEpisode(const int episode)
{
  auto text = duke1::ExeText::load(mDataDir, episode);
  if (!text)
    return false;
  mText = std::move(text);
  mEpisode = episode;
  mpSpeaker = std::make_unique<Duke1Speaker>(mDataDir, episode);
  mpScreens = std::make_unique<Duke1Screens>(mDataDir, *mText);
  return true;
}

void Duke1Mode::startLevel(
  const std::size_t index,
  const duke1::PlayWorld::Carry* pCarry)
{
  mInputLog.clear();
  mLevelStartCarry = pCarry ? *pCarry : duke1::PlayWorld::Carry{};
  const auto& level = mText->levels().at(index);
  auto next = std::make_unique<duke1::PlayWorld>(
    mDataDir, level.filename, &level.backdrops, mEpisode);
  if (pCarry)
  {
    next->applyCarry(*pCarry);
  }
  next->setWide(mWide);
  mpWorld = std::move(next);
  try
  {
    mContext.mpServiceProvider->playMusic(
      LEVEL_MUSIC[index % (sizeof(LEVEL_MUSIC) / sizeof(LEVEL_MUSIC[0]))]);
  }
  catch (const std::exception&)
  {
    // Duke Nukem II data isn't there: play without music
  }
  mLevelIndex = index;
  if (mAutosaveOnStart)
  {
    saveTo("dn1_autosave");
  }
  mCamInitialized = false;
  mSinceUpdate = 0.0;
  mKeyIcons = {};
}


Duke1Mode::Duke1Mode(
  Context context,
  const std::filesystem::path& dataDir,
  std::optional<std::string> loadSlot,
  const int episode,
  const std::size_t initialLevel)
  : mContext(context)
  , mDataDir(dataDir)
{
  mContext.mpServiceProvider->stopMusic();
  mWide = mContext.mpUserProfile->mOptions.widescreenModeActive() &&
    renderer::canUseWidescreenMode(mContext.mpRenderer);
  const auto saved = loadSlot ? readDuke1Save(*loadSlot) : std::nullopt;
  if (!selectEpisode(saved ? saved->mEpisode : episode))
    throw std::runtime_error("Duke 1 episode executable or data is not usable");
  if (!loadSlot || !loadFrom(*loadSlot))
  {
    if (initialLevel >= mText->levels().size())
      throw std::runtime_error("Duke 1 initial level is outside the episode");
    startLevel(initialLevel, nullptr);
    mStartJinglePending =
      true; // the "game start" jingle belongs to a new game only
    if (mpScreens && mpScreens->valid())
    {
      auto pages = mpScreens->introPages();
      if (initialLevel != 0 && !pages.empty())
      {
        auto ready = std::move(pages.back());
        pages.clear();
        pages.push_back(std::move(ready));
      }
#ifdef RIGEL_DEV_SCREENSHOT
      if (std::getenv("RIGEL_DEV_SKIP_STORY"))
      {
        pages.clear();
      }
#endif
      showPages(std::move(pages));
    }
  }
}


void Duke1Mode::handleEvent(const SDL_Event& event)
{
  auto setKey = [&](const SDL_Keycode key, const bool down) {
    switch (key)
    {
      case SDLK_LEFT:
        mInput.left = down;
        break;
      case SDLK_RIGHT:
        mInput.right = down;
        break;
      case SDLK_UP:
        mInput.up = down;
        break;
      case SDLK_DOWN:
        mInput.down = down;
        break;
      case SDLK_SPACE:
      case SDLK_LCTRL:
      case SDLK_z:
        mInput.jump = down;
        break;
      case SDLK_LALT:
      case SDLK_x:
        mInput.fire = down;
        break;
      default:
        break;
    }
  };

  switch (event.type)
  {
    case SDL_KEYDOWN:
      if (event.key.keysym.sym == SDLK_ESCAPE && !event.key.repeat)
      {
        mPauseRequested = true;
      }
      if (event.key.keysym.sym == SDLK_F5 && !event.key.repeat)
      {
        mQuickSaveRequested = true;
      }
      if (event.key.keysym.sym == SDLK_F9 && !event.key.repeat)
      {
        mQuickLoadRequested = true;
      }
      setKey(event.key.keysym.sym, true);
      break;

    case SDL_KEYUP:
      setKey(event.key.keysym.sym, false);
      break;

    case SDL_CONTROLLERBUTTONDOWN:
    case SDL_CONTROLLERBUTTONUP:
      {
        const auto down = event.type == SDL_CONTROLLERBUTTONDOWN;
        switch (event.cbutton.button)
        {
          case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
            mDpad[0] = down;
            break;
          case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
            mDpad[1] = down;
            break;
          case SDL_CONTROLLER_BUTTON_DPAD_UP:
            mDpad[2] = down;
            break;
          case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
            mDpad[3] = down;
            break;
          case SDL_CONTROLLER_BUTTON_BACK:
            mBackHeld = down;
            break;
          case SDL_CONTROLLER_BUTTON_A:
          case SDL_CONTROLLER_BUTTON_B:
          case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
            if (mBackHeld && down)
            {
              mQuickLoadRequested = true; // BACK + A/B loads, like in Duke II
            }
            else
            {
              mInput.jump = down;
            }
            break;
          case SDL_CONTROLLER_BUTTON_X:
          case SDL_CONTROLLER_BUTTON_Y:
          case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
            if (mBackHeld && down)
            {
              mQuickSaveRequested = true; // BACK + X/Y saves
            }
            else
            {
              mInput.fire = down;
            }
            break;
          case SDL_CONTROLLER_BUTTON_START:
            if (down)
            {
              mPauseRequested = true;
            }
            break;
          default:
            break;
        }
        break;
      }

    case SDL_CONTROLLERAXISMOTION:
      {
        const auto value = static_cast<float>(event.caxis.value) / 32767.0f;
        if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX)
        {
          mStickX = value;
        }
        else if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY)
        {
          mStickY = value;
        }
        else if (event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT)
        {
          mInput.jump = value > 0.5f;
        }
        else if (event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT)
        {
          mInput.fire = value > 0.5f;
        }
        break;
      }

    default:
      break;
  }
}


ui::HudArt Duke1Mode::duke1HudArt() const
{
  // In the game Duke's hand covers the rear half of his gun. The barrel is cut
  // out of his standing sprite; the receiver, end cap and grip that his hand
  // hides are drawn in the same style and palette.
  constexpr int BARREL_X = 22;
  constexpr int BARREL_Y = 13;
  constexpr int BARREL_W = 10;
  constexpr int BARREL_H = 6;

  // clang-format off
  static const char* const REAR[] = {
    "..............",
    ".AAAAAAAAAAAAA",
    ".ADDAGGGGGGGGG",
    ".ADDAFFFFFFFFF",
    ".AFFAFFFEEFFFF",
    ".AFFAFFFEEFFFF",
    ".AAAADDDDDDDDD",
    ".....AFFFFA...",
    ".....ADDDDA...",
    ".....AAAAAA..."};
  // clang-format on
  constexpr int REAR_W = 14;
  constexpr int GRID_W = REAR_W + BARREL_W;
  constexpr int GRID_H = 10;

  const auto colorFor = [](const char c) {
    switch (c)
    {
      case 'A': return base::Color{0, 0, 0, 255};
      case 'D': return base::Color{85, 85, 85, 255};
      case 'F': return base::Color{170, 170, 170, 255};
      case 'G': return base::Color{255, 255, 255, 255};
      case 'E': return base::Color{170, 0, 170, 255};
      default: return base::Color{0, 0, 0, 0};
    }
  };

  const auto portrait = mpWorld->standingPortrait();
  data::PixelBuffer pixels(std::size_t(GRID_W) * GRID_H, base::Color{0, 0, 0, 0});
  for (int y = 0; y < GRID_H; ++y)
  {
    for (int x = 0; x < REAR_W; ++x)
    {
      pixels[std::size_t(y) * GRID_W + std::size_t(x)] = colorFor(REAR[y][x]);
    }
  }
  for (int y = 0; y < BARREL_H; ++y)
  {
    for (int x = 0; x < BARREL_W; ++x)
    {
      const auto k =
        (std::size_t(BARREL_Y + y) * 32 + std::size_t(BARREL_X + x)) * 4;
      pixels[std::size_t(1 + y) * GRID_W + std::size_t(REAR_W + x)] =
        base::Color{portrait[k], portrait[k + 1], portrait[k + 2], portrait[k + 3]};
    }
  }

  ui::HudArt art;
  art.mGun = data::Image{std::move(pixels), GRID_W, GRID_H};
  return art;
}


void Duke1Mode::submitOverlayHud()
{
  const auto tier =
    ui::hudTierForWindowHeight(mContext.mpRenderer->windowSize().height);
  if (!mHudSkinTierTried || *mHudSkinTierTried != tier)
  {
    mHudSkin = ui::HudSkin::tryLoad(
      mContext.mpRenderer, gGamePath, tier);
    if (!mHudSkin && !mGeneratedHudSkin)
    {
      mGeneratedHudSkin = ui::HudSkin::generateDefault(
        mContext.mpRenderer, duke1HudArt());
    }
    mHudSkinTierTried = tier;
  }

  const auto* pSkin =
    mHudSkin ? &*mHudSkin : (mGeneratedHudSkin ? &*mGeneratedHudSkin : nullptr);
  if (!pSkin)
  {
    return;
  }

  const auto& player = mpWorld->player();
  ui::OverlayHudState state;
  state.health = player.health;
  state.score = mpWorld->score();
  state.ammo = player.capacity;
  state.maxAmmo = 4;
  state.showRadar = false;
  state.showRadarDisc = false;
  for (int i = 0; i < 8; ++i)
  {
    if (!mpWorld->hasKey(i))
    {
      continue;
    }
    if (!mKeyIcons[std::size_t(i)])
    {
      data::PixelBuffer pixels;
      const auto rgba = mpWorld->keyIcon(i);
      for (std::size_t k = 0; k + 3 < rgba.size(); k += 4)
      {
        pixels.emplace_back(rgba[k], rgba[k + 1], rgba[k + 2], rgba[k + 3]);
      }
      mKeyIcons[std::size_t(i)] = std::make_shared<renderer::Texture>(
        mContext.mpRenderer, data::Image{std::move(pixels), 16, 16});
    }
    state.customIcons.push_back(mKeyIcons[std::size_t(i)]);
  }
  state.timeSeconds = double(SDL_GetTicks()) / 1000.0;

  mContext.mpServiceProvider->setNativeOverlay(
    [pRenderer = mContext.mpRenderer, pSkin, state](const int alpha) {
      ui::drawOverlayHud(pRenderer, *pSkin, state, alpha);
    });
}


std::string Duke1Mode::levelLabel(const std::size_t index)
{
  return "LEVEL " + std::to_string(index + 1);
}


bool Duke1Mode::saveTo(const std::string& slot)
{
  if (mDebugUsed)
  {
    return false; // a debug jump can't be replayed from the input log
  }
  Duke1Save save;
  save.mLevelIndex = static_cast<int>(mLevelIndex);
  save.mEpisode = mEpisode;
  save.mCarry = mLevelStartCarry;
  save.mInputs = mInputLog;
  save.mWide = mWide;
  return writeDuke1Save(slot, save);
}


bool Duke1Mode::loadFrom(const std::string& slot)
{
  const auto oSave = readDuke1Save(slot);
  const auto executable =
    oSave ? duke1::ExeText::load(mDataDir, oSave->mEpisode) : std::nullopt;
  if (
    !oSave || !executable || oSave->mLevelIndex < 0 ||
    std::size_t(oSave->mLevelIndex) >= executable->levels().size())
  {
    return false;
  }

  if (oSave->mEpisode != mEpisode && !selectEpisode(oSave->mEpisode))
    return false;
  mEpisodeFinished = false;
  mAutosaveOnStart = false;
  mWide = oSave->mWide;
  startLevel(std::size_t(oSave->mLevelIndex), &oSave->mCarry);
  mAutosaveOnStart = true;

  // Replaying the recorded inputs recreates the exact state
  for (const auto packed : oSave->mInputs)
  {
    mpWorld->update(unpackDuke1Input(packed));
  }
  mpWorld->takeSounds(); // nothing of the replay should be heard
  mInputLog = oSave->mInputs;
  mTimerRemainder = 0.0;
  mSinceUpdate = 0.0;
  mPause = PauseScreen::None;
  return true;
}


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


bool Duke1Mode::showPages(std::vector<std::vector<std::uint8_t>> pages)
{
  mPages = std::move(pages);
  mPageIndex = 0;
  return !mPages.empty();
}


void Duke1Mode::drawPicture(const std::vector<std::uint8_t>& rgba)
{
  data::PixelBuffer pixels;
  pixels.reserve(rgba.size() / 4);
  for (std::size_t i = 0; i < rgba.size(); i += 4)
  {
    pixels.emplace_back(rgba[i], rgba[i + 1], rgba[i + 2], rgba[i + 3]);
  }
  const renderer::Texture picture{
    mContext.mpRenderer,
    data::Image{
      std::move(pixels),
      std::size_t(DUKE1_SCREEN_W),
      std::size_t(DUKE1_SCREEN_H)}};
  if (mWide)
  {
    // One picture pixel per buffer pixel, centered: the engine's upscaler then
    // scales it like the game
    mContext.mpServiceProvider->markCurrentFrameAsWidescreen();
    const auto target = mContext.mpRenderer->currentRenderTargetSize();
    const auto saved = renderer::saveState(mContext.mpRenderer);
    mContext.mpRenderer->setClipRect({});
    picture.render(
      base::Rect<int>{{0, 0}, {DUKE1_SCREEN_W, DUKE1_SCREEN_H}},
      base::Rect<int>{
        {(target.width - DUKE1_SCREEN_W) / 2,
         (target.height - DUKE1_SCREEN_H) / 2},
        {DUKE1_SCREEN_W, DUKE1_SCREEN_H}});
  }
  else
  {
    picture.render(0, 0);
  }
}


std::unique_ptr<GameMode> Duke1Mode::updateAndRender(
  const engine::TimeDelta dt,
  const std::vector<SDL_Event>& events)
{
  if (!mPages.empty())
  {
    // Story and message pages: A/Enter/Space turn the page, START/Esc skip them
    // all
    for (const auto& event : events)
    {
      const bool start =
        (event.type == SDL_CONTROLLERBUTTONDOWN &&
         event.cbutton.button == SDL_CONTROLLER_BUTTON_START) ||
        (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE &&
         !event.key.repeat);
      const bool next = ui::isMenuConfirmButton(event) ||
        (event.type == SDL_KEYDOWN && !event.key.repeat &&
         (event.key.keysym.sym == SDLK_RETURN ||
          event.key.keysym.sym == SDLK_SPACE));
      if (start)
      {
        mPages.clear();
      }
      else if (next && ++mPageIndex >= mPages.size())
      {
        mPages.clear();
      }
      if (mPages.empty())
      {
        break;
      }
    }
    if (!mPages.empty())
    {
      drawPicture(mPages[mPageIndex]);
      return nullptr;
    }
  }

  if (mEpisodeFinished && mPages.empty())
    return std::make_unique<ConsoleLauncherMode>(mContext);

  for (const auto& event : events)
  {
    if (mPause == PauseScreen::None)
    {
      handleEvent(event);
      continue;
    }

    // Pause menu navigation
    const auto startPressed = event.type == SDL_CONTROLLERBUTTONDOWN &&
      event.cbutton.button == SDL_CONTROLLER_BUTTON_START;
    const auto escPressed = event.type == SDL_KEYDOWN &&
      event.key.keysym.sym == SDLK_ESCAPE && !event.key.repeat;
    if ((startPressed || escPressed) && mPause == PauseScreen::Stats)
    {
      activatePauseSelection();
      continue;
    }
    if (startPressed || escPressed)
    {
      mPause =
        mPause == PauseScreen::Main ? PauseScreen::None : PauseScreen::Main;
      if (mPause == PauseScreen::Main)
      {
        enterPauseScreen(PauseScreen::Main);
      }
      continue;
    }

    switch (mNavigation.convert(event))
    {
      case ui::NavigationEvent::NavigateUp:
        mPauseList.move(-1);
        break;
      case ui::NavigationEvent::NavigateDown:
        mPauseList.move(1);
        break;
      case ui::NavigationEvent::Confirm:
        activatePauseSelection();
        break;
      case ui::NavigationEvent::Cancel:
        if (mPause == PauseScreen::Stats)
        {
          break;
        }
        if (mPause == PauseScreen::Main)
        {
          mPause = PauseScreen::None;
        }
        else if (
          mPause == PauseScreen::DebugLevels ||
          mPause == PauseScreen::DebugFeatures)
        {
          enterPauseScreen(PauseScreen::Debug);
        }
        else
        {
          enterPauseScreen(PauseScreen::Main);
        }
        break;
      default:
        break;
    }
  }

  if (mQuitRequested)
  {
    mContext.mpServiceProvider->fadeOutScreen();
    return std::make_unique<ConsoleLauncherMode>(mContext);
  }

  if (mQuickSaveRequested)
  {
    mQuickSaveRequested = false;
    mToast = saveTo("dn1_quicksave") ? "QUICK SAVED" : "SAVE FAILED";
    mToastSeconds = 1.5;
  }
  if (mQuickLoadRequested)
  {
    mQuickLoadRequested = false;
    mToast = loadFrom("dn1_quicksave") ? "QUICK LOADED" : "NO QUICK SAVE";
    mToastSeconds = 1.5;
  }
  if (mPauseRequested)
  {
    mPauseRequested = false;
    if (mPause == PauseScreen::None)
    {
      mInput = {};
      mDpad[0] = mDpad[1] = mDpad[2] = mDpad[3] = false;
      mStickX = mStickY = 0.0f;
      enterPauseScreen(PauseScreen::Main);
    }
  }
  const auto paused = mPause != PauseScreen::None;

  // Merge d-pad and stick into the direction inputs
  auto input = mInput;
  input.left = input.left || mDpad[0] || mStickX < -STICK_DEADZONE;
  input.right = input.right || mDpad[1] || mStickX > STICK_DEADZONE;
  input.up = input.up || mDpad[2] || mStickY < -STICK_DEADZONE;
  input.down = input.down || mDpad[3] || mStickY > STICK_DEADZONE;

  {
    const auto& soundOptions = mContext.mpUserProfile->mOptions;
    mpSpeaker->setVolume(
      soundOptions.mSoundOn ? soundOptions.mSoundVolume : 0.0f);
  }
  if (mStartJinglePending)
  {
    // Played from the first frame on: mode changes stop all sounds
    mStartJinglePending = false;
    mpSpeaker->play(26); // STARTGAME: four descending sweeps (index 25 is the
                         // cheat-mode beeps)
  }

  // Drive the original timer-callback gate from real time
  if (!paused)
  {
    mTimerRemainder += dt * TIMER_HZ;
  }
  const auto callbacks = static_cast<std::uint64_t>(mTimerRemainder);
  mTimerRemainder -= static_cast<double>(callbacks);
  if (!paused)
  {
    mSinceUpdate += dt;
  }
  if (callbacks > 0 && mGate.callbacks(callbacks))
  {
    mpWorld->update(input);
    for (const auto index : mpWorld->takeSounds())
    {
      mpSpeaker->play(index);
    }
    if (mpWorld->takeDiscontinuity())
    {
      mCamInitialized =
        false; // snap the camera instead of gliding across the level
    }
    mInputLog.push_back(packDuke1Input(input));
    if (mpWorld->dead())
    {
      // Duke died: start the level again with full health
      auto carry = mLevelStartCarry;
      carry.health = 9;
      mContext.mpServiceProvider->fadeOutScreen();
      startLevel(mLevelIndex, &carry);
      mFadeInPending = true;
      return nullptr;
    }
    if (mpWorld->levelComplete())
    {
      const auto carry = mpWorld->carry();
      mStatsCarry = carry;
      {
        // Secret bonuses are paid when the level is left: 10,000 for each one
        // that was earned
        static const char* const NAMES[] = {
          "CAMERAS",
          "ACME SIGNS",
          "ROCKETS",
          "SNAKE BOTS",
          "BUNNY BOTS",
          "NO DAMAGE",
          "D-U-K-E"};
        const auto report = mpWorld->bonusReport();
        std::vector<ui::ConsoleResultsScreen::Row> rows;
        for (int i = 0; i < duke1::PlayWorld::NUM_BONUSES; ++i)
        {
          if (report.present[i])
          {
            rows.push_back({NAMES[i], report.earned[i]});
          }
        }
        const auto seconds = mpWorld->updateCount() / 16;
        const auto time = std::to_string(seconds / 60) + ":" +
          (seconds % 60 < 10 ? "0" : "") + std::to_string(seconds % 60);

        ui::ConsoleResultsScreen::Sounds sounds;
        sounds.mRowEarned = [this] {
          mpSpeaker->play(37);
        }; // GETDUKESND
        sounds.mScoreTick = [this] {
          mpSpeaker->play(16);
        }; // GETBONUSOBJ
        sounds.mTotal = [this] {
          mpSpeaker->play(19);
        }; // LEVELDONE
        mpResults = std::make_unique<ui::ConsoleResultsScreen>(
          levelLabel(mLevelIndex) + " COMPLETE",
          time,
          carry.score,
          10000,
          std::move(rows),
          std::move(sounds));
        mStatsCarry.score += mpResults->bonusTotal();
      }
      mInput = {};
      mDpad[0] = mDpad[1] = mDpad[2] = mDpad[3] = false;
      mStickX = mStickY = 0.0f;
      enterPauseScreen(PauseScreen::Stats);
      return nullptr;
    }
    // Keep the sub-tick remainder so the blend phase doesn't restart at every
    // update
    const auto measured = std::clamp(mSinceUpdate, 0.02, 0.25);
    mUpdateInterval += (measured - mUpdateInterval) * 0.1;
    mSinceUpdate =
      std::clamp(mSinceUpdate - mUpdateInterval, 0.0, mUpdateInterval);
    mPrevCamX = mCamX;
    mPrevCamY = mCamY;
    mCamStepPending = true;
  }

  // The game logic runs at about 16 updates per second; interpolate Duke's
  // drawn position in between so scrolling and movement look smooth.
  const auto blendFactor = std::clamp(mSinceUpdate / mUpdateInterval, 0.0, 1.0);
  const auto drawX = static_cast<int>(std::lround(
    mpWorld->prevWorldX() +
    (mpWorld->worldX() - mpWorld->prevWorldX()) * blendFactor));
  const auto drawY = static_cast<int>(std::lround(
    mpWorld->prevWorldY() +
    (mpWorld->worldY() - mpWorld->prevWorldY()) * blendFactor));

  mpWorld->setRenderBlend(blendFactor);
  std::vector<std::uint8_t> rgba;
  int frameWidth = duke1::SCREEN_W;
  int frameHeight = duke1::SCREEN_H;
  int bufferWidth = duke1::SCREEN_W;
  int bufferHeight = duke1::SCREEN_H;

  const auto& options = mContext.mpUserProfile->mOptions;
  const auto wide = options.widescreenModeActive() &&
    renderer::canUseWidescreenMode(mContext.mpRenderer);

  if (wide)
  {
    // Modern presentation like the console release: no frame, a wide view
    // that follows Duke smoothly, and the overlay HUD. The simulation keeps
    // running on the original 13x10 tile camera rules, only what is drawn
    // changes.
    mContext.mpServiceProvider->markCurrentFrameAsWidescreen();
    const auto target = mContext.mpRenderer->currentRenderTargetSize();
    bufferWidth = target.width;
    bufferHeight = target.height;

    // One game pixel per buffer pixel: the engine's upscaler then scales
    // everything uniformly, so squares stay square. This shows 12.5 tile
    // rows; a taller view would need a non-integer resample first.
    frameHeight = WIDE_VIEW_HEIGHT;
    frameWidth = static_cast<int>(
      std::ceil(double(bufferWidth) * WIDE_VIEW_HEIGHT / bufferHeight));

    // The camera follows the simulated position and takes one smoothing step
    // per game update; between updates it is blended linearly, like Duke.
    const auto targetX = std::clamp(
      static_cast<double>(mpWorld->worldX()) - frameWidth / 2.0,
      0.0,
      2048.0 - frameWidth);
    const auto targetY = std::clamp(
      static_cast<double>(mpWorld->worldY()) - 132.0,
      0.0,
      1440.0 - frameHeight);
    if (!mCamInitialized)
    {
      mCamX = mPrevCamX = targetX;
      mCamY = mPrevCamY = targetY;
      mCamInitialized = true;
      mCamStepPending = false;
    }
    else if (mCamStepPending)
    {
      const auto step = 1.0 - std::exp(-mUpdateInterval * 9.0);
      mCamX += (targetX - mCamX) * step;
      mCamY += (targetY - mCamY) * step;
      mCamStepPending = false;
    }
    const auto camX = static_cast<int>(
      std::lround(mPrevCamX + (mCamX - mPrevCamX) * blendFactor));
    const auto camY = static_cast<int>(
      std::lround(mPrevCamY + (mCamY - mPrevCamY) * blendFactor));

    mpWorld->renderWorld(
      rgba, frameWidth, frameHeight, camX, camY, drawX, drawY);

    submitOverlayHud();
  }
  else
  {
    mpWorld->render(rgba);
  }

  data::PixelBuffer pixels;
  pixels.reserve(rgba.size() / 4);
  for (std::size_t i = 0; i < rgba.size(); i += 4)
  {
    pixels.emplace_back(rgba[i], rgba[i + 1], rgba[i + 2], rgba[i + 3]);
  }
  const renderer::Texture frame{
    mContext.mpRenderer,
    data::Image{
      std::move(pixels), std::size_t(frameWidth), std::size_t(frameHeight)}};
  if (wide)
  {
    const auto saved = renderer::saveState(mContext.mpRenderer);
    mContext.mpRenderer->setClipRect({});
    frame.render(
      base::Rect<int>{{0, 0}, {frameWidth, frameHeight}},
      base::Rect<int>{{0, 0}, {bufferWidth, bufferHeight}});
  }
  else
  {
    frame.render(0, 0);
  }

  if (mPause == PauseScreen::Stats && mpResults)
  {
    mpResults->update(dt);
  }
  if (paused)
  {
    drawPauseMenu();
  }
  if (mToastSeconds > 0.0)
  {
    mToastSeconds -= dt;
    const auto display = ImGui::GetIO().DisplaySize;
    ImGui::GetForegroundDrawList()->AddText(
      nullptr,
      display.y / 24.0f,
      {display.x / 2.0f - mToast.size() * display.y / 96.0f, display.y * 0.1f},
      IM_COL32(255, 153, 0, 255),
      mToast.c_str());
  }

  if (mFadeInPending)
  {
    mFadeInPending = false;
    mContext.mpServiceProvider->fadeInScreen();
  }

  return nullptr;
}

} // namespace rigel
