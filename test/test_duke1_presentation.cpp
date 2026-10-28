#include "duke1_frontend_fixture.hpp"

#include <algorithm>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

namespace rigel::duke1
{
struct PlayWorldTestAccess
{
  static Sandbox& sandbox(PlayWorld& world) { return world.mSandbox; }
  static int columnContact(
    PlayWorld& world,
    int dx,
    int feetOffset,
    bool immunity,
    bool disabled)
  {
    auto& p = world.mSandbox.player;
    p.health = 9;
    p.x8 = 14 + dx;
    p.cameraX8 = 28;
    p.y = 64 + feetOffset;
    p.cameraYWords = 2048;
    p.immunity = immunity ? 21 : 0;
    PlayWorld::ActorInstance column{};
    column.id = 0x302B;
    column.alive = column.ported = true;
    column.state = 1;
    column.x8 = 42;
    column.yw = 2560;
    column.hits = 10;
    column.v0 = disabled ? 1 : 0;
    world.updatePortedPart2(column);
    return p.health;
  }
  static void keyDoor(PlayWorld& world, int color)
  {
    PlayWorld::ActorInstance reader{};
    reader.id = static_cast<std::uint16_t>(0x3048 + color);
    reader.alive = reader.ported = true;
    reader.state = 1;
    reader.x8 = 16;
    reader.yw = 5 * 128;
    reader.cellIndex = 5 * 128 + 7;
    world.mActors.push_back(reader);
    auto door = reader;
    door.id = static_cast<std::uint16_t>(0x304C + color);
    door.x8 = 24;
    door.cellIndex = 5 * 128 + 11;
    world.mActors.push_back(door);
    world.mSandbox.world.cells[door.cellIndex] = 0x1820;
  }
  static void grantKey(PlayWorld& world, int color)
  {
    world.mKeys[color] = true;
  }
  static bool opening(const PlayWorld& world)
  {
    return world.mActors[1].v0 > 0;
  }
  static bool hasKey(const PlayWorld& world, int color)
  {
    return world.mKeys[color];
  }
  static void primedTnt(PlayWorld& world)
  {
    world.mSandbox.player.x8 = 2;
    PlayWorld::ActorInstance actor{};
    actor.id = 0x3012;
    actor.alive = actor.ported = true;
    actor.state = 13;
    actor.x8 = 22;
    actor.yw = 5 * 128;
    actor.col = 10;
    actor.row = 5;
    actor.cellIndex = 5 * 128 + 10;
    actor.v0 = 1;
    actor.v1 = 3;
    world.mActors.push_back(actor);
  }
  static std::vector<int> hazardSounds(PlayWorld& world, int id, int state)
  {
    PlayWorld::ActorInstance actor{};
    actor.id = static_cast<std::uint16_t>(id);
    actor.alive = actor.ported = true;
    actor.state = id == 0x3009 || id == 0x300A ? state : 1;
    actor.v0 = id == 0x3031 ? state : 0;
    actor.x8 = 16;
    actor.yw = 5 * 128;
    actor.hits = 10;
    if (id == 0x302B || id == 0x3022)
      world.updatePortedPart2(actor);
    else if (id == 0x3031)
      world.updatePortedPart3(actor);
    else
      world.updatePortedPart5(actor);
    return world.takeSounds();
  }
  static std::vector<int> bunnySound(PlayWorld& world, bool shot)
  {
    PlayWorld::ActorInstance actor{};
    actor.id = 0x303B;
    actor.alive = actor.ported = true;
    actor.state = 1;
    actor.x8 = 16;
    actor.yw = 5 * 128;
    actor.cellIndex = 5 * 128 + 7;
    if (shot)
    {
      world.mSandbox.bullets[0] = {1, actor.x8, actor.yw};
    }
    world.updatePortedPart2(actor);
    return world.takeSounds();
  }
  static std::pair<bool, int>
    reactorBurnout(PlayWorld& world, int phase, int age)
  {
    world.mPhase = phase;
    world.mDebris.clear();
    PlayWorld::ActorInstance actor{};
    actor.id = 0x302B;
    actor.alive = actor.ported = true;
    actor.state = 1;
    actor.v0 = age;
    actor.x8 = 16;
    actor.yw = 5 * 128;
    actor.cellIndex = 5 * 128 + 7;
    world.updatePortedPart2(actor);
    return {actor.alive, world.mDebris.empty() ? -1 : world.mDebris.back().x8};
  }
  static std::pair<int, int> reactorBurst(PlayWorld& world, int age)
  {
    world.mExplosions.clear();
    world.mDebris.clear();
    PlayWorld::ActorInstance actor{};
    actor.id = 0x302B;
    actor.alive = actor.ported = true;
    actor.state = 1;
    actor.v0 = age;
    actor.x8 = 16;
    actor.yw = 5 * 128;
    actor.cellIndex = 5 * 128 + 7;
    world.updatePortedPart2(actor);
    return {
      int(world.mExplosions.size()),
      world.mExplosions.empty() ? -1 : world.mExplosions.back().yw};
  }
  static unsigned interactionHint(PlayWorld& world, bool elevator)
  {
    PlayWorld::ActorInstance actor{};
    actor.id = elevator ? 0x3001 : 0x3052;
    actor.alive = actor.ported = true;
    actor.state = elevator ? 1 : 2;
    actor.x8 = 16;
    actor.yw = 5 * 128;
    actor.cellIndex = 5 * 128 + 7;
    if (elevator)
      world.updatePortedPart3(actor);
    else
      world.updatePortedPart2(actor);
    return world.takeHintEvents();
  }
  static void cameraFixture(PlayWorld& world)
  {
    world.mActors.clear();
    world.mBonusTotals[0] = 3;
    for (int i = 0; i < 3; ++i)
    {
      PlayWorld::ActorInstance camera{};
      camera.id = 0x3024;
      camera.alive = i != 0;
      world.mActors.push_back(camera);
    }
  }
  static std::array<int, 5>
    blower(PlayWorld& world, int id, bool shot, bool blocked)
  {
    PlayWorld::ActorInstance actor{};
    actor.id = static_cast<std::uint16_t>(id);
    actor.ported = actor.alive = true;
    actor.state = id == 0x301B ? 1 : 2;
    actor.x8 = 16;
    actor.yw = 5 * 128;
    auto& player = world.mSandbox.player;
    player.x8 = actor.x8 + (actor.state == 1 ? -4 : 4);
    player.y = 96;
    if (blocked)
    {
      for (int row = 0; row < 6; ++row)
        for (int col = 0; col < 20; ++col)
          world.mSandbox.world.cells[row * 128 + col] = 0x1820;
    }
    if (shot)
      world.mSandbox.bullets[0] = {1, actor.x8, actor.yw};
    const int before = player.x8;
    const int oldScore = world.mScore;
    world.updatePortedPart1(actor);
    const int delta = player.x8 - before;
    const int state = actor.state;
    const int score = world.mScore - oldScore;
    const int cues = int(world.takeSounds().size());
    for (int i = 0; i < 22; ++i)
      if (actor.alive)
        world.updatePortedPart1(actor);
    return {delta, state, score, cues, actor.alive ? 1 : 0};
  }
  static void prepare(PlayWorld& world)
  {
    world.mActors.clear();
    world.mExplosions.clear();
    world.mSparks.clear();
    world.mSandbox.world.cells.fill(0);
    world.mSandbox.player = PlayerState{};
    world.mSandbox.input = {};
    world.mSandbox.bullets = {};
    world.mPhase = 0;
    world.mBlend = 1.0;
    for (int col = 0; col < 128; ++col)
      world.mSandbox.world.cells[6 * 128 + col] = 0x1820;
    for (auto& bank : world.mSprites)
      for (std::size_t frame = 0; frame < bank.size(); ++frame)
      {
        auto& rgba = bank[frame];
        for (std::size_t i = 0; i < rgba.size(); i += 4)
        {
          rgba[i] = static_cast<std::uint8_t>(frame + 1);
          rgba[i + 1] = rgba[i + 2] = 0;
          rgba[i + 3] = 255;
        }
      }
    // Each four-piece pose has a distinct synthetic EGA color.
    for (std::size_t bank = 0; bank < world.mMan.size(); ++bank)
      for (std::size_t frame = 0; frame < world.mMan[bank].tiles.size();
           ++frame)
      {
        auto& tile = world.mMan[bank].tiles[frame];
        const auto color = (bank * 12 + frame / 4) % 15 + 1;
        tile.packed.assign(160, 0);
        for (std::size_t i = 0; i < tile.packed.size(); i += 5)
        {
          tile.packed[i] = 255;
          for (int plane = 0; plane < 4; ++plane)
            tile.packed[i + 1 + plane] = (color & (1u << plane)) ? 255 : 0;
        }
      }
    world.takeSounds();
  }
};
} // namespace rigel::duke1

