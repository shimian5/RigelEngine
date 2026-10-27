/* Copyright (C) 2016, Nikolai Wuttke. All rights reserved.
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

#include "game_session_mode.hpp"

#include "console_launcher_mode.hpp"

// TODO: Remove this cyclic include
#include "console_menu_mode.hpp"
#include "menu_mode.hpp"

#include "base/match.hpp"
#include "data/saved_game.hpp"
#include "frontend/game_service_provider.hpp"
#include "frontend/user_profile.hpp"
#include "ui/high_score_list.hpp"
#include "ui/menu_navigation.hpp"


namespace rigel
{

GameSessionMode::GameSessionMode(
  const data::GameSessionId& sessionId,
  Context context,
  std::optional<base::Vec2> playerPositionOverride)
  : mCurrentStage(std::make_unique<GameRunner>(
      &mPersistentPlayerState,
      sessionId,
      context,
      playerPositionOverride,
      true /* show welcome message */))
  , mEpisode(sessionId.mEpisode)
  , mCurrentLevelNr(sessionId.mLevel)
  , mDifficulty(sessionId.mDifficulty)
  , mContext(context)
{
}


GameSessionMode::GameSessionMode(const data::SavedGame& save, Context context)
  : mPersistentPlayerState(save)
  , mCurrentStage(std::make_unique<GameRunner>(
      &mPersistentPlayerState,
      save.mSessionId,
      context,
      std::nullopt,
      true /* show welcome message */))
  , mEpisode(save.mSessionId.mEpisode)
  , mCurrentLevelNr(save.mSessionId.mLevel)
  , mDifficulty(save.mSessionId.mDifficulty)
  , mContext(context)
{
}


GameSessionMode::GameSessionMode(const ReplaySave& replay, Context context)
  : mPersistentPlayerState(replay.mPlayerStateAtStart)
  , mCurrentStage(
      std::make_unique<GameRunner>(&mPersistentPlayerState, replay, context))
  , mEpisode(replay.mSessionId.mEpisode)
  , mCurrentLevelNr(replay.mSessionId.mLevel)
  , mDifficulty(replay.mSessionId.mDifficulty)
  , mContext(context)
{
}


GameSessionMode::GameSessionMode(
  const data::GameSessionId& sessionId,
  data::PersistentPlayerState persistentPlayerState,
  Context context)
  : mPersistentPlayerState(std::move(persistentPlayerState))
  , mCurrentStage(std::make_unique<GameRunner>(
      &mPersistentPlayerState,
      sessionId,
      context,
      std::nullopt,
      false /* don't show welcome message */))
  , mEpisode(sessionId.mEpisode)
  , mCurrentLevelNr(sessionId.mLevel)
  , mDifficulty(sessionId.mDifficulty)
  , mContext(context)
{
}


void GameSessionMode::handleEvent(const SDL_Event& event)
{
  base::match(
    mCurrentStage,
    [&event, this](std::unique_ptr<GameRunner>& pIngameMode) {
      pIngameMode->handleEvent(event);
    },

    [&event, this](HighScoreNameEntry& state) {
      auto fadeOut = [&state, this]() {
        mContext.mpScriptRunner->updateAndRender(0);
        state.mNameEntryWidget.updateAndRender(0);
        mContext.mpServiceProvider->fadeOutScreen();
      };

      if (ui::isCancelButton(event))
      {
        fadeOut();
        enterHighScore("");
        return;
      }
      else if (ui::isConfirmButton(event))
      {
        fadeOut();
        enterHighScore(state.mNameEntryWidget.text());
        return;
      }

      state.mNameEntryWidget.handleEvent(event);
    },

    [&event, this](HighScoreListDisplay&) {
      mContext.mpScriptRunner->handleEvent(event);
    },

    [&event](ConsoleBonusStage& stage) {
      if (ui::isMenuConfirmButton(event) || ui::isCancelButton(event))
      {
        stage.mDone = stage.mScreen.confirm();
      }
    },

    [&event](auto&& stage) { stage.handleEvent(event); });
}


