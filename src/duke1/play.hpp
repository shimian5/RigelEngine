// Playable Duke 1 world: original map + backdrop + frame, driven by the
// deterministic core sandbox.
#pragma once

#include "assets.hpp"
#include "core.hpp"

#include <array>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace duke1
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
  bool targetsCleared() const { return targetsCleared_; }
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
    int count() const
    {
      int n = 0;
      for (bool e : earned)
        n += e ? 1 : 0;
      return n;
    }
  };
  BonusReport bonusReport() const;
  bool levelComplete() const { return levelComplete_; }
  void debugFinishLevel() { levelComplete_ = true; }
  const std::array<std::string, 2>& backdropPair() const { return pair_; }
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

  const std::vector<std::string>& warnings() const { return warnings_; }

  // World position of the player's feet in map pixels (the map is 2048x1440).
  int worldX() const
  {
    return (sandbox_.player.x8 + sandbox_.player.cameraX8) * 8;
  }
  int worldY() const
  {
    return sandbox_.player.y + sandbox_.player.cameraYWords / 8;
  }
  int score() const { return score_; }

  // World position before the most recent update, for interpolation.
  int prevWorldX() const { return prevX_; }
  int prevWorldY() const { return prevY_; }
  // Wide presentation: let actors and shots stay active over the whole visible
  // area (world pixels).
  void setActiveView(int left, int top, int w, int h);
  // Modern presentation: actors stay active over a wider window around Duke.
  // This is part of the simulation (it is derived from the simulated position
  // only), so a replay must use the same value.
  void setWide(bool wide) { wide_ = wide; }
  bool wide() const { return wide_; }
  void debugDump() const;
  // 0..1: how far the drawing is between the previous and the current update
  // (smooths actor movement)
  void setRenderBlend(double b) { blend_ = b; }
  bool dead() const { return dyingTick_ >= 30; }
  int updateCount() const { return updates_; }
  int somersault() const
  {
    return sandbox_.player.jumpTravel;
  } // 0 none, 1 left, 2 right (debug/tests)
  // True once after something moved Duke instantly (transporter): the
  // presentation must not blend across it
  bool takeDiscontinuity() { return std::exchange(discontinuity_, false); }
  // Sound requests (original sound indices 0-47) raised since the last call
  std::vector<int> takeSounds() { return std::exchange(sounds_, {}); }
  // Debug: everything Duke can pick up
  void grantAll()
  {
    for (auto& k : keys_)
      k = true;
    sandbox_.player.boost = 1;
    sandbox_.player.claw = 1;
    sandbox_.player.capacity = 4;
    hasItem_ = hasCard_ = true;
  }
  // Inventory slots: 0-3 keys, 4 boots, 5 claw, 6 special item, 7 access card
  bool hasKey(int i) const
  {
    if (i < 4)
      return keys_[i];
    if (i == 4)
      return sandbox_.player.boost != 0;
    if (i == 5)
      return sandbox_.player.claw != 0;
    return i == 6 ? hasItem_ : hasCard_;
  }
  // 16x16 RGBA picture of a key (0 red, 1 green, 2 blue, 3 purple)
  const std::vector<std::uint8_t>& keyIcon(int i) const
  {
    static const int kPiece[4] = {10, 18, 63, 64};
    if (i >= 4)
      return sprites_[6 + kPiece[i - 4] / 50]
                     [static_cast<std::size_t>(kPiece[i - 4] % 50)];
    return sprites_[8][static_cast<std::size_t>(24 + i)];
  }
  // Draws the collected keys into the given area of an RGBA frame (inventory)
  void drawInventory(std::vector<std::uint8_t>& out, int w, int h, int x, int y)
    const;
  void teleportToCell(
    int col,
    int row); // dev/test: place Duke like the start marker would
  const PlayerState& player() const { return sandbox_.player; }

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
  std::vector<Spark> sparks_;
  int sparkVariant_ = 0;
  void spawnSparks(int yw, int x8);
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
  OrbChain orbs_;
  // The final boss (marker 3043): single instance
  struct Boss
  {
    int anim = 0, cnt = 0, step = 0, jumping = 0, dir = 0, period = 10,
        freeze = 0, deadCount = 0;
  };
  Boss boss_;
  bool frozen_ = false;
  bool hasItem_ = false, hasCard_ = false, blast_ = false;
  int dyingTick_ = 0;
  int updates_ = 0;
  bool wide_ = false;
  int bonusTotals_[5] =
    {}; // cameras, signs, rockets, snake chains, bunny bots in the level
  bool damageTaken_ = false;
  bool dukeBonus_ = false;
  bool snakesDone_ = false;
  bool upConsumed_ =
    false; // the Up key press was used by an action and must be released first
  bool discontinuity_ = false; // Duke or the camera jumped this update
  int hitFlash_ = 0;
  bool pullUpDrawn_ = false;
  std::vector<int> sounds_;
  void sound(int index) { sounds_.push_back(index); }
  void updateOrbs();
  std::vector<Explosion> explosions_;
  std::vector<Debris> debris_;
  std::vector<EnemyShot> enemyShots_;
  int shotAnim_ = 0; // 0648: toggles the enemy shot frame
  int hoverAnim_ = 0, hoverTick_ = 0; // 05a6 (0..3), 05a8 (0..9)
  bool lastShotWasLeftFacing_ = false;
  void spawnExplosion(int yw, int x8, int type);
  void spawnDebris(int yw, int x8);
  void spawnEnemyShot(int yw, int x8, int dir);
  void updatePools();
  mutable int shiftX_ = 0,
              shiftY_ =
                0; // pixel offset applied while drawing an interpolated actor
  double blend_ = 1.0;
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
  int famAnim3010_ = 0;
  bool targetsCleared_ = false;
  std::vector<int> letters_; // D, U, K, E collected so far
  int ballAnim_ = 0;
  bool keys_[4] = {false, false, false, false}; // red, green, blue, purple
  int controlAnim_ = 0;
  int walkerAnim_ = 0, walkerPhase_ = 0; // 300E: 05ae / 05b0
  int famAnimUp_ = 0,
      famAnimDown_ = 0x1e0; // flyers: 06e9 counts up, 06eb counts down

  int episode_ = 0;
  Sandbox sandbox_;
  std::vector<ActorInstance> actors_;
  std::array<std::vector<std::vector<std::uint8_t>>, 9>
    sprites_; // ANIM0-5, OBJECT0-2: RGBA per 16x16 piece
  unsigned phase_ = 0;
  struct Popup
  {
    int x, y, value, age;
  };
  std::vector<Popup>
    popups_; // little score sprites that rise from collected items
  std::vector<std::vector<std::uint8_t>> numbers_; // NUMBERS.DN1 pieces as RGBA
  void addScore(int value, int x, int y);
  int score_ = 0;
  bool levelComplete_ = false;
  int exitState_ = 0;
  std::array<std::string, 2> pair_{};
  int prevX_ = 0, prevY_ = 0;
  int airTicks_ =
    0; // updates since Duke left the ground, drives the somersault frames
  std::array<Bank, 8> banks_;
  std::array<std::vector<std::uint8_t>, 2> backdrops_; // 208x160 RGBA each
  std::array<Tileset, 5> man_;
  Tileset border_;
  std::vector<std::uint8_t> frame_; // pre-rendered chrome, 320x200 RGBA
  std::vector<std::string> warnings_;
};

} // namespace duke1
