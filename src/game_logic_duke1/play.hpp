// Playable Duke 1 world: original map + backdrop + frame, driven by the
// deterministic core sandbox.
#pragma once

#include "assets/duke1_assets.hpp"
#include "core.hpp"

#include <array>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace rigel::duke1
{

constexpr int SCREEN_W = 320;
constexpr int SCREEN_H = 200;

class PlayWorld
{
public:
  // levelName without extension, e.g. "WORLDAL1". Throws std::runtime_error if
  // a required file is missing. context: backdrop pair to use for levels that
  // don't have their own (the shared intermission, WORLDAL2).
  PlayWorld(
    const std::filesystem::path& dataDir,
    const std::string& levelName,
    const std::array<std::string, 2>* context = nullptr,
    int episode = 0);

  // What carries over between levels
  struct Carry
  {
    int score = 0, health = 9, boost = 0, claw = 0, capacity = 1;
  };
  Carry carry() const;
  void applyCarry(const Carry& c);
  bool targetsCleared() const { return mTargetsCleared; }
  // End-of-level secret bonuses (10,000 each, paid when the level is left)
  enum Bonus
  {
    Cameras,
    AcmeSigns,
    Rockets,
    SnakeBots,
    BunnyBots,
    NoDamage,
    DukeLetters,
    NUM_BONUSES
  };
  struct BonusReport
  {
    bool present[NUM_BONUSES] = {}; // does this level have any of them at all
    bool earned[NUM_BONUSES] = {};
    int cameraDestroyed = 0;
    int cameraTotal = 0;
    int count() const
    {
      int n = 0;
      for (bool e : earned)
        n += e ? 1 : 0;
      return n;
    }
  };
  BonusReport bonusReport() const;
  const std::vector<int>& collectedLetters() const { return mLetters; }
  bool levelComplete() const { return mLevelComplete; }
  void debugFinishLevel() { mLevelComplete = true; }
  const std::array<std::string, 2>& backdropPair() const { return mPair; }
  static std::array<std::string, 2> backdropPairFor(
    const std::filesystem::path& dataDir,
    const std::string& levelName,
    int episode = 0);

  // One game update (the caller decides the cadence, see TickGate).
  void update(const Input& input);

  // Renders the 320x200 screen as RGBA, row-major, 4 bytes per pixel.
  void render(std::vector<std::uint8_t>& rgba) const;

  // Renders just the world (no frame) into a w x h RGBA buffer, with the world
  // pixel (left, top) at the buffer's top-left corner. Used for wide/modern
  // presentation; the simulation is unaffected by the view size.
  // playerX/playerY: world position of Duke's feet to draw (lets the caller
  // interpolate between updates).
  void renderWorld(
    std::vector<std::uint8_t>& rgba,
    int w,
    int h,
    int left,
    int top,
    int playerX,
    int playerY) const;

  const std::vector<std::string>& warnings() const { return mWarnings; }

  // World position of the player's feet in map pixels (the map is 2048x1440).
  int worldX() const
  {
    return (mSandbox.player.x8 + mSandbox.player.cameraX8) * 8;
  }
  int worldY() const
  {
    return mSandbox.player.y + mSandbox.player.cameraYWords / 8;
  }
  int score() const { return mScore; }

  // World position before the most recent update, for interpolation.
  int prevWorldX() const { return mPrevX; }
  int prevWorldY() const { return mPrevY; }
  // Wide presentation: let actors and shots stay active over the whole visible
  // area (world pixels).
  void setActiveView(int left, int top, int w, int h);
  // Modern presentation: actors stay active over a wider window around Duke.
  // This is part of the simulation (it is derived from the simulated position
  // only), so a replay must use the same value.
  void setWide(bool wide) { mWide = wide; }
  bool wide() const { return mWide; }
  void debugDump() const;
  // 0..1: how far the drawing is between the previous and the current update
  // (smooths actor movement)
  void setRenderBlend(double b) { mBlend = b; }
  bool dead() const { return mDyingTick >= 30; }
  int updateCount() const { return mUpdates; }
  int somersault() const
  {
    return mSandbox.player.jumpTravel;
  } // 0 none, 1 left, 2 right (debug/tests)
  // True once after something moved Duke instantly (transporter): the
  // presentation must not blend across it
  bool takeDiscontinuity() { return std::exchange(mDiscontinuity, false); }
  // Presentation-only interaction notifications, kept out of game physics.
  enum HintEvent : unsigned
  {
    DrinkHint = 1,
    ElevatorHint = 2,
    CrateHint = 1 << 2,
    ExitHint = 1 << 3,
    TeleporterHint = 1 << 4,
    KeyNeededHint = 1 << 5,
    CardNeededHint = 1 << 6,
    MachineNeededHint = 1 << 7,
    FoodHint = 1 << 8,
    TntHint = 1 << 9,
    FragileHint = 1 << 10,
    BalloonHint = 1 << 11,
    CameraHint = 1 << 12,
    FieldHint = 1 << 13,
    BeamHint = 1 << 14
  };
  unsigned takeHintEvents() { return std::exchange(mHintEvents, 0u); }
  // Sound requests (original sound indices 0-47) raised since the last call
  std::vector<int> takeSounds() { return std::exchange(mSounds, {}); }
  // Debug: everything Duke can pick up
  void grantAll()
  {
    for (auto& k : mKeys)
      k = true;
    mSandbox.player.boost = 1;
    mSandbox.player.claw = 1;
    mSandbox.player.capacity = 4;
    mHasItem = mHasCard = true;
  }
  // Inventory slots: 0-3 keys, 4 boots, 5 claw, 6 special item, 7 access card
  bool hasKey(int i) const
  {
    if (i < 4)
      return mKeys[i];
    if (i == 4)
      return mSandbox.player.boost != 0;
    if (i == 5)
      return mSandbox.player.claw != 0;
    return i == 6 ? mHasItem : mHasCard;
  }
  // 16x16 RGBA picture of a key (0 red, 1 green, 2 blue, 3 purple)
  const std::vector<std::uint8_t>& keyIcon(int i) const
  {
    static const int kPiece[4] = {10, 18, 63, 64};
    if (i >= 4)
      return mSprites[6 + kPiece[i - 4] / 50]
                     [static_cast<std::size_t>(kPiece[i - 4] % 50)];
    return mSprites[8][static_cast<std::size_t>(24 + i)];
  }
  // The four collectible letter sprites from the player-supplied OBJECT bank.
  const std::vector<std::uint8_t>& letterIcon(int i) const
  {
    return mSprites[8][static_cast<std::size_t>(18 + i)];
  }
  // 16x16 RGBA picture of the green wave that Duke's shots are drawn as
  const std::vector<std::uint8_t>& shotIcon() const { return mSprites[6][6]; }
  // 32x32 RGBA picture of Duke standing and facing right
  std::vector<std::uint8_t> standingPortrait() const;
  // Draws the collected keys into the given area of an RGBA frame (inventory)
  void drawInventory(std::vector<std::uint8_t>& out, int w, int h, int x, int y)
    const;
  void teleportToCell(
    int col,
    int row); // dev/test: place Duke like the start marker would
  const PlayerState& player() const { return mSandbox.player; }

private:
  friend struct PlayWorldTestAccess;

  struct Bank
  {
    std::vector<std::uint8_t> rgba; // 96x128, 6x8 tiles of 16x16
  };

  void blit(
    std::vector<std::uint8_t>& dst,
    int dx,
    int dy,
    const std::uint8_t* src,
    int srcStride,
    int sx,
    int sy,
    int w,
    int h,
    int clipX0,
    int clipY0,
    int clipX1,
    int clipY1,
    int dstW = SCREEN_W,
    int dstH = SCREEN_H) const;
  void drawTile(std::vector<std::uint8_t>& dst, const Tile& tile, int x, int y)
    const;
  void buildFrame();

  struct ActorInstance
  {
    std::uint16_t id;
    int col, row;
    bool alive = true;
    bool open = false; // containers: shot open, item exposed
    int hits = 1;
    std::uint16_t under = 0; // tile left behind in the map at load
    std::size_t cellIndex = 0;
    // Ported behaviour (see actors.cpp): original per-actor record, coordinates
    // as in the DOS game (x in 8 pixel units including the 2 unit frame offset,
    // y in map words).
    bool ported = false;
    int prevX8 = 0,
        prevYw = 0; // position before the latest update, for smooth drawing
    int state = 0, x8 = 0, yw = 0, v0 = 0, v1 = 0;
    int prevRow = -1; // plain boxes and items: row before the latest update
                      // (for smooth falling)
  };

  void updatePortedPart1(ActorInstance& a);
  void updatePortedPart2(ActorInstance& a);
  void updatePortedPart3(ActorInstance& a);
  void updatePortedPart4(ActorInstance& a);
  void updatePortedPart5(ActorInstance& a);
  void drawPortedPart1(
    std::vector<std::uint8_t>& out,
    int w,
    int h,
    int left,
    int top,
    const ActorInstance& a) const;
  void drawPortedPart2(
    std::vector<std::uint8_t>& out,
    int w,
    int h,
    int left,
    int top,
    const ActorInstance& a) const;
  void drawPortedPart3(
    std::vector<std::uint8_t>& out,
    int w,
    int h,
    int left,
    int top,
    const ActorInstance& a) const;
  void interact(); // bullets and touch against the actors

  // Actors whose behaviour has been ported from the original code (actors.cpp)
  void initPorted(ActorInstance& actor);
  void updatePorted();
  void
    drawPorted(std::vector<std::uint8_t>& out, int w, int h, int left, int top)
      const;
  bool wallAt(int x8, int yw) const; // original 3670: solid cell at (x, y)
  bool floorAt(int x8, int yw) const; // original 3706: solid cell(s) below
  bool
    offscreen(int yw, int x8) const; // original 3260: outside the active window
  int bulletHit(
    int yw,
    int x8); // original 354a: 1 = left bullet, 2 = right bullet, 0 = none
  bool touchesPlayer(int x8, int yw) const; // original 3505
  void drawPiece(
    std::vector<std::uint8_t>& out,
    int w,
    int h,
    int left,
    int top,
    int piece,
    int x8,
    int yw) const;
  // Shared pools, ported from the original effect updaters (explosions 2bcb,
  // debris 2ea3, enemy shots 1be8)
  struct Explosion
  {
    int frame, x8, yw, type;
  };
  struct Spark
  {
    int state;
    int x[4], y[4], piece[4], px[4], py[4];
  }; // four gems thrown out of a smashed object (313e)
  std::vector<Spark> mSparks;
  int mSparkVariant = 0;
  void spawnSparks(int yw, int x8, bool silent = false);
  struct Debris
  {
    int frame, x8, yw;
  };
  struct EnemyShot
  {
    int dir, x8, yw;
    bool fresh;
    int px8;
  };
  // The chain of nine orbs that snakes along a fixed path (loader marker 3013,
  // callback 6939)
  struct OrbChain
  {
    bool on = false;
    int x[10] = {}, y[10] = {}, px[10] = {}, py[10] = {}, frame[10] = {};
    bool live[10] = {};
    int step = 0, anim = 0;
  };
  OrbChain mOrbs;
  // The final boss (marker 3043): single instance
  struct Boss
  {
    int anim = 0, cnt = 0, step = 0, jumping = 0, dir = 0, period = 10,
        freeze = 0, deadCount = 0;
  };
  Boss mBoss;
  bool mFrozen = false;
  bool mHasItem = false, mHasCard = false, mBlast = false;
  int mDyingTick = 0;
  int mUpdates = 0;
  bool mWide = false;
  int mBonusTotals[5] =
    {}; // cameras, signs, rockets, snake chains, bunny bots in the level
  bool mDamageTaken = false;
  bool mDukeBonus = false;
  bool mSnakesDone = false;
  bool mUpConsumed =
    false; // the Up key press was used by an action and must be released first
  bool mDiscontinuity = false; // Duke or the camera jumped this update
  int mHitFlash = 0;
  bool mPullUpDrawn = false;
  std::vector<int> mSounds;
  unsigned mHintEvents = 0;
  void sound(int index) { mSounds.push_back(index); }
  void updateOrbs();
  std::vector<Explosion> mExplosions;
  std::vector<Debris> mDebris;
  std::vector<EnemyShot> mEnemyShots;
  int mShotAnim = 0; // 0648: toggles the enemy shot frame
  int mHoverAnim = 0, mHoverTick = 0; // 05a6 (0..3), 05a8 (0..9)
  bool mLastShotWasLeftFacing = false;
  void spawnExplosion(int yw, int x8, int type);
  void spawnDebris(int yw, int x8);
  void spawnEnemyShot(int yw, int x8, int dir);
  void updatePools();
  mutable int mShiftX = 0,
              mShiftY =
                0; // pixel offset applied while drawing an interpolated actor
  double mBlend = 1.0;
  void
    drawPools(std::vector<std::uint8_t>& out, int w, int h, int left, int top)
      const;
  void drawObjectPiece(
    std::vector<std::uint8_t>& out,
    int w,
    int h,
    int left,
    int top,
    int piece,
    int x8,
    int yw,
    int yOffsetPx = 0) const;
  int mFamAnim3010 = 0;
  bool mTargetsCleared = false;
  std::vector<int> mLetters; // D, U, K, E collected so far
  int mBallAnim = 0;
  bool mKeys[4] = {false, false, false, false}; // red, green, blue, purple
  int mControlAnim = 0;
  int mWalkerAnim = 0, mWalkerPhase = 0; // 300E: 05ae / 05b0
  int mFamAnimUp = 0,
      mFamAnimDown = 0x1e0; // flyers: 06e9 counts up, 06eb counts down

  int mEpisode = 0;
  Sandbox mSandbox;
  std::vector<ActorInstance> mActors;
  std::array<std::vector<std::vector<std::uint8_t>>, 9>
    mSprites; // ANIM0-5, OBJECT0-2: RGBA per 16x16 piece
  unsigned mPhase = 0;
  struct Popup
  {
    int x, y, value, age;
  };
  std::vector<Popup>
    mPopups; // little score sprites that rise from collected items
  std::vector<std::vector<std::uint8_t>> mNumbers; // NUMBERS.DN1 pieces as RGBA
  void addScore(int value, int x, int y);
  int mScore = 0;
  bool mLevelComplete = false;
  int mExitState = 0;
  std::array<std::string, 2> mPair{};
  int mPrevX = 0, mPrevY = 0;
  int mAirTicks =
    0; // updates since Duke left the ground, drives the somersault frames
  std::array<Bank, 8> mBanks;
  std::array<std::vector<std::uint8_t>, 2> mBackdrops; // 208x160 RGBA each
  std::array<Tileset, 5> mMan;
  Tileset mBorder;
  std::vector<std::uint8_t> mFrame; // pre-rendered chrome, 320x200 RGBA
  std::vector<std::string> mWarnings;
};

} // namespace rigel::duke1