namespace
{
using namespace rigel;
using WorldAccess = duke1::PlayWorldTestAccess;

std::array<std::uint8_t, 3>
  pixel(const std::vector<std::uint8_t>& rgba, int x, int y)
{
  const auto index = static_cast<std::size_t>(y * 208 + x) * 4;
  return {rgba.at(index), rgba.at(index + 1), rgba.at(index + 2)};
}
std::vector<std::uint8_t> render(const duke1::PlayWorld& world)
{
  std::vector<std::uint8_t> rgba;
  world.renderWorld(rgba, 208, 160, 0, 0, world.worldX(), world.worldY());
  return rgba;
}
} // namespace

TEST_CASE(
  "DN1 stationary firing changes pose at the DOS player anchor",
  "[duke1][presentation]")
{
  rigel::test::Duke1Fixture fixture;
  duke1::PlayWorld world(fixture.mData, "WORLDAL1");
  WorldAccess::prepare(world);
  auto& sandbox = WorldAccess::sandbox(world);
  sandbox.player.facing = GENERATE(0, 4);
  auto rgba = render(world);
  const bool left = sandbox.player.facing == 0;
  const std::array<std::uint8_t, 3> idle = left
    ? std::array<std::uint8_t, 3>{255, 85, 255}
    : std::array<std::uint8_t, 3>{255, 255, 85};
  const std::array<std::uint8_t, 3> firing = left
    ? std::array<std::uint8_t, 3>{170, 0, 0}
    : std::array<std::uint8_t, 3>{85, 85, 85};
  const std::array<std::uint8_t, 3> black{0, 0, 0};
  // Original screen quadrant x=15, minus the 16-pixel playfield origin.
  CHECK(pixel(rgba, 103, 64) == black);
  CHECK(pixel(rgba, 104, 64) == idle);
  CHECK(pixel(rgba, 135, 95) == idle);
  CHECK(pixel(rgba, 136, 95) == black);

  sandbox.input.fire = true;
  rgba = render(world);
  CHECK(pixel(rgba, 104, 64) == firing);
  sandbox.input.fire = false;
  CHECK(pixel(render(world), 104, 64) == idle);
  sandbox.input.fire = true;
  sandbox.input.left = left;
  sandbox.input.right = !left;
  sandbox.player.walkFrame = 2;
  const std::array<std::uint8_t, 3> walking = left
    ? std::array<std::uint8_t, 3>{0, 170, 170}
    : std::array<std::uint8_t, 3>{170, 170, 170};
  CHECK(pixel(render(world), 104, 64) == walking);
}

