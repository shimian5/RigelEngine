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

#include "game_runner.hpp"

#include "base/math_utils.hpp"
#include "frontend/game_service_provider.hpp"
#include "frontend/user_profile.hpp"
#include "game_logic/game_world.hpp"
#include "game_logic_classic/game_world_classic.hpp"
#include "ui/utils.hpp"

#include <loguru.hpp>

#ifdef __ANDROID__
#include <SDL_log.h>
#endif

#include <cstdlib>
#include <sstream>


namespace rigel
{

namespace
{

template <typename... Args>
std::unique_ptr<game_logic::IGameWorld>
  createGameWorld(const data::GameplayStyle gameplayStyle, Args&&... args)
{
  if (gameplayStyle == data::GameplayStyle::Classic)
  {
    return std::make_unique<game_logic::GameWorld_Classic>(
      std::forward<Args>(args)...);
  }
  else
  {
    return std::make_unique<game_logic::GameWorld>(std::forward<Args>(args)...);
  }
}

} // namespace


/** Forwards to the real service provider, but can silence audio and fades
 *
 * Used while fast-forwarding through a replay save, where the game logic runs
 * without anything being shown or heard.
 */
class GameRunner::FastForwardServiceProvider : public IGameServiceProvider
{
public:
  FastForwardServiceProvider(IGameServiceProvider* pInner, const bool muted)
    : mpInner(pInner)
    , mMuted(muted)
  {
  }

  void setMuted(const bool muted) { mMuted = muted; }

  void fadeOutScreen() override
  {
    if (!mMuted)
    {
      mpInner->fadeOutScreen();
    }
  }

  void fadeInScreen() override
  {
    if (!mMuted)
    {
      mpInner->fadeInScreen();
    }
  }

  void playSound(const data::SoundId id) override
  {
    if (!mMuted)
    {
      mpInner->playSound(id);
    }
  }

  void stopSound(const data::SoundId id) override
  {
    if (!mMuted)
    {
      mpInner->stopSound(id);
    }
  }

  void stopAllSounds() override
  {
    if (!mMuted)
    {
      mpInner->stopAllSounds();
    }
  }

  void playMusic(const std::string& name) override { mpInner->playMusic(name); }
  void stopMusic() override { mpInner->stopMusic(); }
  void scheduleGameQuit() override { mpInner->scheduleGameQuit(); }

  void switchGamePath(const std::filesystem::path& newGamePath) override
  {
    mpInner->switchGamePath(newGamePath);
  }

  void markCurrentFrameAsWidescreen() override
  {
    mpInner->markCurrentFrameAsWidescreen();
  }

  void setNativeOverlay(std::function<void(int)> draw) override
  {
    mpInner->setNativeOverlay(std::move(draw));
  }

  bool isSharewareVersion() const override
  {
    return mpInner->isSharewareVersion();
  }

  const CommandLineOptions& commandLineOptions() const override
  {
    return mpInner->commandLineOptions();
  }