std::unique_ptr<GameMode> GameSessionMode::updateAndRender(
  const engine::TimeDelta dt,
  const std::vector<SDL_Event>& events)
{
  using GameModePtr = std::unique_ptr<GameMode>;

  for (const auto& event : events)
  {
    handleEvent(event);
  }

  return base::match(
    mCurrentStage,
    [this, &dt](std::unique_ptr<GameRunner>& pIngameMode) -> GameModePtr {
      pIngameMode->updateAndRender(dt);

      if (pIngameMode->gameQuit())
      {
        if (mContext.mpUserProfile->mOptions.mConsoleMenus)
        {
          // The console-style menus return to the game selection when quitting
          mContext.mpServiceProvider->stopMusic();
          mContext.mpServiceProvider->fadeOutScreen();
          return std::make_unique<ConsoleLauncherMode>(mContext);
        }

        finishGameSession();
        return nullptr;
      }

      if (auto maybeSavedGame = pIngameMode->requestedGameToLoad())
      {
        mContext.mpServiceProvider->fadeOutScreen();
        return std::make_unique<GameSessionMode>(*maybeSavedGame, mContext);
      }

      if (auto maybeReplay = pIngameMode->requestedReplayToLoad())
      {
        mContext.mpServiceProvider->fadeOutScreen();
        return std::make_unique<GameSessionMode>(*maybeReplay, mContext);
      }

      if (pIngameMode->levelFinished())
      {
        auto achievedBonuses = pIngameMode->achievedBonuses();
        const auto availableBonuses = pIngameMode->availableBonuses();
        if (mContext.mpUserProfile->mOptions.mConsoleMenus)
        {
          // Only what the level offers can be earned (the original also paid
          // for categories with nothing in them)
          for (auto it = achievedBonuses.begin(); it != achievedBonuses.end();)
          {
            it = availableBonuses.count(*it) == 0 ? achievedBonuses.erase(it)
                                                  : std::next(it);
          }
        }
        const auto scoreWithoutBonuses = mPersistentPlayerState.score();

        data::addBonusScore(mPersistentPlayerState, achievedBonuses);

        if (data::isBossLevel(mCurrentLevelNr))
        {
          mContext.mpServiceProvider->playMusic("NEVRENDA.IMF");

          auto endScreens = ui::EpisodeEndSequence{
            mContext, mEpisode, achievedBonuses, scoreWithoutBonuses};
          mContext.mpServiceProvider->fadeOutScreen();
          mCurrentStage = std::move(endScreens);
        }
        else if (mContext.mpUserProfile->mOptions.mConsoleMenus)
        {
          mContext.mpServiceProvider->playMusic("OPNGATEA.IMF");

          using data::Bonus;
          static const std::pair<Bonus, const char*> CATEGORIES[] = {
            {Bonus::DestroyedAllCameras, "CAMERAS"},
            {Bonus::CollectedEveryWeapon, "WEAPONS"},
            {Bonus::CollectedAllMerchandise, "MERCHANDISE"},
            {Bonus::DestroyedAllSpinningLaserTurrets, "TURRETS"},
            {Bonus::DestroyedAllFireBombs, "NAPALM BOMBS"},
            {Bonus::ShotAllBonusGlobes, "CRYSTAL BALLS"},
            {Bonus::NoDamageTaken, "NO DAMAGE"}};
          std::vector<ui::ConsoleResultsScreen::Row> rows;
          for (const auto& [bonus, label] : CATEGORIES)
          {
            if (availableBonuses.count(bonus) > 0)
            {
              rows.push_back({label, achievedBonuses.count(bonus) > 0});
            }
          }

          const auto seconds = int(pIngameMode->levelSeconds());
          char time[16];
          if (seconds >= 0)
          {
            std::snprintf(
              time, sizeof(time), "%d:%02d", seconds / 60, seconds % 60);
          }
          else
          {
            std::snprintf(time, sizeof(time), "--:--");
          }

          auto* pServices = mContext.mpServiceProvider;
          ui::ConsoleResultsScreen::Sounds sounds;
          sounds.mRowEarned = [pServices] {
            pServices->playSound(data::SoundId::BigExplosion);
          };
          sounds.mScoreTick = [pServices] {
            pServices->playSound(data::SoundId::DukeJumping);
          };
          sounds.mTotal = [pServices] {
            pServices->playSound(data::SoundId::BigExplosion);
          };

          auto stage = ConsoleBonusStage{ui::ConsoleResultsScreen{
            "LEVEL " + std::to_string(mCurrentLevelNr + 1) + " COMPLETE",
            time,
            scoreWithoutBonuses,
            100000,
            std::move(rows),
            std::move(sounds)}};
          mCurrentStage = std::move(stage);
        }
        else
        {
          mContext.mpServiceProvider->playMusic("OPNGATEA.IMF");

          auto bonusScreen =
            ui::BonusScreen{mContext, achievedBonuses, scoreWithoutBonuses};

          fadeToNewStage(bonusScreen);
          mCurrentStage = std::move(bonusScreen);
        }
      }

      return nullptr;
    },

    [this, &dt](ui::BonusScreen& bonusScreen) -> GameModePtr {
      bonusScreen.updateAndRender(dt);

      if (bonusScreen.finished())
      {
        mPersistentPlayerState.resetForNewLevel();

        // The new level we are about to enter might have different
        // requirements w.r.t. low-res vs. hi-res mode (per-element upscaling).
        // The only way we can switch between these modes is with a game mode
        // switch at the top-level (Game class/main loop), so we need to
        // enter a new instance of GameSessionMode here even though we
        // technically remain in the same game mode. Otherwise, we would
        // potentially see brief glitches during the fade-in into the new
        // level.
        //
        // This feels like a bit of a hack, but I couldn't think of anything
        // else that wouldn't be massively more complicated.
        //
        // We can't use make_unique here, because the constructor is private.
        return std::unique_ptr<GameSessionMode>{new GameSessionMode{
          data::GameSessionId{mEpisode, ++mCurrentLevelNr, mDifficulty},
          mPersistentPlayerState,
          mContext}};
      }

      return nullptr;
    },

    [this, &dt](ConsoleBonusStage& stage) -> GameModePtr {
      stage.mScreen.update(dt);
      stage.mScreen.draw();

      if (stage.mDone)
      {
        mPersistentPlayerState.resetForNewLevel();
        return std::unique_ptr<GameSessionMode>{new GameSessionMode{
          data::GameSessionId{mEpisode, ++mCurrentLevelNr, mDifficulty},
          mPersistentPlayerState,
          mContext}};
      }

      return nullptr;
    },

    [this, &dt](ui::EpisodeEndSequence& endScreens) -> GameModePtr {
      endScreens.updateAndRender(dt);

      if (endScreens.finished())
      {
        finishGameSession();
      }

      return nullptr;
    },

    [this, &dt](HighScoreNameEntry& state) -> GameModePtr {
      mContext.mpScriptRunner->updateAndRender(dt);
      state.mNameEntryWidget.updateAndRender(dt);
      return nullptr;
    },

    [this, &dt](const HighScoreListDisplay&) -> GameModePtr {
      mContext.mpScriptRunner->updateAndRender(dt);
      ui::drawHighScoreList(mContext, mEpisode);

      if (mContext.mpScriptRunner->hasFinishedExecution())
      {
        mContext.mpServiceProvider->fadeOutScreen();
        return createMainMenuMode(mContext);
      }

      return nullptr;
    });
}