TEST_CASE(
  "DN1 projectiles use directional DOS art and screen coordinates",
  "[duke1][presentation]")
{
  rigel::test::Duke1Fixture fixture;
  duke1::PlayWorld world(fixture.mData, "WORLDAL1");
  WorldAccess::prepare(world);
  auto& sandbox = WorldAccess::sandbox(world);
  sandbox.player.health = 0; // isolate the projectile layer
  const auto kind = GENERATE(1, 5);
  const bool fresh = GENERATE(false, true);
  sandbox.bullets[0] = {kind, 20, 512, 0, fresh ? 1 : 2};
  const auto rgba = render(world);
  // Original screen x=20*8, y=512/8+19; subtract playfield origin (16,16).
  const std::array<std::uint8_t, 3> black{0, 0, 0};
  const auto frame = fresh ? (kind == 1 ? 46 : 47) : (kind == 1 ? 6 : 8);
  const std::array<std::uint8_t, 3> expected{
    static_cast<std::uint8_t>(frame + 1), 0, 0};
  CHECK(pixel(rgba, 143, 67) == black);
  CHECK(pixel(rgba, 144, 67) == expected);
  CHECK(pixel(rgba, 159, 82) == expected);
  CHECK(pixel(rgba, 160, 82) == black);
}

TEST_CASE(
  "DN1 wall contacts show the small enemy-impact animation silently",
  "[duke1][presentation]")
{
  rigel::test::Duke1Fixture fixture;
  duke1::PlayWorld world(fixture.mData, "WORLDAL1");
  WorldAccess::prepare(world);
  auto& sandbox = WorldAccess::sandbox(world);
  const auto kind = GENERATE(1, 5);
  const int wallCol = kind == 1 ? 3 : 5;
  sandbox.world.cells[4 * 128 + wallCol] = 0x1820;
  sandbox.bullets[0] = {kind, 10, 512, 0, 2};
  world.update({});
  CHECK(sandbox.bullets[0].kind == 0);
  CHECK(world.takeSounds().empty());
  const int x = kind == 1 ? 56 : 72;
  for (int frame = 42; frame <= 47; ++frame)
  {
    const std::array<std::uint8_t, 3> expected{
      static_cast<std::uint8_t>(frame + 1), 0, 0};
    CHECK(pixel(render(world), x, 72) == expected);
    world.update({});
  }
  const std::array<std::uint8_t, 3> black{0, 0, 0};
  CHECK(pixel(render(world), x, 72) == black);
}

