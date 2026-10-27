#include "duke1_frontend_fixture.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

namespace rigel::duke1
{
struct PlayWorldTestAccess
{
  static Sandbox& sandbox(PlayWorld& world) { return world.mSandbox; }
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
