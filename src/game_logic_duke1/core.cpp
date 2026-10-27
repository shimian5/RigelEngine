#include "core.hpp"
#include <algorithm>
#include <stdexcept>
namespace rigel::duke1
{
namespace
{
// Portable arithmetic floor division for original signed-coordinate sensors.
int floorDiv(int n, int d)
{
  int q = n / d;
  return q - ((n % d) < 0);
}
bool attaches(std::uint16_t t)
{
  return (t >= 0x1b80 && t <= 0x1ca0) || t >= 0x2ee0;
}
} // namespace
CollisionWorld::CollisionWorld(std::uint16_t fill)
{
  cells.fill(fill);
}
std::uint16_t CollisionWorld::at(int index) const
{
  if (index < 0 || index >= int(cells.size()))
    return 0x1800;
  return cells.at(std::size_t(index));
}
bool CollisionWorld::clear(
  Sensor sensor,
  PlayerState& p,
  std::vector<Event>& events) const
{
  const int wx = p.x8 + p.cameraX8;
  if (sensor == Sensor::Floor && p.pullUp)
    return false;
  if (sensor == Sensor::Left || sensor == Sensor::Right)
  {
    int col = floorDiv(wx - (sensor == Sensor::Left ? 3 : 0), 2);
    int row = floorDiv(p.y - 16, 16);
    int i = p.cameraYWords + 128 * row + col;
    return at(i) < 0x1800 && at(i - 128) < 0x1800 &&
      at(i + ((p.y & 8) ? 128 : 0)) < 0x1800;
  }
  const int i = p.cameraYWords +
    128 * floorDiv(p.y - (sensor == Sensor::Head ? 40 : 0), 16) +
    floorDiv(wx - 2, 2);
  const auto a = at(i), b = at(i + ((wx & 1) ? 1 : 0));
  if (sensor == Sensor::Head)
  {
    if (attaches(a) || attaches(b))
    {
      if (!p.pullUp && !p.falling)
      {
        if (!p.cling)
          p.walkFrame = 0;
        if (p.claw)
        {
          p.cling = true;
          p.verticalScroll = true;
          events.push_back({Event::Sound, 27});
        }
        else
          events.push_back({Event::Sound, 38});
      }
      return false;
    }
    p.cling = false;
    if (a >= 0x1800 || b >= 0x1800)
    {
      events.push_back({Event::Sound, 38});
      return false;
    }
  }
  return a < 0x1800 && b < 0x1800;
}
bool Sandbox::clear(Sensor s)
{
  return query ? query(s) : world.clear(s, player, events);
}
void Sandbox::alignY()
{
  player.y = floorDiv(player.y, 16) * 16;
}
void Sandbox::contact()
{
  events.push_back(
    {Event::MapContact,
     (floorDiv(player.y - 16, 16) * 128 + player.cameraYWords) & 65535});
  events.push_back({Event::Sound, 14});
}
void Sandbox::movement()
{
  events.clear();
  auto& p = player;
  if (p.pendingHit && p.immunity)
    p.pendingHit = false;
  if (p.death)
    return;
  if (clear(Sensor::Floor) && p.grounded)
  {
    p.grounded = false;
    p.falling = true;
  }
  if (!p.grounded && !p.cling)
  {
    if (p.jumpTravel && ++p.jumpTravelAge == 14)
    {
      p.jumpTravel = 0;
      p.jumpTravelAge = 0;
    }
    if (!clear(Sensor::Head) && p.jumpTravel)
    {
      p.jumpTravel = 0;
      p.jumpTravelAge = 0;
    }
    if (!p.falling && p.jumpIndex < 9 && clear(Sensor::Head))
    {
      static constexpr int rises[2][9] = {
        {13, 11, 9, 7, 5, 3, 0, 0, 0}, {15, 13, 11, 9, 7, 5, 3, 1, 0}};
      if (p.boost < 0 || p.boost > 1)
        throw std::invalid_argument("unverified jump upgrade selector");
      p.y -= rises[p.boost][p.jumpIndex++];
      p.verticalScroll = false;
      if (!clear(Sensor::Floor))
      {
        p.grounded = true;
        p.verticalScroll = true;
        alignY();
        contact();
        p.jumpTravel = 0;
        p.jumpTravelAge = 0;
      }
    }
    else
      p.falling = true;
    if (p.falling)
    {
      if (!clear(Sensor::Floor))
      {
        p.grounded = true;
        p.verticalScroll = true;
        p.falling = false;
        p.jumpTravel = 0;
        p.jumpTravelAge = 0;
        alignY();
        contact();
      }
      else
        p.y += 8;
      if (!clear(Sensor::Floor))
      {
        alignY();
        contact();
        p.jumpTravel = 0;
        p.jumpTravelAge = 0;
        p.verticalScroll = true;
      }
      if (p.y >= 132)
      {
        p.y += 8;
        p.verticalScroll = true;
        if (!clear(Sensor::Floor))
        {
          p.grounded = true;
          p.jumpIndex = 0;
          p.falling = false;
          alignY();
          contact();
        }
      }
    }
  }
  // Three-stage ceiling pull-up is distinct from an ordinary jump.
  if (input.up && p.cling && !p.pullUp)
  {
    p.y -= 16;
    if (clear(Sensor::Head))
    {
      p.y -= 16;
      if (clear(Sensor::Head))
      {
        p.pullUp = 3;
        p.cling = false;
      }
      p.y += 16;
    }
    else
    {
      p.pullUp = 0;
      p.cling = true;
    }
    p.y += 16;
  }
  if (input.down && p.cling)
  {
    p.walkFrame = 0;
    p.cling = false;
  }
  const bool clingFiring = p.cling && input.fire;
  auto walk = [&](bool left) {
    const int target = left ? 0 : 4;
    if (p.pullUp)
      return;
    if (p.facing == target && clear(left ? Sensor::Left : Sensor::Right))
      p.x8 += left ? -1 : 1;
    if (p.facing != target)
    {
      p.facing = target;
      p.walkFrame = 0;
      p.jumpTravel = 0;
      return;
    }
    if (p.phase == 0 || p.phase == 64)
    {
      if (!clear(Sensor::Floor))
        events.push_back({Event::Sound, 22});
      p.walkFrame = (p.walkFrame + 1) % 4;
    }
  };
  if (!clingFiring && input.left && !input.right)
    walk(true);
  // Camera checks are interleaved with directional movement in the DOS routine.
  if (p.x8 == 11 && p.cameraX8 > 0)
  {
    --p.cameraX8;
    ++p.x8;
  }
  else if (p.x8 <= 10 && p.cameraX8 > 1)
  {
    p.cameraX8 -= 2;
    p.x8 += 2;
    if (p.cameraX8 == 1)
    {
      --p.cameraX8;
      ++p.x8;
    }
  }
  if (!clingFiring && input.right && !input.left)
    walk(false);
  if (p.x8 == 17 && p.cameraX8 < 230)
  {
    ++p.cameraX8;
    --p.x8;
  }
  else if (p.x8 >= 18 && p.cameraX8 < 230)
  {
    p.cameraX8 += 2;
    p.x8 -= 2;
    if (p.cameraX8 == 229)
    {
      ++p.cameraX8;
      --p.x8;
    }
  }
  if (p.cling)
  {
    alignY();
    p.falling = true;
    p.grounded = false;
    clear(Sensor::Head);
  }
  if (input.jump && p.grounded && !p.jumpLatch && !p.cling && !p.pullUp)
  {
    p.grounded = false;
    p.jumpIndex = 0;
    p.falling = false;
    if (p.boost && p.phase == 0)
    {
      if (input.left && clear(Sensor::Left))
        p.jumpTravel = 1;
      if (input.right && clear(Sensor::Right))
        p.jumpTravel = 2;
    }
    events.push_back({Event::Sound, 13});
  }
  p.jumpLatch = input.jump && !p.cling;
}
void Sandbox::cameraY()
{
  auto& p = player;
  if (p.cameraLock || !p.verticalScroll)
    return;
  if (p.y < 100 && p.cameraYWords > 0)
  {
    p.cameraYWords -= 128;
    p.y += 16;
    if (p.y >= 100)
      p.verticalScroll = false;
  }
  else if (p.y > 112)
  {
    p.cameraYWords += 128;
    p.y -= 16;
    if (p.y <= 112)
      p.verticalScroll = false;
  }
}
bool Sandbox::spawn(int face, int yWords, int x8)
{
  if (player.capacity < 1 || player.capacity > 4)
    throw std::invalid_argument("bullet capacity must be1..4");
  for (int i = 0; i < player.capacity; ++i)
    if (!bullets[i].kind)
    {
      bullets[i] = {face + 1, x8, yWords, 1};
      events.push_back({Event::Sound, 6});
      return true;
    }
  return false;
}
bool Sandbox::culled(int x8, int yWords) const
{
  const auto& p = player;
  int x0 = p.cameraX8, x1 = p.cameraX8 + 29, y0 = p.cameraYWords - 64,
      y1 = p.cameraYWords + 1280;
  if (wideCull)
  {
    x0 = std::min(x0, cullX0);
    x1 = std::max(x1, cullX1);
    y0 = std::min(y0, cullY0);
    y1 = std::max(y1, cullY1);
  }
  return x8 < x0 || x8 >= x1 || yWords < y0 || yWords >= y1;
}
void Sandbox::moveBullets(bool checkWorld, bool cull)
{
  if (player.capacity < 1 || player.capacity > 4)
    throw std::invalid_argument("bullet capacity must be1..4");
  for (int slot = 0; slot < player.capacity; ++slot)
  {
    auto& b = bullets[slot];
    if (!b.kind)
      continue;
    if (b.kind != 1 && b.kind != 5)
      throw std::invalid_argument("unverified projectile kind");
    b.x8 += b.kind == 1 ? -2 : 2;
    const int col = floorDiv(b.x8 + (b.kind == 1 ? 1 : 0), 2) - 1;
    const int index = floorDiv(b.yWords, 128) * 128 + col;
    if (checkWorld)
    {
      // Breakable0x1800 is a proved bullet exception, not the generic solid
      // threshold.
      int broken = -1;
      if (index >= 0 && index < 11520)
      {
        if (world.at(index) == 0x1800)
          broken = index;
        else if (index + 128 < 11520 && world.at(index + 128) == 0x1800)
          broken = index + 128;
      }
      if (broken >= 0)
      {
        world.cells[broken] = 0x17e0;
        events.push_back({Event::Break, broken});
        events.push_back({Event::Sound, 42});
        events.push_back({Event::Score, 10});
        b.kind = 0;
        continue;
      }
      // Actual bullet helper compares the cell as a signed16-bit word.
      const auto cell = world.at(index);
      if (cell >= 0x1800 && cell < 0x8000)
      {
        impacts.push_back({b.x8, b.yWords});
        b.kind = 0;
        continue;
      }
    }
    if (cull && culled(b.x8, b.yWords))
      b.kind = 0;
    b.newFlag = 0;
    ++b.age;
  }
}
int Sandbox::drawPlayer()
{
  auto& p = player;
  if (p.exitAnimation >= 4 || p.cameraLock)
    return 0;
  if (p.immunity > 0)
    --p.immunity;
  if (p.y < 16)
  {
    p.verticalScroll = true;
    return 0;
  }
  if (p.immunity % 2)
    return 0;
  return p.y < 32 ? 2 : 4;
}
void Sandbox::damage(bool drawAfter)
{
  auto& p = player;
  if (!p.pendingHit || p.immunity)
    return;
  if (!p.death)
    --p.health;
  p.pendingHit = false;
  p.immunity = 21;
  p.jumpIndex = 0;
  p.grounded = false;
  p.cling = false;
  p.falling = true;
  p.jumpTravel = 0;
  p.jumpTravelAge = 0;
  events.push_back({Event::Sound, 15});
  if (drawAfter)
    drawPlayer();
}
void Sandbox::lateUpdate()
{
  auto& p = player;
  if (!p.pullUp && !p.pendingHit)
    drawPlayer();
  if (p.pullUp && !p.pendingHit)
  {
    --p.pullUp;
    p.y -= 16;
    drawPlayer();
    if (!p.pullUp)
      p.verticalScroll = true;
  }
  if (input.fire && !p.fireLatch && !p.pendingHit && !p.shotBlock)
    spawn(
      p.facing,
      floorDiv(p.y - 29, 16) * 128 + p.cameraYWords,
      p.x8 + p.cameraX8);
  p.fireLatch = input.fire;
  damage(true);
}
void Sandbox::tick()
{
  movement();
  lateUpdate();
  cameraY();
  moveBullets();
}
TickGate::TickGate(int speed)
  : mSpeed(speed)
{
  if (speed < 0 || speed > 9)
    throw std::invalid_argument(
      "speed must be0..9; default is a sandbox choice");
}
bool TickGate::callbacks(std::uint64_t count)
{
  const auto threshold = std::uint64_t(9 + mSpeed);
  if (count >= threshold || mPending >= threshold - count)
  {
    mPending = 0;
    return true;
  }
  mPending += count;
  return false;
}
} // namespace rigel::duke1