TEST_CASE(
  "DN1 original sensors allow a single-tile shaft without widening",
  "[duke1][collision]")
{
  duke1::Sandbox sandbox;
  for (int row = 0; row < 6; ++row)
  {
    sandbox.world.cells[row * 128 + 9] = 0x1820;
    sandbox.world.cells[row * 128 + 11] = 0x1820;
  }
  for (int col = 0; col < 128; ++col)
    sandbox.world.cells[6 * 128 + col] = 0x1820;
  sandbox.player.x8 = 22;
  sandbox.player.y = 96;
  auto& p = sandbox.player;
  CHECK(sandbox.world.clear(duke1::Sensor::Head, p, sandbox.events));
  CHECK_FALSE(sandbox.world.clear(duke1::Sensor::Left, p, sandbox.events));
  CHECK_FALSE(sandbox.world.clear(duke1::Sensor::Right, p, sandbox.events));
  sandbox.input.jump = true;
  sandbox.tick();
  sandbox.tick();
  CHECK(p.y == 83);
  CHECK(p.x8 + p.cameraX8 == 22);
}

TEST_CASE(
  "DN1 TNT plays its ignition sound once before the ground fire spreads",
  "[duke1][sound]")
{
  rigel::test::Duke1Fixture fixture;
  duke1::PlayWorld world(fixture.mData, "WORLDAL1");
  WorldAccess::prepare(world);
  WorldAccess::primedTnt(world);
  world.update({});
  CHECK(world.takeSounds() == std::vector<int>{33});
  for (int i = 0; i < 9; ++i)
  {
    world.update({});
    CHECK(world.takeSounds().empty());
  }
}

TEST_CASE(
  "DN1 muzzle flashes draw over the gun like the DOS projectile pass",
  "[duke1][presentation]")
{
  rigel::test::Duke1Fixture fixture;
  duke1::PlayWorld world(fixture.mData, "WORLDAL1");
  WorldAccess::prepare(world);
  auto& sandbox = WorldAccess::sandbox(world);
  const auto kind = GENERATE(1, 5);
  sandbox.player.facing = kind == 1 ? 0 : 4;
  sandbox.input.fire = true;
  sandbox.bullets[0] = {kind, kind == 1 ? 14 : 18, 512, 0, 1};
  const std::array<std::uint8_t, 3> expected{
    static_cast<std::uint8_t>(kind == 1 ? 47 : 48), 0, 0};
  CHECK(pixel(render(world), kind == 1 ? 104 : 128, 67) == expected);
}

