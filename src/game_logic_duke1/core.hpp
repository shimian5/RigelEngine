// Duke Nukem 1 player, collision and projectile core, following the original
// program's integer coordinate rules.
#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
namespace duke1
{
struct Input
{
  bool left = false, right = false, up = false, down = false, jump = false,
       fire = false;
};
struct Event
{
  enum Kind
  {
    Sound,
    MapContact,
    Score,
    Break
  } kind;
  int value;
};
struct PlayerState
{
  int x8 = 16, y = 96, cameraX8 = 0, cameraYWords = 0;
  bool grounded = true, falling = false, cling = false, jumpLatch = false,
       fireLatch = false;
  int jumpIndex = 0, facing = 4, walkFrame = 0, pullUp = 0, boost = 0, claw = 0;
  bool verticalScroll = false;
  int cameraLock = 0, exitAnimation = 0, death = 0;
  int health = 9, immunity = 0;
  bool pendingHit = false;
  int shotBlock = 0, capacity = 1;
  int jumpTravel = 0, jumpTravelAge = 0, phase = 0;
};
struct Bullet
{
  int kind = 0, x8 = 0, yWords = 0, newFlag = 0;
};
enum class Sensor
{
  Left,
  Right,
  Head,
  Floor
};
class CollisionWorld
{
public:
  explicit CollisionWorld(std::uint16_t fill = 0);
  std::array<std::uint16_t, 128 * 90> cells{};
  bool clear(Sensor sensor, PlayerState& p, std::vector<Event>& events) const;
  // Outside the researched map is an explicit solid sandbox boundary policy.
  std::uint16_t at(int index) const;
};
class Sandbox
{
public:
  PlayerState player;
  Input input;
  std::array<Bullet, 4> bullets{};
  std::vector<Event> events;
  // Optional controlled query provider for matching the original substituted
  // probes.
  std::function<bool(Sensor)> query;
  CollisionWorld world;
  void movement();
  // Stage1 pre-render boundary; no automatic cameraY/draw/bullet update.
  void lateUpdate();
  // Resolved pull-up/fire/damage subset; no actor callbacks.
  void tick();
  void cameraY();
  void damage(bool drawAfter = false);
  int drawPlayer();
  // Returns visible quadrant count; changes immunity as original draw does.
  bool spawn(int facing, int yWords, int x8);
  void moveBullets(bool checkWorld = true, bool cull = true);
  bool culled(int x8, int yWords) const;
  // Optional larger active window for wide presentation (union with the
  // original 13x10 tile window)
  bool wideCull = false;
  int cullX0 = 0, cullX1 = 0, cullY0 = 0, cullY1 = 0;

private:
  bool clear(Sensor s);
  void contact();
  void alignY();
};
class TickGate
{
public:
  static constexpr std::uint32_t pitDivisor = 0x2051;
  explicit TickGate(int speed = 0);
  // Caller supplies integer DOS timer callbacks. At most one update; remainder
  // discarded.
  bool callbacks(std::uint64_t count);
  std::uint64_t pending() const { return pending_; }
  int speed() const { return speed_; }

private:
  int speed_;
  std::uint64_t pending_ = 0;
};
} // namespace duke1
