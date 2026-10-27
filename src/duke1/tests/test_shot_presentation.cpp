// Generated map and pixels; optional player-owned DOS files initialize the
// world.
#include "play.hpp"

#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace duke1
{
struct PlayWorldTestAccess
{
  static void check(bool value, const char* message)
  {
    if (!value)
      throw std::runtime_error(message);
  }
  static int shotTop(PlayWorld& world, int drawY)
  {
    std::vector<std::uint8_t> withShot, withoutShot;
    world.sandbox_.player.health = 0; // isolate projectile pixels
    world.renderWorld(withShot, 256, 144, 0, 0, world.worldX(), drawY);
    const auto saved = world.sandbox_.bullets;
    for (auto& b : world.sandbox_.bullets)
      b.kind = 0;
    world.renderWorld(withoutShot, 256, 144, 0, 0, world.worldX(), drawY);
    world.sandbox_.bullets = saved;
    world.sandbox_.player.health = 9;
    for (int y = 0; y < 144; ++y)
      for (int x = 0; x < 256; ++x)
      {
        const auto i = static_cast<std::size_t>((y * 256 + x) * 4);
        if (!std::equal(
              withShot.begin() + i,
              withShot.begin() + i + 4,
              withoutShot.begin() + i))
          return y;
      }
    throw std::runtime_error("no projectile pixels rendered");
  }
  static void run(PlayWorld& world, bool left)
  {
    world.actors_.clear();
    world.sandbox_.world.cells.fill(0);
    for (int col = 0; col < 128; ++col)
    {
      world.sandbox_.world.cells[2 * 128 + col] = 0x1800;
      world.sandbox_.world.cells[6 * 128 + col] = 0x1820;
    }
    auto& p = world.sandbox_.player;
    p = PlayerState{};
    p.facing = left ? 0 : 4;
    world.grantAll();
    world.phase_ = 3; // boots flip starts on the next phase
    // Synthetic opaque graphics distinguish projectile placement from game art.
    for (auto frame : {6, 7})
      for (std::size_t i = 0; i < world.sprites_[6][frame].size(); i += 4)
      {
        auto& pixels = world.sprites_[6][frame];
        pixels[i] = 1;
        pixels[i + 1] = 2;
        pixels[i + 2] = 3;
        pixels[i + 3] = 255;
      }
    Input input{};
    input.jump = true;
    input.left = left;
    input.right = !left;
    world.update(input);
    check(world.worldY() == 96 && p.jumpTravel != 0, "boots jump starts");
    world.update(input);
    check(world.worldY() == 81, "boots rise is 15 pixels");
    input.fire = true;
    world.update(input);
    check(
      world.prevWorldY() == 81 && world.worldY() == 89,
      "low ceiling changes rising to falling");
    check(
      p.jumpTravel == 0 && p.falling && !p.cling,
      "ceiling cancels flip without a claw attachment");
    check(
      world.sandbox_.bullets[0].kind == (left ? 1 : 5),
      "shot allocated in facing direction");
    check(
      world.sandbox_.bullets[0].yWords == 3 * 128,
      "DOS collision row is unchanged");
    for (int drawY : {81, 83, 85, 87, 89})
    {
      world.setRenderBlend((drawY - 81) / 8.0);
      check(
        shotTop(world, drawY) == drawY - 29,
        "birth follows interpolated muzzle vertically");
    }
    input.fire = false;
    world.update(input);
    check(
      shotTop(world, world.worldY()) == 60,
      "existing shot stays level when Duke falls");
    // A released/reused slot must acquire the new muzzle height.
    for (auto& b : world.sandbox_.bullets)
      b.kind = 0;
    p.fireLatch = false;
    p.y = 96;
    p.grounded = false;
    p.falling = true;
    world.sandbox_.query = [](Sensor) {
      return true;
    };
    input.fire = true;
    world.update(input);
    check(
      shotTop(world, world.worldY()) == world.worldY() - 29,
      "reused slot resets visual height");
    // Instant player movement must not drag a just-fired shot with it.
    for (auto& b : world.sandbox_.bullets)
      b.kind = 0;
    p.fireLatch = false;
    world.discontinuity_ = true;
    world.update(input);
    const int birthY = world.shotPresentation_[0].y;
    check(!world.shotPresentation_[0].born, "discontinuity clears birth blend");
    check(
      shotTop(world, world.worldY() - 50) == birthY,
      "a discontinuity cannot drag the shot along with Duke");
  }
};
} // namespace duke1

int main(int argc, char** argv)
{
  if (argc < 2 || argc > 3)
    return 2;
  try
  {
    const int episode = argc == 3 ? std::stoi(argv[2]) - 1 : 0;
    for (bool left : {false, true})
    {
      duke1::PlayWorld world(argv[1], "WORLDAL3", nullptr, episode);
      duke1::PlayWorldTestAccess::run(world, left);
    }
    std::cout
      << "Low-ceiling muzzle, interpolation, flight and slot reuse passed\n";
  }
  catch (const std::exception& error)
  {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