TEST_CASE(
  "DN1 key switches request the DOS door sound once on successful unlock",
  "[duke1][sound]")
{
  rigel::test::Duke1Fixture fixture;
  duke1::PlayWorld world(fixture.mData, "WORLDAL1");
  WorldAccess::prepare(world);
  const int color = GENERATE(0, 1, 2, 3);
  WorldAccess::keyDoor(world, color);
  duke1::Input input;
  input.up = true;
  world.update(input);
  CHECK(world.takeSounds().empty());
  CHECK_FALSE(WorldAccess::opening(world));

  WorldAccess::grantKey(world, color);
  world.update({});
  CHECK(world.takeSounds().empty());
  CHECK_FALSE(WorldAccess::opening(world));
  world.update(input);
  CHECK(world.takeSounds() == std::vector<int>{31});
  CHECK(WorldAccess::opening(world));
  CHECK_FALSE(WorldAccess::hasKey(world, color));
  for (int i = 0; i < 18; ++i)
  {
    world.update(input);
    CHECK(world.takeSounds().empty());
  }
}

TEST_CASE(
  "DN1 exit column matches original lethal-contact boundaries",
  "[duke1][collision]")
{
  rigel::test::Duke1Fixture fixture;
  duke1::PlayWorld world(fixture.mData, "WORLDAL1");
  WorldAccess::prepare(world);
  for (bool immunity : {false, true})
    for (bool disabled : {false, true})
      for (int dx = -3; dx <= 3; ++dx)
        for (int feetOffset = -48; feetOffset <= 48; feetOffset += 8)
        {
          CAPTURE(dx, feetOffset, immunity, disabled);
          // Original callback sweep: upper overlap is not universally lethal.
          const bool lethal = !disabled && dx > -2 && dx < 2 &&
            feetOffset >= 0 && feetOffset < 48;
          CHECK(
            WorldAccess::columnContact(
              world, dx, feetOffset, immunity, disabled) == (lethal ? 0 : 9));
        }
}

TEST_CASE(
  "DN1 hazard sounds follow their original transitions",
  "[duke1][sound]")
{
  rigel::test::Duke1Fixture fixture;
  duke1::PlayWorld world(fixture.mData, "WORLDAL1");
  WorldAccess::prepare(world);
  CHECK(WorldAccess::hazardSounds(world, 0x3009, 14) == std::vector<int>{43});
  CHECK(WorldAccess::hazardSounds(world, 0x300A, 14) == std::vector<int>{43});
  CHECK(WorldAccess::hazardSounds(world, 0x3031, 6).empty());
  CHECK(WorldAccess::hazardSounds(world, 0x3031, 7) == std::vector<int>{34});
  CHECK(WorldAccess::hazardSounds(world, 0x302B, 0) == std::vector<int>{36});
  CHECK(WorldAccess::hazardSounds(world, 0x3022, 0) == std::vector<int>{22});
}

TEST_CASE("DN1 camera progress counts destroyed cameras", "[duke1][results]")
{
  rigel::test::Duke1Fixture fixture;
  duke1::PlayWorld world(fixture.mData, "WORLDAL1");
  WorldAccess::prepare(world);
  WorldAccess::cameraFixture(world);
  const auto report = world.bonusReport();
  CHECK(report.present[duke1::PlayWorld::Cameras]);
  CHECK_FALSE(report.earned[duke1::PlayWorld::Cameras]);
  CHECK(report.cameraDestroyed == 1);
  CHECK(report.cameraTotal == 3);
}