bool GameSessionMode::needsPerElementUpscaling() const
{
  return base::match(
    mCurrentStage,
    [this](const std::unique_ptr<GameRunner>& pIngameMode) {
      return pIngameMode->needsPerElementUpscaling();
    },

    [](auto&&) { return false; });
}


template <typename StageT>
void GameSessionMode::fadeToNewStage(StageT& stage)
{
  mContext.mpServiceProvider->fadeOutScreen();
  stage.updateAndRender(0);
  mContext.mpServiceProvider->fadeInScreen();
}


void GameSessionMode::finishGameSession()
{
  mContext.mpServiceProvider->stopMusic();
  mContext.mpServiceProvider->fadeOutScreen();

  const auto scoreQualifies = data::scoreQualifiesForHighScoreList(
    mPersistentPlayerState.score(),
    mContext.mpUserProfile->mHighScoreLists[mEpisode]);
  if (scoreQualifies)
  {
    SDL_StartTextInput();
    mCurrentStage = HighScoreNameEntry{ui::setupHighScoreNameEntry(mContext)};
  }
  else
  {
    ui::setupHighScoreListDisplay(mContext, mEpisode);
    mCurrentStage = HighScoreListDisplay{};
  }
}

void GameSessionMode::enterHighScore(std::string_view name)
{
  SDL_StopTextInput();

  data::insertNewScore(
    mPersistentPlayerState.score(),
    std::string{name},
    mContext.mpUserProfile->mHighScoreLists[mEpisode]);
  mContext.mpUserProfile->saveToDisk();

  ui::setupHighScoreListDisplay(mContext, mEpisode);
  mCurrentStage = HighScoreListDisplay{};
}

} // namespace rigel
