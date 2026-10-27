#include "play.hpp"

#include <stdexcept>

namespace duke1
{
struct PlayWorldTestAccess
{
  static void check(bool ok)
  {
    if (!ok)
      throw std::runtime_error("boss variant assertion failed");
  }
  static void run(PlayWorld& world, std::uint16_t id)
  {
    world.mActors.clear();
    world.mSandbox.world.cells.fill(0);
    PlayWorld::ActorInstance actor{};
    actor.id = id;
    actor.col = 20;
    actor.row = 20;
    world.initPorted(actor);
    check(actor.ported && actor.hits == 10 && actor.state == 1);
    world.mActors.push_back(actor);
    for (int hit = 1; hit <= 9; ++hit)
    {
      auto& boss = world.mActors.front();
      world.mBoss.cnt = 0;
      world.mBoss.jumping = 0;
      world.mBoss.period = 100;
      auto& bullet = world.mSandbox.bullets[0];
      bullet.kind = 1;
      bullet.x8 = boss.x8 + 1;
      bullet.yWords = boss.yw - 0x40;
      world.updatePorted();
      check(boss.hits == 10 - hit);
    }
    auto& boss = world.mActors.front();
    check(boss.state == (id == 0x3043 ? 0 : 1));
    if (id == 0x3042)
    {
      auto& bullet = world.mSandbox.bullets[0];
      bullet.kind = 5;
      bullet.x8 = boss.x8 + 1;
      bullet.yWords = boss.yw - 0x40;
      world.updatePorted();
      check(boss.hits == 0 && boss.state == 2);
      const int floorRow = (boss.yw + 0x20) >> 7;
      world.mSandbox.world
        .cells[std::size_t(floorRow * 128 + (boss.x8 + 1) / 2 - 1)] = 0x1800;
      world.updatePorted();
      check(world.mBoss.deadCount == 1);
      for (int i = 0; i < 9; ++i)
        world.updatePorted();
      check(world.levelComplete());
      check(world.score() == 100000);
    }
    else
    {
      for (int i = 0; i < 32; ++i)
        world.updatePorted();
      check(boss.hits == 1);
    }
  }
};
} // namespace duke1
int main(int argc, char** argv)
{
  if (argc != 2)
    return 2;
  duke1::PlayWorld world(argv[1], "WORLDAL1");
  duke1::PlayWorldTestAccess::run(world, 0x3043);
  duke1::PlayWorld destroyed(argv[1], "WORLDAL1");
  duke1::PlayWorldTestAccess::run(destroyed, 0x3042);
}