TEST_CASE(
  "DN1 walking queues the speaker effect while moving on the floor",
  "[duke1][sound]")
{
  rigel::test::Duke1Fixture fixture;
  duke1::PlayWorld world(fixture.mData, "WORLDAL1");
  WorldAccess::prepare(world);
  duke1::Input input;
  input.right = true;
  bool heard = false;
  for (int i = 0; i < 8; ++i)
  {
    world.update(input);
    const auto sounds = world.takeSounds();
    heard =
      heard || std::find(sounds.begin(), sounds.end(), 22) != sounds.end();
  }
  CHECK(heard);
}

TEST_CASE(
  "DN1 reactor burnout follows original timing and effect positions",
  "[duke1][effects]")
{
  rigel::test::Duke1Fixture fixture;
  duke1::PlayWorld world(fixture.mData, "WORLDAL1");
  WorldAccess::prepare(world);
  CHECK(
    WorldAccess::reactorBurnout(world, 0, 1) == std::pair<bool, int>{true, 15});
  CHECK(
    WorldAccess::reactorBurnout(world, 1, 1) == std::pair<bool, int>{true, -1});
  CHECK(
    WorldAccess::reactorBurnout(world, 2, 1) == std::pair<bool, int>{true, 17});
  CHECK_FALSE(WorldAccess::reactorBurnout(world, 0, 31).first);
}

TEST_CASE(
  "DN1 pink enemy contact and defeat sounds match DOS",
  "[duke1][sound]")
{
  rigel::test::Duke1Fixture fixture;
  duke1::PlayWorld world(fixture.mData, "WORLDAL1");
  WorldAccess::prepare(world);
  CHECK(WorldAccess::bunnySound(world, false) == std::vector<int>{35});
  WorldAccess::prepare(world);
  CHECK(WorldAccess::bunnySound(world, true) == std::vector<int>{18});
}

TEST_CASE(
  "DN1 beam burnout shows harmless bursts along its height",
  "[duke1][effects]")
{
  rigel::test::Duke1Fixture fixture;
  duke1::PlayWorld world(fixture.mData, "WORLDAL1");
  WorldAccess::prepare(world);
  CHECK(WorldAccess::reactorBurst(world, 2) == std::pair<int, int>{0, -1});
  CHECK(WorldAccess::reactorBurst(world, 3) == std::pair<int, int>{1, 4 * 128});
  CHECK(WorldAccess::reactorBurst(world, 7) == std::pair<int, int>{1, 3 * 128});
  CHECK(
    WorldAccess::reactorBurst(world, 11) == std::pair<int, int>{1, 5 * 128});
}

TEST_CASE(
  "DN1 drink and elevator interactions request their hints",
  "[duke1][hints]")
{
  rigel::test::Duke1Fixture fixture;
  duke1::PlayWorld world(fixture.mData, "WORLDAL1");
  WorldAccess::prepare(world);
  REQUIRE(WorldAccess::sandbox(world).player.health == 9);
  CHECK(
    WorldAccess::interactionHint(world, false) == duke1::PlayWorld::DrinkHint);
  CHECK(world.takeHintEvents() == 0);
  WorldAccess::prepare(world);
  CHECK(
    WorldAccess::interactionHint(world, true) ==
    duke1::PlayWorld::ElevatorHint);
  CHECK(world.takeHintEvents() == 0);
}

TEST_CASE("DN1 air devices push and wind down after a shot", "[duke1][actors]")
{
  rigel::test::Duke1Fixture fixture;
  duke1::PlayWorld world(fixture.mData, "WORLDAL1");
  for (const int id : {0x301B, 0x301C})
  {
    WorldAccess::prepare(world);
    const auto live = WorldAccess::blower(world, id, false, false);
    CHECK(live[0] == (id == 0x301B ? -1 : 1));
    WorldAccess::prepare(world);
    CHECK(WorldAccess::blower(world, id, false, true)[0] == 0);
    WorldAccess::prepare(world);
    const auto shot = WorldAccess::blower(world, id, true, false);
    CHECK(shot[0] == 0);
    CHECK(shot[1] == 4);
    CHECK(shot[2] == 1000);
    CHECK(shot[3] == 1);
    CHECK(shot[4] == 0);
  }
}