  const GameControllerInfo& gameControllerInfo() const override
  {
    return mpInner->gameControllerInfo();
  }

private:
  IGameServiceProvider* mpInner;
  bool mMuted;
};


GameRunner::GameRunner(
  data::PersistentPlayerState* pPersistentPlayerState,
  const data::GameSessionId& sessionId,
  GameMode::Context context,
  const std::optional<base::Vec2> playerPositionOverride,
  const bool showWelcomeMessage)
  : GameRunner(
      pPersistentPlayerState,
      sessionId,
      context,
      playerPositionOverride,
      showWelcomeMessage,
      nullptr)
{
}


GameRunner::GameRunner(
  data::PersistentPlayerState* pPersistentPlayerState,
  const ReplaySave& replay,
  GameMode::Context context)
  : GameRunner(
      pPersistentPlayerState,
      replay.mSessionId,
      context,
      replay.mPlayerPositionOverride,
      replay.mShowWelcomeMessage,
      &replay)
{
}


GameRunner::GameRunner(
  data::PersistentPlayerState* pPersistentPlayerState,
  const data::GameSessionId& sessionId,
  GameMode::Context context,
  const std::optional<base::Vec2> playerPositionOverride,
  const bool showWelcomeMessage,
  const ReplaySave* pReplay)
  : mContext(context)
  , mpPlayerState(pPersistentPlayerState)
  , mPlayerStateAtStart(*pPersistentPlayerState)
  , mSessionId(sessionId)
  , mPlayerPositionOverride(playerPositionOverride)
  , mShowWelcomeMessage(showWelcomeMessage)
  , mpServiceProviderProxy(std::make_unique<FastForwardServiceProvider>(
      context.mpServiceProvider,
      pReplay != nullptr))
  , mpWorld(createGameWorld(
      context.mpUserProfile->mOptions.mGameplayStyle,
      pPersistentPlayerState,
      sessionId,
      std::invoke([&]() {
        // Only route through the proxy when resuming from a replay, normal
        // gameplay talks to the real provider directly.
        auto worldContext = context;
        if (pReplay)
        {
          worldContext.mpServiceProvider = mpServiceProviderProxy.get();
        }
        return worldContext;
      }),
      playerPositionOverride,
      showWelcomeMessage))
  , mInputHandler(&context.mpUserProfile->mOptions)
  , mMenu(context, pPersistentPlayerState, mpWorld.get(), sessionId)
  , mPauseMenu(context)
{
  const auto& options = context.mpUserProfile->mOptions;

  if (pReplay)
  {
    fastForward(*pReplay);
  }
  else
  {
    if (const auto* pSelfTest = std::getenv("RIGEL_REPLAY_SELFTEST"))
    {
      mSelfTestTicks = std::atoi(pSelfTest);
    }

#ifdef RIGEL_DEV_SCREENSHOT
    // Development hooks for testing disk saves without a keyboard:
    //   RIGEL_DEV_SCRIPTED_INPUT=1     use scripted input
    //   RIGEL_DEV_SAVE_AT_TICK=<n>     write quicksave at tick n, then exit
    //   RIGEL_DEV_LOAD_SAVE=<slot>     load the given slot right away
    if (std::getenv("RIGEL_DEV_SCRIPTED_INPUT"))
    {
      mSelfTestTicks = -1;
    }
    if (std::getenv("RIGEL_DEV_OPEN_PAUSE"))
    {
      openPauseMenu();
    }
    if (const auto* pSaveAt = std::getenv("RIGEL_DEV_SAVE_AT_TICK"))
    {
      mDevSaveAtTick = std::atoi(pSaveAt);
    }
    if (const auto* pLoad = std::getenv("RIGEL_DEV_LOAD_SAVE"))
    {
      mRequestedReplay = readReplaySaveFile(pLoad);
      LOG_F(INFO, "DEV: loading save '%s': %s", pLoad, mRequestedReplay ? "found" : "NOT FOUND");
    }
#endif

    if (!sessionId.mIsDemo && options.mQuickSavingEnabled)
    {
      // Automatic save at the start of each level (or checkpoint restart)
      writeDiskSave("autosave");
    }
  }
}


GameRunner::~GameRunner() = default;


void GameRunner::fastForward(const ReplaySave& replay)
{
  LOG_F(INFO, "Resuming from replay save: %zu ticks", replay.mInputs.size());

  mpWorld->setSimulationViewportOverride(replay.mSimulationViewport);

  for (const auto packed : replay.mInputs)
  {
    mpWorld->updateGameLogic(unpackPlayerInput(packed));
    mpWorld->processEndOfFrameActions();
  }

  mpWorld->setSimulationViewportOverride(std::nullopt);
  mInputLog = replay.mInputs;
  mpServiceProviderProxy->setMuted(false);

  if (replay.mStateHash != 0)
  {
    const auto hash = mpWorld->debugStateHash();
    if (hash == replay.mStateHash)
    {
      LOG_F(INFO, "Replay save verified: state matches");
#ifdef __ANDROID__
      SDL_Log("RIGEL replay save verified (%zu ticks)", replay.mInputs.size());
#endif
    }
    else
    {
      LOG_F(
        WARNING,
        "Replay save diverged: expected state %016llx, got %016llx",
        static_cast<unsigned long long>(replay.mStateHash),
        static_cast<unsigned long long>(hash));
#ifdef __ANDROID__
      SDL_Log("RIGEL replay save DIVERGED");
#endif
    }
  }
}


ReplaySave GameRunner::makeReplaySave() const
{
  ReplaySave save;
  save.mSessionId = mSessionId;
  save.mPlayerStateAtStart = mPlayerStateAtStart;
  save.mPlayerPositionOverride = mPlayerPositionOverride;
  save.mShowWelcomeMessage = mShowWelcomeMessage;
  save.mGameplayStyle =
    static_cast<std::uint8_t>(mContext.mpUserProfile->mOptions.mGameplayStyle);
  save.mSimulationViewport = mpWorld->simulationViewport();
  save.mInputs = mInputLog;
  save.mScore = mpPlayerState->score();
  save.mHealth = mpPlayerState->health();
  save.mStateHash = mpWorld->debugStateHash();
  return save;
}


void GameRunner::writeDiskSave(const char* slotName) const
{
  writeReplaySaveFile(slotName, makeReplaySave());
}


void GameRunner::doQuickSave(const bool alwaysWriteToDisk)
{
  if (mpPlayerState->isDead())
  {
    return;
  }

  mpWorld->quickSave();

  // Also keep a copy on disk, so it survives quitting the game
  if (alwaysWriteToDisk || mpWorld->canQuickLoad())
  {
    writeDiskSave("quicksave");
  }
}


void GameRunner::doQuickLoad(const bool allowDiskWithoutOption)
{
  if (mpWorld->canQuickLoad())
  {
    mpWorld->quickLoad();
  }
  else if (
    allowDiskWithoutOption ||
    mContext.mpUserProfile->mOptions.mQuickSavingEnabled)
  {
    // Nothing in memory (e.g. the game was restarted): use the disk save
    if (auto oSave = readReplaySaveFile("quicksave"))
    {
      mRequestedReplay = std::move(oSave);
    }
  }
}


void GameRunner::loadNewestSave()
{
  const auto oName = newestSaveSlotName();
  if (!oName)
  {
    return;
  }

  if (*oName == "quicksave" && mpWorld->canQuickLoad())
  {
    mpWorld->quickLoad();
  }
  else if (auto oSave = readReplaySaveFile(*oName))
  {
    mRequestedReplay = std::move(oSave);
  }
}


void GameRunner::openPauseMenu()
{
  mInputHandler.reset();

  ui::PauseMenu::Info info;
  info.mEpisode = mSessionId.mEpisode + 1;
  info.mLevel = mSessionId.mLevel + 1;
  info.mScore = mpPlayerState->score();
  info.mPlayTimeSeconds = double(mInputLog.size()) / 15.0;
  mPauseMenu.open(info);
}


void GameRunner::executePauseMenuAction()
{
  using Action = ui::PauseMenu::Action;

  int slot = 0;
  switch (mPauseMenu.takeAction(&slot))
  {
    case Action::QuickSave:
      doQuickSave(true);
      break;

    case Action::LoadLastSave:
      loadNewestSave();
      break;

    case Action::SaveToSlot:
      if (!mpPlayerState->isDead())
      {
        writeReplaySaveFile(consoleSlotName(slot), makeReplaySave());
      }
      break;

    case Action::LoadFromSlot:
      if (auto oSave = readReplaySaveFile(consoleSlotName(slot)))
      {
        mRequestedReplay = std::move(oSave);
      }
      break;

    case Action::Quit:
      mQuitRequestedFromPauseMenu = true;
      break;

    case Action::Resume:
    case Action::None:
      break;
  }
}


void GameRunner::runReplaySelfTest()
{
  const auto save = makeReplaySave();
  const auto expectedHash = mpWorld->debugStateHash();

  data::PersistentPlayerState replayedState = save.mPlayerStateAtStart;
  GameRunner replayed{&replayedState, save, mContext};
  const auto replayedHash = replayed.mpWorld->debugStateHash();

  // Also verify the file format round trips
  const auto bytes = serializeReplaySave(save);
  const auto oRoundTripped = deserializeReplaySave(bytes);

  LOG_F(
    INFO,
    "REPLAY SELFTEST: ticks=%zu live=%016llx replayed=%016llx score %d/%d "
    "fileBytes=%zu roundTrip=%s => %s",
    save.mInputs.size(),
    static_cast<unsigned long long>(expectedHash),
    static_cast<unsigned long long>(replayedHash),
    mpPlayerState->score(),
    replayedState.score(),
    bytes.size(),
    (oRoundTripped && oRoundTripped->mInputs == save.mInputs) ? "ok" : "BAD",
    expectedHash == replayedHash ? "PASS" : "FAIL");
}


void GameRunner::handleEvent(const SDL_Event& event)
{
  if (gameQuit() || requestedGameToLoad() || mRequestedReplay)
  {
    return;
  }

  if (mContext.mpUserProfile->mOptions.mConsoleMenus)
  {
    if (mPauseMenu.isActive())
    {
      mPauseMenu.handleEvent(event);
      executePauseMenuAction();
      return;
    }

    const auto command =
      mInputHandler.handleEvent(event, mpWorld->isPlayerInShip());
    if (command == InputHandler::MenuCommand::QuickSave)
    {
      doQuickSave(false);
    }
    else if (command == InputHandler::MenuCommand::QuickLoad)
    {
      doQuickLoad(false);
    }
    else if (ui::PauseMenu::isOpenTrigger(event))
    {
      openPauseMenu();
    }

    if (mContext.mpServiceProvider->commandLineOptions().mDebugModeEnabled)
    {
      handleDebugKeys(event);
    }
    return;
  }

  mMenu.handleEvent(event);
  if (mMenu.isActive())
  {
    // The menu overrides game event handling when it is active, therefore
    // stop here
    return;
  }

  const auto menuCommand =
    mInputHandler.handleEvent(event, mpWorld->isPlayerInShip());

  switch (menuCommand)
  {
    case InputHandler::MenuCommand::QuickSave:
      doQuickSave(false);
      break;

    case InputHandler::MenuCommand::QuickLoad:
      doQuickLoad(false);
      break;

    default:
      break;
  }

  const auto debugModeEnabled =
    mContext.mpServiceProvider->commandLineOptions().mDebugModeEnabled;
  if (debugModeEnabled)
  {
    handleDebugKeys(event);
  }
}


void GameRunner::updateAndRender(engine::TimeDelta dt)
{
  if (gameQuit() || levelFinished() || requestedGameToLoad() || mRequestedReplay)
  {
    // TODO: This is a workaround to make the fadeout on quitting work.
    // Would be good to find a better way to do this.
    mpWorld->render(interpolationFactor(dt));
    return;
  }

  if (updateMenu(dt))
  {
    return;
  }

  updateWorld(dt);
  mpWorld->render(interpolationFactor(dt));

  renderDebugText();
  mpWorld->processEndOfFrameActions();
}


bool GameRunner::needsPerElementUpscaling() const
{
  return mpWorld->needsPerElementUpscaling();
}


float GameRunner::interpolationFactor(const engine::TimeDelta dt) const
{
  return mContext.mpUserProfile->mOptions.mMotionSmoothing
    ? static_cast<float>(mAccumulatedTime / game_logic::GAME_LOGIC_UPDATE_DELAY)
    : 1.0f;
}


void GameRunner::updateWorld(const engine::TimeDelta dt)
{
  auto update = [this]() {
    game_logic::PlayerInput input;
    if (mSelfTestTicks != 0)
    {
      // Scripted pseudo-random input for the determinism self test
      const auto n = std::uint32_t(mInputLog.size());
      const auto phase = (n * 2654435761u) >> 28;
      input.mRight = (n / 40) % 3 != 2;
      input.mLeft = !input.mRight && (n % 7) < 3;
      input.mJump.mIsPressed = (phase % 5) == 0;
      input.mJump.mWasTriggered = input.mJump.mIsPressed && (n % 3) == 0;
      input.mFire.mIsPressed = (phase % 3) == 0;
      input.mFire.mWasTriggered = input.mFire.mIsPressed && (n % 2) == 0;
      input.mUp = (phase % 11) == 0;
    }
    else
    {
      input = mInputHandler.fetchInput();
    }

    mInputLog.push_back(packPlayerInput(input));
    mpWorld->updateGameLogic(input);

    if (mSelfTestTicks > 0 && int(mInputLog.size()) == mSelfTestTicks)
    {
      runReplaySelfTest();
      mSelfTestTicks = -1; // done
    }

    if (mDevSaveAtTick > 0 && int(mInputLog.size()) == mDevSaveAtTick)
    {
      writeDiskSave("quicksave");
      LOG_F(INFO, "DEV: wrote quicksave at tick %d, exiting", mDevSaveAtTick);
      std::_Exit(0);
    }
  };


  if (mSingleStepping)
  {
    if (mDoNextSingleStep)
    {
      update();
      mDoNextSingleStep = false;
    }
  }
  else
  {
    mAccumulatedTime += dt;

    // Unusually long delta time - most likely, the game was paused in the
    // debugger, or something else happened (computer put to sleep?)
    // Ignore this delta time.
    if (mAccumulatedTime > 0.25)
    {
      mAccumulatedTime = 0.0;
    }

    if (mAccumulatedTime >= game_logic::GAME_LOGIC_UPDATE_DELAY)
    {
      update();
      mAccumulatedTime -= game_logic::GAME_LOGIC_UPDATE_DELAY;
    }

    mpWorld->updateBackdropAutoScrolling(dt);
  }
}


bool GameRunner::updateMenu(const engine::TimeDelta dt)
{
  if (mPauseMenu.isActive())
  {
    mInputHandler.reset();
    mpWorld->render(interpolationFactor(dt));
    mPauseMenu.updateAndRender(dt);
    return true;
  }

  if (mMenu.isActive())
  {
    mInputHandler.reset();

    if (mMenu.isTransparent())
    {
      mpWorld->render(interpolationFactor(dt));
    }

    const auto result = mMenu.updateAndRender(dt);

    if (result == ui::IngameMenu::UpdateResult::FinishedNeedsFadeout)
    {
      mContext.mpServiceProvider->fadeOutScreen();
      mpWorld->render(interpolationFactor(dt));
      mContext.mpServiceProvider->fadeInScreen();
    }

    return true;
  }

  return false;
}


void GameRunner::handleDebugKeys(const SDL_Event& event)
{
  if (event.type != SDL_KEYDOWN || event.key.repeat != 0)
  {
    return;
  }

  switch (event.key.keysym.sym)
  {
    case SDLK_b:
      mpWorld->debugToggleBoundingBoxDisplay();
      break;

    case SDLK_c:
      mpWorld->debugToggleWorldCollisionDataDisplay();
      break;

    case SDLK_d:
      mShowDebugText = !mShowDebugText;
      break;

    case SDLK_g:
      mpWorld->debugToggleGridDisplay();
      break;

    case SDLK_s:
      mSingleStepping = !mSingleStepping;
      break;

    case SDLK_SPACE:
      if (mSingleStepping)
      {
        mDoNextSingleStep = true;
      }
      break;

    case SDLK_F10:
      mpWorld->toggleGodMode();
      break;

    case SDLK_F11:
      mLevelFinishedByDebugKey = true;
      break;
  }
}


void GameRunner::renderDebugText()
{
  std::stringstream debugText;

  if (mpWorld->isGodModeOn())
  {
    debugText << "GOD MODE on\n";
  }

  if (mShowDebugText)
  {
    mpWorld->printDebugText(debugText);
  }

  ui::drawText(debugText.str(), 0, 32, {255, 255, 255, 255});
}


bool GameRunner::levelFinished() const
{
  return mpWorld->levelFinished() || mLevelFinishedByDebugKey;
}


bool GameRunner::gameQuit() const
{
  return mMenu.quitRequested() || mQuitRequestedFromPauseMenu;
}


std::optional<data::SavedGame> GameRunner::requestedGameToLoad() const
{
  return mMenu.requestedGameToLoad();
}


std::set<data::Bonus> GameRunner::achievedBonuses() const
{
  return mpWorld->achievedBonuses();
}


std::set<data::Bonus> GameRunner::availableBonuses() const
{
  return mpWorld->availableBonuses();
}


double GameRunner::levelSeconds() const
{
  return mpWorld->levelSeconds();
}

} // namespace rigel
