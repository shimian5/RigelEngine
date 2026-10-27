// Drawing helpers, the inventory picture and the shared pools of explosions,
// debris, sparks and enemy shots for the Duke Nukem 1 world.
#include "play.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace duke1
{

void PlayWorld::drawPiece(
  std::vector<std::uint8_t>& out,
  int w,
  int h,
  int left,
  int top,
  int piece,
  int x8,
  int yw) const
{
  // Pieces are numbered across the ANIM files, 48 usable pieces each.
  const std::size_t bank = static_cast<std::size_t>(piece / 48),
                    idx = static_cast<std::size_t>(piece % 48);
  if (bank >= 6 || idx >= mSprites[bank].size())
    return;
  blit(
    out,
    (x8 - 2) * 8 - left + mShiftX,
    yw / 8 - top + mShiftY,
    mSprites[bank][idx].data(),
    16,
    0,
    0,
    16,
    16,
    0,
    0,
    w,
    h,
    w,
    h);
}

void PlayWorld::drawInventory(
  std::vector<std::uint8_t>& out,
  int w,
  int h,
  int x,
  int y) const
{
  for (int i = 0; i < 4; ++i)
  {
    if (!mKeys[i])
      continue;
    const std::size_t bank = 8,
                      idx = static_cast<std::size_t>(24 + 1 + i - 1 + 0);
    if (idx >= mSprites[bank].size())
      continue;
    blit(
      out,
      x + i * 16,
      y,
      mSprites[bank][idx].data(),
      16,
      0,
      0,
      16,
      16,
      0,
      0,
      w,
      h,
      w,
      h);
  }
}

void PlayWorld::debugDump() const
{
  std::printf(
    "player x8=%d y=%d camX8=%d camYW=%d health=%d\n",
    mSandbox.player.x8,
    mSandbox.player.y,
    mSandbox.player.cameraX8,
    mSandbox.player.cameraYWords,
    mSandbox.player.health);
  for (const auto& a : mActors)
    if (!a.ported && a.alive && (a.open || std::getenv("DN1_ALL")))
      std::printf(
        "generic %04X col=%d row=%d open=%d\n", a.id, a.col, a.row, a.open);
  for (const auto& a : mActors)
    if (a.ported && a.alive)
      std::printf(
        "actor %04X col=%d row=%d state=%d x8=%d yw=%d\n",
        a.id,
        a.col,
        a.row,
        a.state,
        a.x8,
        a.yw);
}

void PlayWorld::drawObjectPiece(
  std::vector<std::uint8_t>& out,
  int w,
  int h,
  int left,
  int top,
  int piece,
  int x8,
  int yw,
  int yOffsetPx) const
{
  // OBJECT pieces are numbered across the three OBJECT files, 50 pieces each.
  const std::size_t bank = 6 + static_cast<std::size_t>(piece / 50),
                    idx = static_cast<std::size_t>(piece % 50);
  if (bank >= 9 || idx >= mSprites[bank].size())
    return;
  blit(
    out,
    (x8 - 2) * 8 - left + mShiftX,
    yw / 8 + yOffsetPx - top + mShiftY,
    mSprites[bank][idx].data(),
    16,
    0,
    0,
    16,
    16,
    0,
    0,
    w,
    h,
    w,
    h);
}

void PlayWorld::spawnExplosion(int yw, int x8, int type)
{
  mExplosions.push_back({1, x8, yw, type});
  if (type == 1)
    sound(18);
  else if (type == 3)
    sound(33);
}
void PlayWorld::spawnDebris(int yw, int x8)
{
  mDebris.push_back({1, x8, yw});
}

void PlayWorld::spawnSparks(int yw, int x8)
{
  static const int kVariants[4][4] = {
    {1, 2, 3, 4}, {4, 1, 2, 3}, {2, 4, 3, 1}, {3, 4, 2, 1}};
  mSparkVariant = (mSparkVariant + 1) % 4;
  Spark s{};
  s.state = 1;
  const int xs[4] = {x8, x8 + 1, x8, x8 + 1};
  const int ys[4] = {yw, yw, yw + 8, yw + 8};
  for (int i = 0; i < 4; ++i)
  {
    s.x[i] = xs[i];
    s.y[i] = ys[i];
    s.piece[i] = kVariants[mSparkVariant][i];
    s.px[i] = s.x[i];
    s.py[i] = s.y[i];
  }
  mSparks.push_back(s);
  sound(10);
}
void PlayWorld::spawnEnemyShot(int yw, int x8, int dir)
{
  if (mEnemyShots.size() < 8)
  {
    mEnemyShots.push_back({dir, x8, yw, true, x8});
    sound(11);
  }
}

void PlayWorld::updateOrbs()
{
  static const int kDx[39] = {0,  -1, -1, -2, -2, -2, -1, -1, 0, 1, 1, 2, 2,
                              2,  1,  1,  1,  1,  1,  1,  1,  2, 1, 1, 0, -1,
                              -1, -2, -2, -2, -1, 0,  0,  0,  0, 0, 0, 0, 0};
  static const int kDy[39] = {1,  1,  1, 1, 0, -1, -1, -1, -2, -1, -1, -1, 0,
                              1,  1,  1, 1, 1, 1,  1,  1,  0,  -1, -1, -2, -1,
                              -1, -1, 0, 1, 1, 1,  0,  0,  0,  0,  0,  0,  0};
  if (!mOrbs.on)
    return;
  auto& p = mSandbox.player;
  bool clear = true;
  for (int k = 1; k < 10; ++k)
  {
    mOrbs.x[k - 1] = mOrbs.x[k];
    mOrbs.y[k - 1] = mOrbs.y[k];
    mOrbs.px[k - 1] = mOrbs.px[k];
    mOrbs.py[k - 1] = mOrbs.py[k];
    if (mOrbs.live[k])
      clear = false;
  }
  if (clear)
  {
    mOrbs.on = false;
    mSnakesDone =
      true; // the 10,000 for the whole chain is paid when the level is left
    return;
  }
  mOrbs.px[9] = mOrbs.x[9];
  mOrbs.py[9] = mOrbs.y[9];
  mOrbs.x[9] += kDx[mOrbs.step];
  mOrbs.y[9] += kDy[mOrbs.step] * 0x40;
  for (int k = 1; k < 10; ++k)
  {
    mOrbs.frame[k] = mOrbs.anim / 0xa0;
    if (
      !offscreen(mOrbs.y[k], mOrbs.x[k]) && mOrbs.live[k] &&
      touchesPlayer(mOrbs.x[k], mOrbs.y[k]))
      p.pendingHit = true;
    if (mPhase % 4 != 0)
      mOrbs.anim += 0xa0;
    if (mOrbs.anim == 0x460)
      mOrbs.anim = 0;
    if (mOrbs.live[k] && bulletHit(mOrbs.y[k] - 0x80, mOrbs.x[k]))
    {
      spawnExplosion(mOrbs.y[k], mOrbs.x[k], 3);
      addScore(1000, (mOrbs.x[k] - 2) * 8, mOrbs.y[k] / 8);
      spawnSparks(mOrbs.y[k] - 0x80, mOrbs.x[k]);
      mOrbs.live[k] = false;
    }
  }
  if (++mOrbs.step == 39)
    mOrbs.step = 0;
}

void PlayWorld::updatePools()
{
  updateOrbs();
  auto& p = mSandbox.player;
  // Explosions (2bcb): play seven frames, type 2 also hurts the player
  for (auto& e : mExplosions)
  {
    if (offscreen(e.yw, e.x8))
    {
      e.frame = 0;
      continue;
    }
    if (e.type != 3 && e.type != 1 && e.type != 4 && touchesPlayer(e.x8, e.yw))
      p.pendingHit = true;
    ++e.frame;
  }
  mExplosions.erase(
    std::remove_if(
      mExplosions.begin(),
      mExplosions.end(),
      [](const Explosion& e) {
        return e.frame == 0 ||
          e.frame >= (e.type == 3     ? 5
                        : e.type == 4 ? 3
                                      : 8);
      }),
    mExplosions.end());
  // Sparks (3297): four gems fly out along fixed paths
  static const int kT[4][24] = {
    {0,   0, -71, 0, -59, 0, -45, 0,  -33, -1, -25, 0,
     -10, 0, -1,  0, 13,  0, 16,  -1, 19,  0,  31,  0},
    {0, 0, -50, 1, -47, 0, -33, 1, -23, 0, -10, 1,
     8, 0, 21,  0, 33,  0, 46,  1, 19,  0, 29,  0},
    {0,  0,  1,  -1, 8,  -1, 17, -1, 29, -1, 32, -1,
     45, -1, 51, -1, 65, -1, 79, -1, 79, -1, 99, -1},
    {0,  0, 1,  1, 3,  1, 7,  1, 9,  1, 12, 1,
     15, 1, 21, 1, 25, 1, 29, 1, 36, 1, 40, 1}};
  for (auto& s : mSparks)
  {
    for (int i = 0; i < 4; ++i)
    {
      s.px[i] = s.x[i];
      s.py[i] = s.y[i];
      s.x[i] += kT[i][s.state];
      s.y[i] += kT[i][s.state + 1];
    }
    s.state += 2;
  }
  mSparks.erase(
    std::remove_if(
      mSparks.begin(),
      mSparks.end(),
      [](const Spark& s) { return s.state >= 0x17; }),
    mSparks.end());
  // Debris (2ea3): six frames
  for (auto& d : mDebris)
  {
    if (offscreen(d.yw, d.x8))
    {
      d.frame = 0;
      continue;
    }
    ++d.frame;
  }
  mDebris.erase(
    std::remove_if(
      mDebris.begin(),
      mDebris.end(),
      [](const Debris& d) { return d.frame == 0 || d.frame >= 7; }),
    mDebris.end());
  // Enemy shots (1be8): one unit (8 px) per update, gone on walls, the player
  // or leaving the window
  mShotAnim = (mShotAnim + 1) % 2;
  for (auto& s : mEnemyShots)
  {
    if (s.dir == 0)
      continue;
    if (touchesPlayer(s.x8, s.yw))
    {
      if (!p.pendingHit && p.immunity == 0)
      {
        // The shot also shoves Duke one unit in its direction when there is
        // room
        std::vector<Event> scratch;
        if (mSandbox.world.clear(
              s.dir == 1 ? Sensor::Left : Sensor::Right, p, scratch))
          p.x8 += s.dir == 1 ? -1 : 1;
      }
      p.pendingHit = true;
      s.dir = 0;
      continue;
    }
    s.px8 = s.x8;
    s.x8 += s.dir == 1 ? -1 : 1;
    if (
      wallAt(s.x8 + (s.dir == 2 ? 1 : 0), s.yw) ||
      wallAt(s.x8 + (s.dir == 2 ? 1 : 0), s.yw + 0x40) || offscreen(s.yw, s.x8))
    {
      s.dir = 0;
      continue;
    }
    s.fresh = false;
  }
  mEnemyShots.erase(
    std::remove_if(
      mEnemyShots.begin(),
      mEnemyShots.end(),
      [](const EnemyShot& s) { return s.dir == 0; }),
    mEnemyShots.end());
}

void PlayWorld::drawPools(
  std::vector<std::uint8_t>& out,
  int w,
  int h,
  int left,
  int top) const
{
  mShiftX = mShiftY = 0;
  for (const auto& e : mExplosions)
  {
    if (e.type == 4)
    { // balloon pop
      drawObjectPiece(out, w, h, left, top, 73, e.x8, e.yw);
      continue;
    }
    if (e.type == 3)
    { // large blast: a centre flash, then four pieces around it
      if (e.frame == 2)
        drawPiece(out, w, h, left, top, 207, e.x8, e.yw);
      if (e.frame == 4)
      {
        drawPiece(out, w, h, left, top, 208, e.x8 - 1, e.yw - 0x40);
        drawPiece(out, w, h, left, top, 209, e.x8 + 1, e.yw - 0x40);
        drawPiece(out, w, h, left, top, 210, e.x8 - 1, e.yw + 0x40);
        drawPiece(out, w, h, left, top, 211, e.x8 + 1, e.yw + 0x40);
      }
      continue;
    }
    const int base = e.type == 1 ? 41 : 89;
    drawPiece(out, w, h, left, top, base + (e.frame - 1), e.x8, e.yw);
  }
  for (const auto& s : mSparks)
    for (int i = 0; i < 4; ++i)
    {
      mShiftX =
        static_cast<int>(std::lround((s.x[i] - s.px[i]) * 8 * (mBlend - 1.0)));
      mShiftY = static_cast<int>(
        std::lround((s.y[i] - s.py[i]) / 8.0 * (mBlend - 1.0)));
      drawObjectPiece(out, w, h, left, top, s.piece[i], s.x[i], s.y[i]);
    }
  mShiftX = mShiftY = 0;
  for (const auto& d : mDebris)
    drawObjectPiece(out, w, h, left, top, 33 + (d.frame - 1), d.x8, d.yw);
  if (mOrbs.on)
    for (int k = 1; k < 10; ++k)
    {
      if (!mOrbs.live[k] || offscreen(mOrbs.y[k], mOrbs.x[k]))
        continue;
      mShiftX = static_cast<int>(
        std::lround((mOrbs.x[k] - mOrbs.px[k]) * 8 * (mBlend - 1.0)));
      mShiftY = static_cast<int>(
        std::lround((mOrbs.y[k] - mOrbs.py[k]) / 8.0 * (mBlend - 1.0)));
      drawPiece(
        out, w, h, left, top, 120 + mOrbs.frame[k], mOrbs.x[k], mOrbs.y[k]);
    }
  mShiftX = mShiftY = 0;
  for (const auto& s : mEnemyShots)
  {
    const int base = s.dir == 1 ? 39 : 41;
    const int piece = s.fresh ? (s.dir == 1 ? 48 : 49) : base + mShotAnim;
    mShiftX =
      static_cast<int>(std::lround((s.x8 - s.px8) * 8 * (mBlend - 1.0)));
    drawObjectPiece(out, w, h, left, top, piece, s.x8, s.yw, 3);
  }
  mShiftX = 0;
}

} // namespace duke1
