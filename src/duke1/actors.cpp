// Behaviour of the original actors, ported from the DOS game's code (see re-notes/dn1).
// Coordinates follow the original: x in 8 pixel units (the loader adds 2 for the frame border), y in map words
// (128 per 16 pixel row), cells are 16 pixels = 2 units wide.
#include "play.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace duke1 {

bool PlayWorld::wallAt(int x8, int yw) const {
  const int index = (x8 >> 1) - 1 + (yw >> 7) * 128;
  if (index < 0 || index >= 128 * 90) return true;
  return sandbox_.world.cells[static_cast<std::size_t>(index)] >= 0x1800;
}

bool PlayWorld::floorAt(int x8, int yw) const {
  const int row = (yw >> 7) * 128;
  const int a = (x8 >> 1) - 1 + row;
  const int b = (x8 & 1) ? (x8 >> 1) + row : a;
  auto solid = [&](int i) { return i < 0 || i >= 128 * 90 || sandbox_.world.cells[static_cast<std::size_t>(i)] >= 0x1800; };
  return solid(a) || solid(b);
}

bool PlayWorld::offscreen(int yw, int x8) const {
  return sandbox_.culled(x8, yw);  // same window as the player's shots
}

void PlayWorld::setActiveView(int left, int top, int w, int h) {
  auto& s = sandbox_;
  s.wideCull = true;
  s.cullX0 = left / 8 + 2 - 2;
  s.cullX1 = (left + w) / 8 + 2 + 2;
  s.cullY0 = top * 8 - 0x40;
  s.cullY1 = (top + h) * 8 + 0x40;
}

int PlayWorld::bulletHit(int yw, int x8) {
  for (auto& b : sandbox_.bullets) {
    if (b.kind == 0) continue;
    if (std::abs(b.x8 - x8) < 2 && b.yWords - yw < 0x80 && b.yWords - yw > -0x81) {
      const int kind = b.kind;
      b.kind = 0;
      if (kind == 1) return 1;
      if (kind == 5) return 2;
    }
  }
  return 0;
}

bool PlayWorld::touchesPlayer(int x8, int yw) const {
  const auto& p = sandbox_.player;
  const int dy = ((p.y >> 4) * 0x80 + p.cameraYWords) - yw + 2;
  return std::abs(p.x8 + p.cameraX8 - x8) < 2 && dy > 0x10 && dy < 0x180;
}

void PlayWorld::drawPiece(std::vector<std::uint8_t>& out, int w, int h, int left, int top, int piece, int x8, int yw) const {
  // Pieces are numbered across the ANIM files, 48 usable pieces each.
  const std::size_t bank = static_cast<std::size_t>(piece / 48), idx = static_cast<std::size_t>(piece % 48);
  if (bank >= 6 || idx >= sprites_[bank].size()) return;
  blit(out, (x8 - 2) * 8 - left + shiftX_, yw / 8 - top + shiftY_, sprites_[bank][idx].data(), 16, 0, 0, 16, 16, 0, 0, w, h, w, h);
}

void PlayWorld::drawInventory(std::vector<std::uint8_t>& out, int w, int h, int x, int y) const {
  for (int i = 0; i < 4; ++i) {
    if (!keys_[i]) continue;
    const std::size_t bank = 8, idx = static_cast<std::size_t>(24 + 1 + i - 1 + 0);
    if (idx >= sprites_[bank].size()) continue;
    blit(out, x + i * 16, y, sprites_[bank][idx].data(), 16, 0, 0, 16, 16, 0, 0, w, h, w, h);
  }
}

void PlayWorld::debugDump() const {
  std::printf("player x8=%d y=%d camX8=%d camYW=%d health=%d\n", sandbox_.player.x8, sandbox_.player.y,
              sandbox_.player.cameraX8, sandbox_.player.cameraYWords, sandbox_.player.health);
  for (const auto& a : actors_)
    if (!a.ported && a.alive && (a.open || std::getenv("DN1_ALL")))
      std::printf("generic %04X col=%d row=%d open=%d\n", a.id, a.col, a.row, a.open);
  for (const auto& a : actors_)
    if (a.ported && a.alive)
      std::printf("actor %04X col=%d row=%d state=%d x8=%d yw=%d\n", a.id, a.col, a.row, a.state, a.x8, a.yw);
}

void PlayWorld::drawObjectPiece(std::vector<std::uint8_t>& out, int w, int h, int left, int top, int piece, int x8, int yw,
                                int yOffsetPx) const {
  // OBJECT pieces are numbered across the three OBJECT files, 50 pieces each.
  const std::size_t bank = 6 + static_cast<std::size_t>(piece / 50), idx = static_cast<std::size_t>(piece % 50);
  if (bank >= 9 || idx >= sprites_[bank].size()) return;
  blit(out, (x8 - 2) * 8 - left + shiftX_, yw / 8 + yOffsetPx - top + shiftY_, sprites_[bank][idx].data(), 16, 0, 0, 16, 16, 0, 0, w, h, w, h);
}

void PlayWorld::spawnExplosion(int yw, int x8, int type) {
  explosions_.push_back({1, x8, yw, type});
  if (type == 1) sound(18);
  else if (type == 3) sound(33);
}
void PlayWorld::spawnDebris(int yw, int x8) { debris_.push_back({1, x8, yw}); }

void PlayWorld::spawnSparks(int yw, int x8) {
  static const int kVariants[4][4] = {{1, 2, 3, 4}, {4, 1, 2, 3}, {2, 4, 3, 1}, {3, 4, 2, 1}};
  sparkVariant_ = (sparkVariant_ + 1) % 4;
  Spark s{};
  s.state = 1;
  const int xs[4] = {x8, x8 + 1, x8, x8 + 1};
  const int ys[4] = {yw, yw, yw + 8, yw + 8};
  for (int i = 0; i < 4; ++i) {
    s.x[i] = xs[i];
    s.y[i] = ys[i];
    s.piece[i] = kVariants[sparkVariant_][i];
    s.px[i] = s.x[i];
    s.py[i] = s.y[i];
  }
  sparks_.push_back(s);
  sound(10);
}
void PlayWorld::spawnEnemyShot(int yw, int x8, int dir) {
  if (enemyShots_.size() < 8) {
    enemyShots_.push_back({dir, x8, yw, true, x8});
    sound(11);
  }
}

void PlayWorld::updateOrbs() {
  static const int kDx[39] = {0, -1, -1, -2, -2, -2, -1, -1, 0, 1, 1, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 0, -1, -1, -2, -2, -2, -1, 0, 0, 0, 0, 0, 0, 0, 0};
  static const int kDy[39] = {1, 1, 1, 1, 0, -1, -1, -1, -2, -1, -1, -1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, -1, -1, -2, -1, -1, -1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0};
  if (!orbs_.on) return;
  auto& p = sandbox_.player;
  bool clear = true;
  for (int k = 1; k < 10; ++k) {
    orbs_.x[k - 1] = orbs_.x[k];
    orbs_.y[k - 1] = orbs_.y[k];
    orbs_.px[k - 1] = orbs_.px[k];
    orbs_.py[k - 1] = orbs_.py[k];
    if (orbs_.live[k]) clear = false;
  }
  if (clear) {
    orbs_.on = false;
    snakesDone_ = true;  // the 10,000 for the whole chain is paid when the level is left
    return;
  }
  orbs_.px[9] = orbs_.x[9];
  orbs_.py[9] = orbs_.y[9];
  orbs_.x[9] += kDx[orbs_.step];
  orbs_.y[9] += kDy[orbs_.step] * 0x40;
  for (int k = 1; k < 10; ++k) {
    orbs_.frame[k] = orbs_.anim / 0xa0;
    if (!offscreen(orbs_.y[k], orbs_.x[k]) && orbs_.live[k] && touchesPlayer(orbs_.x[k], orbs_.y[k])) p.pendingHit = true;
    if (phase_ % 4 != 0) orbs_.anim += 0xa0;
    if (orbs_.anim == 0x460) orbs_.anim = 0;
    if (orbs_.live[k] && bulletHit(orbs_.y[k] - 0x80, orbs_.x[k])) {
      spawnExplosion(orbs_.y[k], orbs_.x[k], 3);
      addScore(1000, (orbs_.x[k] - 2) * 8, orbs_.y[k] / 8);
      spawnSparks(orbs_.y[k] - 0x80, orbs_.x[k]);
      orbs_.live[k] = false;
    }
  }
  if (++orbs_.step == 39) orbs_.step = 0;
}

void PlayWorld::updatePools() {
  updateOrbs();
  auto& p = sandbox_.player;
  // Explosions (2bcb): play seven frames, type 2 also hurts the player
  for (auto& e : explosions_) {
    if (offscreen(e.yw, e.x8)) { e.frame = 0; continue; }
    if (e.type != 3 && e.type != 1 && e.type != 4 && touchesPlayer(e.x8, e.yw)) p.pendingHit = true;
    ++e.frame;
  }
  explosions_.erase(std::remove_if(explosions_.begin(), explosions_.end(), [](const Explosion& e) { return e.frame == 0 || e.frame >= (e.type == 3 ? 5 : e.type == 4 ? 3 : 8); }),
                    explosions_.end());
  // Sparks (3297): four gems fly out along fixed paths
  static const int kT[4][24] = {
      {0, 0, -71, 0, -59, 0, -45, 0, -33, -1, -25, 0, -10, 0, -1, 0, 13, 0, 16, -1, 19, 0, 31, 0},
      {0, 0, -50, 1, -47, 0, -33, 1, -23, 0, -10, 1, 8, 0, 21, 0, 33, 0, 46, 1, 19, 0, 29, 0},
      {0, 0, 1, -1, 8, -1, 17, -1, 29, -1, 32, -1, 45, -1, 51, -1, 65, -1, 79, -1, 79, -1, 99, -1},
      {0, 0, 1, 1, 3, 1, 7, 1, 9, 1, 12, 1, 15, 1, 21, 1, 25, 1, 29, 1, 36, 1, 40, 1}};
  for (auto& s : sparks_) {
    for (int i = 0; i < 4; ++i) {
      s.px[i] = s.x[i];
      s.py[i] = s.y[i];
      s.x[i] += kT[i][s.state];
      s.y[i] += kT[i][s.state + 1];
    }
    s.state += 2;
  }
  sparks_.erase(std::remove_if(sparks_.begin(), sparks_.end(), [](const Spark& s) { return s.state >= 0x17; }), sparks_.end());
  // Debris (2ea3): six frames
  for (auto& d : debris_) {
    if (offscreen(d.yw, d.x8)) { d.frame = 0; continue; }
    ++d.frame;
  }
  debris_.erase(std::remove_if(debris_.begin(), debris_.end(), [](const Debris& d) { return d.frame == 0 || d.frame >= 7; }), debris_.end());
  // Enemy shots (1be8): one unit (8 px) per update, gone on walls, the player or leaving the window
  shotAnim_ = (shotAnim_ + 1) % 2;
  for (auto& s : enemyShots_) {
    if (s.dir == 0) continue;
    if (touchesPlayer(s.x8, s.yw)) {
      if (!p.pendingHit && p.immunity == 0) {
        // The shot also shoves Duke one unit in its direction when there is room
        std::vector<Event> scratch;
        if (sandbox_.world.clear(s.dir == 1 ? Sensor::Left : Sensor::Right, p, scratch)) p.x8 += s.dir == 1 ? -1 : 1;
      }
      p.pendingHit = true;
      s.dir = 0;
      continue;
    }
    s.px8 = s.x8;
    s.x8 += s.dir == 1 ? -1 : 1;
    if (wallAt(s.x8 + (s.dir == 2 ? 1 : 0), s.yw) || wallAt(s.x8 + (s.dir == 2 ? 1 : 0), s.yw + 0x40) || offscreen(s.yw, s.x8)) {
      s.dir = 0;
      continue;
    }
    s.fresh = false;
  }
  enemyShots_.erase(std::remove_if(enemyShots_.begin(), enemyShots_.end(), [](const EnemyShot& s) { return s.dir == 0; }), enemyShots_.end());
}

void PlayWorld::drawPools(std::vector<std::uint8_t>& out, int w, int h, int left, int top) const {
  shiftX_ = shiftY_ = 0;
  for (const auto& e : explosions_) {
    if (e.type == 4) {  // balloon pop
      drawObjectPiece(out, w, h, left, top, 73, e.x8, e.yw);
      continue;
    }
    if (e.type == 3) {  // large blast: a centre flash, then four pieces around it
      if (e.frame == 2) drawPiece(out, w, h, left, top, 207, e.x8, e.yw);
      if (e.frame == 4) {
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
  for (const auto& s : sparks_)
    for (int i = 0; i < 4; ++i) {
      shiftX_ = static_cast<int>(std::lround((s.x[i] - s.px[i]) * 8 * (blend_ - 1.0)));
      shiftY_ = static_cast<int>(std::lround((s.y[i] - s.py[i]) / 8.0 * (blend_ - 1.0)));
      drawObjectPiece(out, w, h, left, top, s.piece[i], s.x[i], s.y[i]);
    }
  shiftX_ = shiftY_ = 0;
  for (const auto& d : debris_) drawObjectPiece(out, w, h, left, top, 33 + (d.frame - 1), d.x8, d.yw);
  if (orbs_.on)
    for (int k = 1; k < 10; ++k) {
      if (!orbs_.live[k] || offscreen(orbs_.y[k], orbs_.x[k])) continue;
      shiftX_ = static_cast<int>(std::lround((orbs_.x[k] - orbs_.px[k]) * 8 * (blend_ - 1.0)));
      shiftY_ = static_cast<int>(std::lround((orbs_.y[k] - orbs_.py[k]) / 8.0 * (blend_ - 1.0)));
      drawPiece(out, w, h, left, top, 120 + orbs_.frame[k], orbs_.x[k], orbs_.y[k]);
    }
  shiftX_ = shiftY_ = 0;
  for (const auto& s : enemyShots_) {
    const int base = s.dir == 1 ? 39 : 41;
    const int piece = s.fresh ? (s.dir == 1 ? 48 : 49) : base + shotAnim_;
    shiftX_ = static_cast<int>(std::lround((s.x8 - s.px8) * 8 * (blend_ - 1.0)));
    drawObjectPiece(out, w, h, left, top, piece, s.x8, s.yw, 3);
  }
  shiftX_ = 0;
}

void PlayWorld::teleportToCell(int col, int row) {
  auto& p = sandbox_.player;
  p.cameraX8 = std::clamp(2 * col - 14, 0, 230);
  p.x8 = 2 * col - p.cameraX8;
  p.cameraYWords = std::max(0, 128 * row - 384);
  p.y = 16 * row + 16 - p.cameraYWords / 8;
  prevX_ = worldX();
  prevY_ = worldY();
}

void PlayWorld::initPorted(ActorInstance& a) {
  struct PrevInit { ActorInstance& a; ~PrevInit() { a.prevX8 = a.x8; a.prevYw = a.yw; } } prevInit{a};
  switch (a.id) {
    case 0x3010:  // walking cylinder robot
      a.ported = true;
      a.state = 1;
      a.x8 = a.col * 2 + 2;
      a.yw = a.row * 128;
      break;
    case 0x302A:  // sign that drops on Duke when he walks under it
      a.ported = true;
      a.state = 1;
      a.x8 = a.col * 2 + 2;
      a.yw = a.row * 128;
      break;
    case 0x3007:  // rocket on a launch pad: shoot it and it flies up the shaft
      a.ported = true;
      a.state = 1;
      a.x8 = a.col * 2 + 2;
      a.yw = a.row * 128;
      break;
    case 0x3042: case 0x3043:  // last boss
      a.ported = true;
      a.state = 1;
      a.x8 = a.col * 2 + 2;
      a.yw = a.row * 128;
      a.hits = 10;
      boss_ = Boss{};
      break;
    case 0x3034:  // card reader: switches off every force field when Duke uses the card
      a.ported = true;
      a.state = 1;
      a.x8 = a.col * 2 + 2;
      a.yw = a.row * 128;
      break;
    case 0x3035:  // wall machine: needs the special item
      a.ported = true;
      a.state = 1;
      a.x8 = a.col * 2 + 2;
      a.yw = a.row * 128;
      a.v0 = 0;  // switched on
      a.v1 = 0;  // alarm
      break;
    case 0x3036:  // blast path: turns the cells to its right into 0x2fe0 one by one
      a.ported = true;
      a.state = 1;
      a.x8 = a.col * 2 + 2;
      a.yw = a.row * 128;
      a.v0 = a.x8;
      sandbox_.world.cells[a.cellIndex] = 0x2fe0;
      break;
    case 0x3013:  // chain of nine orbs
      a.ported = true;
      a.state = 1;
      a.x8 = a.col * 2 + 2;
      a.yw = a.row * 128;
      orbs_ = OrbChain{};
      orbs_.on = true;
      for (int k = 1; k <= 9; ++k) {
        orbs_.x[k] = orbs_.px[k] = a.col * 2 + 2 * k;
        orbs_.y[k] = orbs_.py[k] = a.row * 128 + 2;
        orbs_.live[k] = true;
      }
      break;
    case 0x3019: {  // brittle floor: collapses when Duke steps on it a second time
      a.ported = true;
      a.state = 1;
      a.x8 = a.col * 2 + 2;
      a.yw = a.row * 128;
      a.v1 = 0;
      auto& cells = sandbox_.world.cells;
      const auto idx = a.cellIndex;
      int j = 0;
      while (idx + j + 1 < cells.size() && cells[idx + j + 1] < 0x1800) {
        cells[idx + j] = 0x21a0;
        cells[idx + j + 1] = 0x21c0;
        j += 2;
      }
      cells[idx] = 0x21a0;
      a.v0 = j;  // width in cells
      break;
    }
    case 0x302B:  // electric column: 10 hits to destroy, deadly to touch
      a.ported = true;
      a.state = 1;
      a.x8 = a.col * 2 + 2;
      a.yw = a.row * 128;
      a.v0 = 0;   // 0 = intact, >0 = counting while it burns out
      a.v1 = 0;   // flash counter after a hit
      a.hits = 10;
      break;
    case 0x302F: case 0x3030:  // transporter pads (a pair: each sends Duke to the other)
      a.ported = true;
      a.state = 1;
      a.x8 = a.col * 2 + 2;
      a.yw = a.row * 128;
      a.v0 = 0;  // teleport countdown
      break;
    case 0x3015: case 0x3052:  // red container with a soda can (3052: already open)
    case 0x3018: case 0x3051:  // red container with food (3051: already open)
      a.ported = true;
      a.state = (a.id == 0x3052 || a.id == 0x3051) ? 2 : 1;
      a.x8 = a.col * 2 + 2;
      a.yw = a.row * 128;
      a.v0 = 0;
      break;
    case 0x3022:  // helicopter: hovers over Duke, swoops, needs 4 hits
      a.ported = true;
      a.state = 1;
      a.x8 = a.col * 2 + 2;
      a.yw = a.row * 128;
      a.v0 = 0;       // mode / hop counter
      a.v1 = 0;       // swoop counter
      a.hits = 4;
      break;
    case 0x303B:  // pink creature: walks, latches on when it touches Duke
      a.ported = true;
      a.state = 1;
      a.x8 = a.col * 2 + 2;
      a.yw = a.row * 128;
      a.v0 = 0;  // sub-state / animation counter
      a.v1 = -1;
      break;
    case 0x3023:  // blue box with a rising 10,000 point item
      a.ported = true;
      a.state = 1;
      a.x8 = a.col * 2 + 2;
      a.yw = a.row * 128;
      a.v0 = 0;
      break;
    case 0x3024:  // wall target: turns to face Duke, shot for 100 points
    case 0x3058: case 0x3059:  // metal vertical, hurts on touch
    case 0x3029:  // colour ball in a crate
    case 0x3031:  // bobbing heart-shaped hazard
    case 0x3057:  // heart that drops and explodes
    case 0x3037: case 0x3038: case 0x3039: case 0x303A:  // letters D U K E in grey crates
      a.ported = true;
      a.state = 1;
      a.x8 = a.col * 2 + 2;
      a.yw = a.row * 128;
      a.v0 = a.id == 0x3031 ? -6 : 0;  // bob counter, else opened
      break;
    case 0x3002: case 0x3004:  // conveyor belt start (3002 pushes left, 3004 pushes right)
      a.ported = true;
      a.state = 1;
      a.x8 = a.col * 2 + 2;
      a.yw = a.row * 128;
      a.v0 = a.id == 0x3004 ? 1 : 0;  // direction
      a.v1 = 1;                         // width in cells, set below
      {
        int end = a.col + 1;
        while (end < 128) {
          const auto cell = sandbox_.world.cells[static_cast<std::size_t>(a.row * 128 + end)];
          if (cell == 0x3003 || cell == 0x3005) break;
          ++end;
        }
        a.v1 = end - a.col + 1;
      }
      break;
    case 0x3001:  // elevator platform: Up while standing on it raises it one row
      a.ported = true;
      a.state = 1;
      a.x8 = a.col * 2 + 2;
      a.yw = a.row * 128;
      a.v0 = 0;
      a.v1 = a.cellIndex >= 128 ? sandbox_.world.cells[a.cellIndex - 128] : 0;  // the tile it leaves behind
      break;
    case 0x3011:  // exit door
      a.ported = true;
      a.state = 1;
      a.x8 = a.col * 2 + 2;
      a.yw = a.row * 128 + 2;
      a.v0 = 0;
      break;
    case 0x3044: case 0x3045: case 0x3046: case 0x3047:  // keys
    case 0x3048: case 0x3049: case 0x304A: case 0x304B:  // controls that need a key
    case 0x304C: case 0x304D: case 0x304E: case 0x304F:  // doors
      a.ported = true;
      a.state = 1;
      a.x8 = a.col * 2 + 2;
      a.yw = a.row * 128;
      a.v0 = 0;  // doors: opening animation counter (0 = closed); controls: 0 = unused, 1 = used
      break;
    case 0x300C:  // large wheeled robot: jumps and shoots (3 hits)
    case 0x300D:  // blue walking robot (2 hits)
      a.ported = true;
      a.state = 1;
      a.x8 = a.col * 2 + 2;
      a.yw = a.row * 128;
      a.v0 = 0;      // 300C: facing/dir (0 = asleep, 1 = left, 2 = right)
      a.v1 = -1;     // frame drawn this update (-1 = none)
      a.hits = a.id == 0x300C ? 3 : 2;
      break;
    case 0x300E:  // four-piece walker, armoured except during the first part of its cycle (2 hits)
      a.ported = true;
      a.state = 1;
      a.x8 = a.col * 2 + 2;
      a.yw = a.row * 128;
      a.v1 = -1;
      a.hits = 2;
      break;
    case 0x301A:  // electric arc (horizontal)
    case 0x3021:  // electric field (vertical), pushes Duke
    case 0x302C:  // spikes
      a.ported = true;
      a.state = 1;
      a.x8 = a.col * 2 + 2;
      a.yw = a.row * 128;
      break;
    case 0x3009:  // fire jet, shoots right
    case 0x300A:  // fire jet, shoots left
      a.ported = true;
      a.state = 1;  // counter: 1..0x41, flame while 0x19..0x2c
      a.x8 = a.col * 2 + 2;
      a.yw = a.row * 128;
      break;
    case 0x303C:  // fire creature, patrols
      a.ported = true;
      a.state = 1;  // 1 = walking left, 2 = walking right
      a.x8 = a.col * 2 + 2;
      a.yw = a.row * 128;
      break;
    case 0x3012:  // explosive barrel (TNT in a grey container)
      a.ported = true;
      a.state = 1;  // counter
      a.x8 = a.col * 2 + 2;
      a.yw = a.row * 128;
      a.v0 = 0;      // opened (shot)
      a.v1 = 3;      // bit 0: left blast chain still going, bit 1: right
      break;
    case 0x3000:  // grey crate
      a.ported = true;
      a.state = 1;
      a.x8 = a.col * 2 + 2;
      a.yw = a.row * 128;
      break;
    case 0x300B:  // hovering robot that shoots
      a.ported = true;
      a.state = 1;
      a.x8 = a.col * 2 + 2;
      a.yw = a.row * 128;
      a.v0 = 0;
      break;
    case 0x3016:  // green flyer, variant 1 (copies the cell to its right)
    case 0x3017:  // green flyer, variant 3
      a.ported = true;
      a.state = a.id == 0x3016 ? 1 : 3;
      a.x8 = a.col * 2 + 2;
      a.yw = a.row * 128;
      break;
    default:
      break;
  }
}

void PlayWorld::updatePorted() {
  for (auto& a : actors_) {
    a.prevX8 = a.x8;
    a.prevYw = a.yw;
  }
  bool any3010 = false;
  for (auto& a : actors_) any3010 = any3010 || (a.alive && a.id == 0x3010);
  if (any3010) {
    famAnim3010_ += 0xa0;
    if (famAnim3010_ == 0x1e0) famAnim3010_ = 0;
  }

  bool anyFlyer = false;
  for (auto& a : actors_) anyFlyer = anyFlyer || (a.alive && (a.id == 0x3016 || a.id == 0x3017));
  if (anyFlyer) {
    famAnimUp_ += 0xa0;
    famAnimDown_ -= 0xa0;
    if (famAnimDown_ == -0xa0) famAnimDown_ = 0x1e0;
    if (famAnimUp_ == 0x280) famAnimUp_ = 0;
  }

  bool any300B = false;
  for (auto& a : actors_) any300B = any300B || (a.alive && a.id == 0x300B);
  if (any300B) {
    hoverTick_ = (hoverTick_ + 1) % 10;
    hoverAnim_ = (hoverAnim_ + 1) % 4;
  }

  bool any300E = false;
  for (auto& a : actors_) any300E = any300E || (a.alive && a.id == 0x300E);
  if (any300E) {
    ++walkerAnim_;
    walkerPhase_ = (walkerPhase_ + 1) % 0x3c;
    if (walkerPhase_ < 0x19 && walkerAnim_ > 3) walkerAnim_ = 0;
    if (walkerPhase_ > 0x18 && walkerAnim_ > 7) walkerAnim_ = 4;
  }

  auto& p = sandbox_.player;
  for (auto& a : actors_) {
    // (the boss has a meaningful state 0: it is beaten and leaving)
    if (!a.alive || !a.ported || (a.state == 0 && a.id != 0x3042 && a.id != 0x3043)) continue;
    switch (a.id) {
      case 0x302A: {
        if (offscreen(a.yw, a.x8)) {
          if (a.state > 0xf) { a.state = 0; a.alive = false; }
          break;
        }
        const int px = p.x8 + p.cameraX8;
        const int feet = (p.y >> 4) * 0x80 + p.cameraYWords;
        if (a.state > 1) {
          if (floorAt(a.x8 + 1, a.yw + 0x80)) {
            spawnSparks(a.yw, a.x8 + 1);
            spawnDebris(a.yw, a.x8 + 1);
            a.state = 0;
            a.alive = false;
            break;
          }
          ++a.state;
        }
        if (a.state < 2 && a.x8 - 1 <= px && px <= a.x8 + 3 && feet - a.yw < 0x400 && feet != a.yw && a.yw <= feet) a.state = 2;
        if (a.state == 0x10) sound(32);
        if (a.state > 0xf) {
          a.yw += 0x60;
          if (touchesPlayer(a.x8, a.yw) || touchesPlayer(a.x8 + 2, a.yw)) p.pendingHit = true;
          if (bulletHit(a.yw - 0x40, a.x8) || bulletHit(a.yw - 0x40, a.x8 + 2)) {
            spawnSparks(a.yw, a.x8 + 1);
            spawnDebris(a.yw, a.x8 + 1);
            spawnExplosion(a.yw, a.x8 + 1, 1);
            addScore(500, (a.x8 - 2) * 8, a.yw / 8);
            a.state = 0;
            a.alive = false;
          }
        }
        break;
      }
      case 0x3007: {
        // 1 waiting, 2-9 igniting, 10 flying up, 0 gone
        if (a.state == 1 && bulletHit(a.yw, a.x8)) {
          a.state = 2;
          sound(30);
          addScore(200, (a.x8 - 2) * 8, a.yw / 8);
        }
        if (a.state > 1 && a.state < 10) {
          if (a.state == 9) {
            auto& cells = sandbox_.world.cells;
            const auto below = static_cast<std::size_t>((a.yw / 128 + 1) * 128 + a.col);
            if (below + 128 < cells.size()) cells[below] = cells[below + 128];  // the launch pad is gone
            spawnSparks(a.yw + 0x80, a.x8);
          }
          if (a.state == 3) spawnExplosion(a.yw + 0x80, a.x8, 3);
          ++a.state;
        }
        if (a.state == 10) a.yw -= 0x80;
        if (a.state == 10 && floorAt(a.x8, a.yw - 0x200)) {
          spawnSparks(a.yw - 0x200, a.x8 - 3);
          spawnSparks(a.yw - 0x180, a.x8 + 2);
          spawnSparks(a.yw - 0x180, a.x8 + 1);
          spawnSparks(a.yw - 0x100, a.x8 - 1);
          spawnSparks(a.yw - 0x80, a.x8);
          const auto hit = static_cast<std::size_t>((a.yw / 128 - 4) * 128 + a.col);
          if (hit + 128 < sandbox_.world.cells.size()) sandbox_.world.cells[hit] = sandbox_.world.cells[hit + 128];
          a.state = 0;
          a.alive = false;
        }
        break;
      }
      case 0x3042: case 0x3043: {
        auto& b = boss_;
        if (b.deadCount != 0) {
          if (++b.deadCount == 10 && !levelComplete_) {
            levelComplete_ = true;
            addScore(100000, (a.x8 - 2) * 8, a.yw / 8);  // the ending bonus
          }
          break;
        }
        b.anim ^= 1;
        const int px = p.x8 + p.cameraX8;
        if (a.state == 2) {  // destroyed: sinks to the floor in a shower of explosions
          a.yw += 0x20;
          switch (phase_ % 4) {
            case 0: spawnSparks(a.yw, a.x8 + 2); break;
            case 1: spawnExplosion(a.yw, a.x8 + 2, 1); break;
            case 2: spawnSparks(a.yw, a.x8); break;
            default: spawnExplosion(a.yw, a.x8 + 2, 3); break;
          }
          if (floorAt(a.x8 + 1, a.yw)) { b.deadCount = 1; sound(41); }
        } else if (a.state == 0) {  // beaten: Duke is frozen while the boss rises out of the level
          if (!offscreen(a.yw, a.x8) && b.freeze == 0) {
            b.freeze = 1;
            frozen_ = true;
            sound(40);
          }
          if (b.freeze > 0x1e && !levelComplete_) {
            levelComplete_ = true;
            addScore(100000, (a.x8 - 2) * 8, a.yw / 8);  // the ending bonus
          }
          if (b.freeze != 0) ++b.freeze;
          if (b.freeze != 0 && a.yw > 0x100 && p.cameraYWords > 0x100) {
            a.yw -= 0x80;
            p.cameraYWords -= 0x80;
          }
        } else {
          static const int kJump[12] = {128, 92, 64, 32, 16, 8, -8, -16, -32, -64, -92, -128};
          if (b.jumping == 0) {
            ++b.cnt;
            if ((b.period >> 1) == b.cnt) {
              if (px - 2 < a.x8) spawnEnemyShot(a.yw - 0x20, a.x8, 1);
              else spawnEnemyShot(a.yw - 0x20, a.x8 + 3, 2);
            }
            if (b.period < b.cnt) {
              b.cnt = 0;
              b.step = 0;
              b.jumping = 1;
              b.dir = px - 2 < a.x8 ? 1 : 0;
            }
          } else {
            a.yw += kJump[b.step];
            if (b.dir == 0) {
              if (!wallAt(a.x8 + 3, a.yw - 0x40)) ++a.x8;
            } else if (!wallAt(a.x8 - 1, a.yw - 0x40)) {
              --a.x8;
            }
            if (++b.step == 12) {
              b.jumping = 0;
              b.period = (((phase_ % 4) * 0x20) >> 1) + 1;
              b.cnt = 0;
            }
          }
          if (bulletHit(a.yw - 0x40, a.x8 + 1)) {
            spawnExplosion(a.yw - 0x5c, a.x8 + 2, 3);
            --a.hits;
          }
          if (phase_ % 4 != 0 && a.hits < 10) {
            spawnDebris(a.yw - 0x20, a.x8 + 2);
            if (a.hits < 6) spawnDebris(a.yw - 0x40, a.x8 + 1);
            if (a.hits < 2) spawnDebris(a.yw - 0x40, a.x8 + 3);
          }
          if (a.hits == 1 && a.id == 0x3043) a.state = 0;
          else if (a.hits <= 0) a.state = 2;
        }
        if (a.state != 2 || b.deadCount == 0) {
          // Touching any part hurts
          for (const int dx : {0, 2})
            if (touchesPlayer(a.x8 + dx, a.yw) || touchesPlayer(a.x8 + dx, a.yw - 0x80)) p.pendingHit = true;
        }
        break;
      }
      case 0x3034: {
        if (a.state != 1 || offscreen(a.yw, a.x8)) break;
        if (touchesPlayer(a.x8, a.yw) && hasCard_ && sandbox_.input.up) {
          for (auto& f : actors_)
            if (f.id == 0x3021) { f.state = 0; f.alive = false; }
          a.state = 0;
          hasCard_ = false;
          sound(31);
        }
        break;
      }
      case 0x3035: {
        if (offscreen(a.yw, a.x8)) { a.v1 = 0; break; }
        if (touchesPlayer(a.x8, a.yw) && sandbox_.input.up && a.v0 == 0) {
          if (!hasItem_) a.v1 = 1;
          else { a.v0 = 1; blast_ = true; sound(5); }
        }
        if (a.v1) {
          if (phase_ % 4 == 0) spawnEnemyShot(a.yw - 0x24, a.x8 + 2, 1);
          if (phase_ % 4 == 2) spawnEnemyShot(a.yw - 0x24, a.x8 - 2, 2);
        }
        break;
      }
      case 0x3036: {
        if (a.state != 1 || !blast_) break;
        spawnExplosion(a.yw, a.v0 + 2, 3);
        const auto idx = static_cast<std::size_t>(a.row * 128 + (a.v0 >> 1));
        if (idx >= sandbox_.world.cells.size() || sandbox_.world.cells[idx] == 0x2fe0) {
          a.state = 0;
          blast_ = false;
        } else {
          sandbox_.world.cells[idx] = 0x2fe0;
        }
        a.v0 += 2;
        break;
      }
      case 0x3019: {
        if (a.state != 1) break;
        const int px = p.x8 + p.cameraX8;
        const int dy = ((p.y >> 4) * 0x80 + p.cameraYWords) - (a.yw - 0x20) + 2;
        const bool on = px > 2 * a.col && px < 2 * a.col + 2 + 2 * a.v0 && dy > 0 && dy < 0x180;
        if (!on) {
          if (a.v1 == 1) a.v1 = 2;
        } else {
          if (a.v1 == 2) {
            auto& cells = sandbox_.world.cells;
            for (int i = 0; i < a.v0; ++i) {
              const auto idx = a.cellIndex + static_cast<std::size_t>(i);
              if (idx >= 128) cells[idx] = cells[idx - 128];
              if (i % 2 == 0) spawnExplosion(a.yw, 2 * a.col + 4 + 2 * i, 1);
              else spawnSparks(a.yw, 2 * a.col + 4 + 2 * i);
            }
            a.state = 0;
            a.alive = false;
            sound(21);
          }
          a.v1 = 1;
        }
        break;
      }
      case 0x302B: {
        if (offscreen(a.yw, a.x8)) break;
        if (a.v0 == 0) {
          bool hit = false;
          for (int dy : {0, -0x80, -0x100}) {
            if (bulletHit(a.yw + dy, a.x8)) {
              --a.hits;
              spawnSparks(a.yw + dy, a.x8);
              hit = true;
              if (a.hits <= 0) a.v0 = 1;
            }
          }
          a.v1 = hit ? 1 : 0;
          if (a.v0 == 0) {
            // Contact box a little wider than the generic one so walking under or against the column counts
            const int dx = std::abs(p.x8 + p.cameraX8 - a.x8);
            const int dyc = ((p.y >> 4) * 0x80 + p.cameraYWords) - a.yw + 2;
            if (dx <= 2 && dyc > 0x10 && dyc < 0x1c0) p.health = 0;  // high voltage
          }
        } else {
          ++a.v0;
          if (phase_ % 4 == 0 || phase_ % 4 == 2) spawnDebris(a.yw, a.x8);
          if (a.v0 > 24) {
            a.state = 0;
            a.alive = false;
            const auto idx = a.cellIndex;
            sandbox_.world.cells[idx] = idx > 0 ? sandbox_.world.cells[idx - 1] : 0;
          }
        }
        break;
      }
      case 0x302F: case 0x3030: {
        const auto& in = sandbox_.input;
        const ActorInstance* partner = nullptr;
        for (const auto& o : actors_)
          if (o.alive && o.id == (a.id == 0x302F ? 0x3030 : 0x302F)) { partner = &o; break; }
        if (!partner) break;
        // Countdown as in the original: 8 on activation, the jump happens on the update that finds it at 1
        if (a.v0 == 1) {
          // Move Duke to the other pad, keeping the camera rules happy
          const int targetX = partner->x8 + 2;
          p.cameraX8 = std::clamp(targetX - p.x8, 0, 230);
          p.x8 = targetX - p.cameraX8;
          p.cameraYWords = std::max(0, partner->yw - 0x300);
          p.y = std::max(0, (partner->yw - p.cameraYWords) / 8);
          if (p.y > 150) { p.cameraYWords += (p.y - 150) * 8 / 128 * 128; p.y = std::max(0, (partner->yw - p.cameraYWords) / 8); }
          p.verticalScroll = true;
          a.v0 = 0;
          upConsumed_ = true;       // the key press is used up: it has to be pressed again
          discontinuity_ = true;    // nothing should be blended across the jump
        } else if (a.v0 > 1) {
          --a.v0;
        }
        if (a.v0 == 0 && !offscreen(a.yw, a.x8) && touchesPlayer(a.x8 + 2, a.yw) && in.up && !upConsumed_ &&
            !in.left && !in.right && !in.jump) {
          a.v0 = 8;
          sound(7);
        }
        break;
      }
      case 0x3015: case 0x3052: case 0x3018: case 0x3051: {
        const bool soda = a.id == 0x3015 || a.id == 0x3052;
        if (soda && a.state > 2 && offscreen(a.yw, a.x8)) { a.state = 0; a.alive = false; sandbox_.world.cells[a.cellIndex] = a.under; break; }
        if (offscreen(a.yw, a.x8)) break;
        if (!floorAt(a.x8, a.yw + 0x80) && (a.state < 3 || !soda)) a.yw += 0x80;
        if (soda && a.state > 2) {  // a shot can sends the soda flying upwards (food just keeps falling)
          if (phase_ % 4 != 0) a.yw -= 0x40;
          if (floorAt(a.x8, a.yw - 0x40)) {
            spawnExplosion(a.yw, a.x8, 1);
            a.state = 0; a.alive = false;
            sandbox_.world.cells[a.cellIndex] = a.under;
            break;
          }
          if (touchesPlayer(a.x8, a.yw)) {
            addScore(1000, (a.x8 - 2) * 8, a.yw / 8);
            sound(16);
            a.state = 0; a.alive = false;
            sandbox_.world.cells[a.cellIndex] = a.under;
            break;
          }
        }
        if (bulletHit(a.yw, a.x8)) {
          if (a.state == 1) {
            spawnSparks(a.yw, a.x8);
            a.state = 2;
            if (!floorAt(a.x8, a.yw + 0x80)) addScore(500, (a.x8 - 2) * 8, a.yw / 8);
          } else if (a.state == 2) {
            a.state = 3;
            if (soda) sound(2);  // the can is hit and sent flying
          }
        }
        if (a.state >= 2 && touchesPlayer(a.x8, a.yw) && (soda ? a.state == 2 : true)) {
          if (soda) { addScore(200, (a.x8 - 2) * 8, a.yw / 8); if (p.health < 9) ++p.health; }
          else if (a.state == 2) { addScore(100, (a.x8 - 2) * 8, a.yw / 8); if (p.health < 9) ++p.health; }
          else { addScore(200, (a.x8 - 2) * 8, a.yw / 8); p.health = std::min(9, p.health + 2); }
          sound(1);
          a.state = 0; a.alive = false;
          sandbox_.world.cells[a.cellIndex] = a.under;
        }
        break;
      }
      case 0x3022: {
        const int playerX = p.x8 + p.cameraX8;
        const int step = phase_ % 4;
        const bool moveNow = step == 0 || step == 2;
        if (!offscreen(a.yw, a.x8) && a.v0 == 0) a.v0 = floorAt(a.x8 + 3, a.yw + 0x80) ? 0x18 : 1;
        if (a.v0 == 0) break;
        if (a.v0 > 2) { --a.v0; a.yw -= 0x10; }
        if (a.v1 > 1 && a.v0 < 3) {
          a.yw += 0x10;
          ++a.v1;
          if (a.v1 > 0x17 || floorAt(a.x8 + 3, a.yw + 0x80)) { a.v0 = 0x18; a.v1 = 0; }
        }
        if (playerX < a.x8 + 3) {
          a.state = 2;
          if (moveNow && !wallAt(a.x8 - 2, a.yw)) --a.x8;
        } else if (a.x8 + 3 < playerX) {
          a.state = 3;
          if (moveNow && !wallAt(a.x8 + 8, a.yw)) ++a.x8;
        } else {
          if (a.v0 < 3 && a.v1 == 0) a.v1 = 2;
          a.state = 1;
        }
        if ((bulletHit(a.yw, a.x8) || bulletHit(a.yw, a.x8 + 8) || bulletHit(a.yw - 0x80, a.x8 + 4)) && a.hits > 0) --a.hits;
        if (a.hits < 4 && step == 0) spawnDebris(a.yw - 0x60, a.x8 + 3);
        if (a.hits < 3 && step == 1) spawnDebris(a.yw - 0x60, a.x8);
        if (a.hits < 2 && step == 2) spawnDebris(a.yw - 0x80, a.x8 + 2);
        if (a.hits == 0) {
          if (!floorAt(a.x8, a.yw + 0x80) && !floorAt(a.x8 + 8, a.yw + 0x80)) {
            a.yw += 0x20;  // the wreck falls
          } else {
            spawnExplosion(a.yw - 0x20, a.x8 + 1, 1);
            spawnExplosion(a.yw - 0x40, a.x8 + 7, 1);
            spawnExplosion(a.yw - 0x80, a.x8 + 2, 1);
            spawnExplosion(a.yw - 0x80, a.x8 + 3, 1);
            spawnExplosion(a.yw - 0x20, a.x8 + 5, 1);
            for (int k = 0; k < 5; ++k) spawnSparks(a.yw - (k % 2) * 0x40, a.x8 + 2 * (k / 2) + (k % 2) * 2);
            addScore(5000, (a.x8 - 2) * 8, a.yw / 8);
            a.state = 0;
            a.alive = false;
            sandbox_.world.cells[a.cellIndex] = a.under;
          }
        } else if (touchesPlayer(a.x8 + 2, a.yw - 0x40) || touchesPlayer(a.x8 + 4, a.yw - 0x40)) {
          p.pendingHit = true;
        }
        break;
      }
      case 0x303B: {
        a.v1 = -1;
        const int step = phase_ % 4;
        const bool moveNow = step == 0 || step == 2;
        if (a.state > 2) {
          static const int kBiteFrame[4] = {0, 6, 3, 7};
          a.v1 = kBiteFrame[a.v0 & 3];
          ++a.state;
          a.v0 = (a.v0 + 1) % 4;
          if (a.state == 0x14) {
            a.state = 0;
            a.alive = false;
            sandbox_.world.cells[a.cellIndex] = a.under;
            spawnDebris(a.yw, a.x8);
            spawnDebris(a.yw - 0x80, a.x8);
            spawnDebris(a.yw - 0x40, a.x8 - 1);
            spawnDebris(a.yw - 0x40, a.x8 + 1);
            spawnDebris(a.yw - 0xc0, a.x8 + 1);
            spawnDebris(a.yw, a.x8 - 1);
            addScore(5000, (a.x8 - 2) * 8, a.yw / 8);
          }
          break;
        }
        if (!floorAt(a.x8, a.yw + 0x80)) {
          a.yw += 0x40;
        } else if (wallAt(a.x8 - 1, a.yw) && a.state == 1) {
          a.state = 2;
          a.v0 = 0;
        } else if (wallAt(a.x8 + 2, a.yw) && a.state == 2) {
          a.state = 1;
          a.v0 = 0;
        }
        if (a.v0 == 0) {
          a.v1 = a.state == 1 ? 6 : 7;
          if (moveNow) a.v0 = 1;
        } else {
          if (a.state == 1) {
            if (moveNow) --a.x8;
            a.v1 = a.v0 < 3 ? a.v0 : 5 - a.v0;
            if (moveNow) {
              if (floorAt(a.x8, a.yw + 0x80)) ++a.v0;
              if (a.v0 == 6) a.v0 = 1;
            }
          }
          if (a.state == 2) {
            if (moveNow) ++a.x8;
            a.v1 = a.v0 < 3 ? a.v0 + 2 : 8 - a.v0;
            if (moveNow) {
              if (floorAt(a.x8, a.yw + 0x80)) ++a.v0;
              if (a.v0 == 6) a.v0 = 1;
            }
          }
        }
        if (bulletHit(a.yw, a.x8)) {
          a.state = 0;
          a.alive = false;
          sandbox_.world.cells[a.cellIndex] = a.under;
          spawnDebris(a.yw, a.x8);
          spawnDebris(a.yw - 0x80, a.x8);
          spawnDebris(a.yw - 0x40, a.x8 - 1);
          spawnDebris(a.yw - 0x40, a.x8 + 1);
          spawnDebris(a.yw - 0xc0, a.x8 + 1);
          spawnDebris(a.yw, a.x8 - 1);
          addScore(200, (a.x8 - 2) * 8, a.yw / 8);
        } else if (touchesPlayer(a.x8, a.yw)) {
          p.pendingHit = true;
          a.state = 3;
          a.v0 = 0;
        }
        break;
      }
      case 0x3023: {
        if (a.state == 1) {
          if (!floorAt(a.x8, a.yw + 0x80)) a.yw += 0x40;
          if (!offscreen(a.yw, a.x8) && bulletHit(a.yw, a.x8)) {
            spawnSparks(a.yw, a.x8);
            if (!floorAt(a.x8, a.yw + 0x80)) addScore(500, (a.x8 - 2) * 8, a.yw / 8);
            a.state = 2;
          }
        } else {
          if ((phase_ % 4) == 0) a.v0 = (a.v0 + 1) % 3;
          if (!floorAt(a.x8, a.yw - 0x80)) a.yw -= 8;
          const bool shot = bulletHit(a.yw - 0x80, a.x8) != 0;
          if (!shot && !floorAt(a.x8, a.yw - 0x80)) {
            if (touchesPlayer(a.x8, a.yw)) {
              addScore(10000, (a.x8 - 2) * 8, a.yw / 8);
              sound(3);
              a.state = 0;
              a.alive = false;
              sandbox_.world.cells[a.cellIndex] = a.under;
            }
          } else {
            spawnExplosion(a.yw - 0x80, a.x8, 4);  // the balloon pops
            spawnDebris(a.yw - 0x80, a.x8);
            a.state = 0;
            a.alive = false;
            sandbox_.world.cells[a.cellIndex] = a.under;
          }
        }
        break;
      }
      case 0x3024: {
        if (offscreen(a.yw, a.x8)) break;
        if (bulletHit(a.yw, a.x8)) {
          addScore(100, (a.x8 - 2) * 8, a.yw / 8);
          spawnExplosion(a.yw, a.x8, 1);
          a.state = 0;
          a.alive = false;
          sandbox_.world.cells[a.cellIndex] = a.under;
        }
        break;
      }
      case 0x3058: case 0x3059:
        if (!offscreen(a.yw, a.x8) && touchesPlayer(a.x8, a.yw)) p.pendingHit = true;
        break;
      case 0x3029: case 0x3037: case 0x3038: case 0x3039: case 0x303A: {
        if (offscreen(a.yw, a.x8)) break;
        if (!floorAt(a.x8, a.yw + 0x80)) a.yw += 0x80;
        if (a.v0 == 0 && bulletHit(a.yw, a.x8)) {
          spawnSparks(a.yw, a.x8);
          if (!floorAt(a.x8, a.yw + 0x80)) addScore(500, (a.x8 - 2) * 8, a.yw / 8);
          a.v0 = 1;
        }
        if (a.v0 && touchesPlayer(a.x8, a.yw)) {
          if (a.id == 0x3029) {
            addScore(1000, (a.x8 - 2) * 8, a.yw / 8);
            p.health = 9;  // the sphere restores full health
            sound(4);
          } else {
            static const int kLetter[4] = {'D', 'U', 'K', 'E'};
            const int letter = kLetter[a.id - 0x3037];
            if (letter == 'E' && letters_.size() == 3 && letters_[0] == 'D' && letters_[1] == 'U' && letters_[2] == 'K') {
              dukeBonus_ = true;  // paid when the level is left
              letters_.push_back(letter);
              sound(37);
            } else {
              letters_.push_back(letter);
              addScore(500, (a.x8 - 2) * 8, a.yw / 8);
              sound(16);
            }
          }
          a.state = 0;
          a.alive = false;
          sandbox_.world.cells[a.cellIndex] = a.under;
        }
        break;
      }
      case 0x3031: {
        if (offscreen(a.yw, a.x8)) break;
        static const int kBob[14] = {128, 96, 64, 32, 8, 4, 1, -1, -4, -8, -32, -64, -96, -128};
        a.yw -= kBob[a.v0 + 6];
        if (++a.v0 == 8) a.v0 = -6;
        if (touchesPlayer(a.x8, a.yw)) p.pendingHit = true;
        bulletHit(a.yw, a.x8);
        break;
      }
      case 0x3057: {
        if (offscreen(a.yw, a.x8)) break;
        if (!floorAt(a.x8, a.yw + 0x80)) a.yw += 0x40;
        if (touchesPlayer(a.x8, a.yw)) {
          p.pendingHit = true;
          spawnExplosion(a.yw, a.x8, 2);
          a.state = 0;
          a.alive = false;
          sandbox_.world.cells[a.cellIndex] = a.under;
        }
        break;
      }
      case 0x3002: case 0x3004: {
        // Rewrites its tiles every update so they animate; carries Duke along while he stands on it
        const int anim = phase_ % 4;
        const int base = a.row * 128 + a.col;
        for (int i = 0; i < a.v1; ++i) {
          const auto idx = static_cast<std::size_t>(base + i);
          if (idx >= sandbox_.world.cells.size()) break;
          std::uint16_t v = static_cast<std::uint16_t>(0x1c00 + (anim & 1) * 0x20);
          if (i == 0) v = static_cast<std::uint16_t>(a.v0 ? 0x1b80 + anim * 0x20 : 0x1be0 - anim * 0x20);
          if (i == a.v1 - 1) v = static_cast<std::uint16_t>(a.v0 ? 0x1c40 + anim * 0x20 : 0x1ca0 - anim * 0x20);
          sandbox_.world.cells[idx] = v;
        }
        const int playerX = p.x8 + p.cameraX8;
        const int dy = ((p.y >> 4) * 0x80 + p.cameraYWords) - (a.yw - 0x20) + 2;
        const bool standing = playerX > a.x8 - 2 && playerX < a.x8 + 2 * a.v1 + 2 && dy > 0 && dy < 0x180;
        if (standing) {
          std::vector<Event> scratch;
          const bool canMove = sandbox_.world.clear(a.v0 ? Sensor::Right : Sensor::Left, p, scratch);
          if (canMove) p.x8 += a.v0 ? 1 : -1;
        }
        // Duke hanging from the ceiling below the belt is carried the opposite way
        const int dyUnder = ((p.y >> 4) * 0x80 + p.cameraYWords) - (a.yw + 0x60) + 2;
        if (p.cling && playerX > a.x8 - 2 && playerX < a.x8 + 2 * a.v1 + 2 && dyUnder > 0 && dyUnder < 0x180) {
          std::vector<Event> scratch;
          const bool canMove = sandbox_.world.clear(a.v0 ? Sensor::Left : Sensor::Right, p, scratch);
          if (canMove) p.x8 += a.v0 ? -1 : 1;
        }
        break;
      }
      case 0x3001: {
        // Elevator: a solid carrier tile that rises when Up is pressed while standing exactly on it
        if (offscreen(a.yw, a.x8)) {
          if (a.v0 > 0) {  // out of view it simply settles back down
            sandbox_.world.cells[a.cellIndex - static_cast<std::size_t>(a.v0) * 128] = static_cast<std::uint16_t>(a.v1);
            --a.v0;
            a.yw += 0x80;
          }
          break;
        }
        const int playerX = p.x8 + p.cameraX8;
        const int dy = ((p.y >> 4) * 0x80 + p.cameraYWords) - (a.yw - 0x20) + 2;
        const bool standing = playerX == a.x8 && dy > 0 && dy < 0x180;
        if (!standing) {
          if (a.v0 > 0) {  // lowers back down
            sandbox_.world.cells[a.cellIndex - static_cast<std::size_t>(a.v0) * 128] = static_cast<std::uint16_t>(a.v1);
            --a.v0;
            a.yw += 0x80;
          }
        } else if (sandbox_.input.up && !p.cling && sandbox_.world.clear(Sensor::Head, p, sandbox_.events)) {
          sound(20);
          ++a.v0;
          a.yw -= 0x80;
          const auto idx = a.cellIndex - static_cast<std::size_t>(a.v0) * 128;
          sandbox_.world.cells[idx] = 0x1ae0;
          p.y -= 0x10;
          if (p.y < 0x41) p.verticalScroll = true;
        }
        break;
      }
      case 0x3011: {
        const auto& in = sandbox_.input;
        if (a.v0 == 0) {
          if (in.up && !in.left && !in.right && !in.jump && (touchesPlayer(a.x8 + 1, a.yw) || touchesPlayer(a.x8 + 1, a.yw - 0x80))) a.v0 = 1;
        } else {
          if (phase_ % 2 == 0) ++a.v0;
          exitState_ = a.v0;
          if (a.v0 >= 8) { levelComplete_ = true; sound(19); }
        }
        break;
      }
      case 0x3044: case 0x3045: case 0x3046: case 0x3047: {
        if (offscreen(a.yw, a.x8)) break;
        if (touchesPlayer(a.x8, a.yw)) {
          keys_[a.id - 0x3044] = true;
          sound(9);
          addScore(1000, (a.x8 - 2) * 8, a.yw / 8);
          a.state = 0;
          a.alive = false;
          sandbox_.world.cells[a.cellIndex] = a.under;
        }
        break;
      }
      case 0x3048: case 0x3049: case 0x304A: case 0x304B: {
        if (offscreen(a.yw, a.x8) || a.v0) break;
        const int color = a.id - 0x3048;
        if (touchesPlayer(a.x8, a.yw) && sandbox_.input.up && keys_[color]) {
          for (auto& d : actors_) {
            if (d.alive && d.id == 0x304C + color && d.v0 == 0) {
              d.v0 = 1;  // start opening
              const auto idx = d.cellIndex;
              sandbox_.world.cells[idx] = idx > 0 ? sandbox_.world.cells[idx - 1] : 0;
            }
          }
          keys_[color] = false;
          sound(31);
          a.v0 = 1;
          addScore(1000, (a.x8 - 2) * 8, a.yw / 8);
        }
        break;
      }
      case 0x304C: case 0x304D: case 0x304E: case 0x304F: {
        if (a.v0 == 0 || offscreen(a.yw, a.x8)) break;
        const int step = phase_ % 4;
        if (step == 0 || step == 2) ++a.v0;
        if (a.v0 >= 9) {  // fully open: gone
          a.state = 0;
          a.alive = false;
        }
        break;
      }
      case 0x300C: {
        // Jump height table from the original data: (dx, dy) pairs in 8 px units / map words
        static const int kJump[24] = {1, 256, 2, 128, 0, 64, 1, 32, 0, 16, 1, 8, 0, 4, 1, 2, 0, 1, 1, -1, 0, -2, 1, -4};
        a.v1 = -1;
        if (!offscreen(a.yw, a.x8) && a.v0 == 0) a.v0 = 1;
        if (a.v0 == 0) break;
        const int playerX = p.x8 + p.cameraX8;
        const int stepPhase = phase_ % 4;
        auto hurtCheck = [&](int frame) {
          a.v1 = frame;
          if (touchesPlayer(a.x8 + 1, a.yw) || touchesPlayer(a.x8 + 1, a.yw - 0x80)) p.pendingHit = true;
        };
        int hitsTaken = 0;
        if (bulletHit(a.yw - 0x80, a.x8 + 1) || bulletHit(a.yw, a.x8 + 1)) hitsTaken = 1;
        a.hits -= hitsTaken;  // hits counts down from 3
        if (hitsTaken && a.hits <= 0) {
          spawnSparks(a.yw - 0x100, a.x8 + 1);
          spawnSparks(a.yw - 0x80, a.x8);
          spawnSparks(a.yw, a.x8 + 2);
          spawnExplosion(a.yw - 0x80, a.x8 + 1, 1);
          spawnExplosion(a.yw - 0x40, a.x8 + 2, 1);
          spawnExplosion(a.yw - 0x80, a.x8 + 2, 1);
          spawnExplosion(a.yw, a.x8 + 1, 1);
          addScore(2000, (a.x8 - 2) * 8, a.yw / 8);
          a.state = 0;
          a.alive = false;
          sandbox_.world.cells[a.cellIndex] = a.under;
          break;
        }
        if (hitsTaken && a.hits == 2 && stepPhase == 0) spawnDebris(a.yw - 0xc0, a.x8);
        if (hitsTaken && a.hits == 1 && stepPhase == 0) { spawnDebris(a.yw - 0xc0, a.x8 + 2); spawnDebris(a.yw - 0xc0, a.x8); }
        for (int guard = 0; guard < 2; ++guard) {
          if (a.state < 0x19) {
            if (!floorAt(a.x8 + 1, a.yw + 0x80)) {  // in the air: fall a row
              hurtCheck(playerX - 3 < a.x8 ? 2 : 5);
              a.yw += 0x80;
            } else {
              if (playerX - 3 < a.x8) {
                a.v0 = 1;
                if (a.state != 0xc) hurtCheck(0);
              } else {
                if (a.state != 0xc) hurtCheck(3);
                a.v0 = 2;
              }
              ++a.state;
              a.yw = (a.yw >> 7) << 7;
              if (a.state == 0xd) {
                if (a.v0 == 1) { spawnEnemyShot(a.yw - 0x50, a.x8 - 1, 1); hurtCheck(1); }
                if (a.v0 == 2) { spawnEnemyShot(a.yw - 0x50, a.x8 + 3, 2); hurtCheck(4); }
                ++a.state;
              }
            }
            break;
          }
          if (a.state < 0x32) {
            const int i0 = a.state - 0x19;
            const int dx = (i0 >= 0 && i0 < 24) ? kJump[i0] : 0;
            const int dy = (i0 + 1 >= 0 && i0 + 1 < 24) ? kJump[i0 + 1] : 0;
            if (a.v0 == 1) {
              if (floorAt(a.x8 + 1, a.yw - 0x100)) {
                a.yw = (a.yw >> 7) << 7;
                hurtCheck(2);
                a.state = 1;
                break;
              }
              if (!wallAt(a.x8 - 1, a.yw)) a.x8 -= dx;
              a.yw -= dy;
              hurtCheck(2);
              a.state += 2;
              break;
            }
            if (a.v0 == 2) {
              if (!floorAt(a.x8 + 1, a.yw - 0x100)) {
                if (!wallAt(a.x8 + 4, a.yw)) a.x8 += dx;
                a.yw -= dy;
                hurtCheck(5);
                a.state += 2;
              } else {
                a.yw = (a.yw >> 7) << 7;
                hurtCheck(5);
                a.state = 1;
              }
              break;
            }
          }
          a.state = 1;
        }
        break;
      }
      case 0x300E: {
        a.v1 = -1;
        const bool vulnerable = walkerPhase_ < 0x19;
        const bool shotNow = bulletHit(a.yw, a.x8) || bulletHit(a.yw, a.x8 + 2);
        if (shotNow && vulnerable) --a.hits;
        if (a.hits <= 0) {
          spawnSparks(a.yw, a.x8 + 2);
          spawnSparks(a.yw, a.x8);
          spawnDebris(a.yw - 0x80, a.x8 + 2);
          spawnExplosion(a.yw, a.x8 + 1, 1);
          spawnExplosion(a.yw - 0x80, a.x8 + 1, 1);
          spawnExplosion(a.yw - 0x80, a.x8, 1);
          addScore(5000, (a.x8 - 2) * 8, a.yw / 8);
          a.state = 0;
          a.alive = false;
          sandbox_.world.cells[a.cellIndex] = a.under;
          break;
        }
        if (a.hits == 1 && (phase_ % 4) == 0) spawnDebris(a.yw - 0x40, a.x8 + 2);
        const int step = phase_ % 4;
        const bool moveNow = step == 0 || step == 2;
        bool draw = true;
        if (!floorAt(a.x8 + 1, a.yw + 0x80)) {
          a.yw += 0x80;
        } else {
          if (a.state == 1) {
            if (wallAt(a.x8, a.yw + 0x80) && !wallAt(a.x8, a.yw)) {
              if (moveNow) --a.x8;
            } else {
              a.state = 2;
            }
          }
          if (a.state == 2) {
            if (!wallAt(a.x8 + 3, a.yw + 0x80) || wallAt(a.x8 + 3, a.yw)) a.state = 1;
            else if (moveNow) ++a.x8;
          } else if (a.state != 1) {
            draw = false;
          }
        }
        if (draw) {
          a.v1 = walkerAnim_;
          if (touchesPlayer(a.x8 + 1, a.yw) || touchesPlayer(a.x8 + 1, a.yw - 0x40)) p.pendingHit = true;
        }
        break;
      }
      case 0x300D: {
        if (!a.alive) break;
        a.v1 = -1;
        const int anim = (phase_ / 2) % 2, slow = phase_ % 2;
        int hitsTaken = (bulletHit(a.yw, a.x8) || bulletHit(a.yw, a.x8 + 2)) ? 1 : 0;
        a.hits -= hitsTaken;
        if (a.hits <= 0) {
          spawnSparks(a.yw, a.x8 + 2);
          spawnSparks(a.yw, a.x8);
          spawnExplosion(a.yw, a.x8 + 1, 1);
          addScore(2500, (a.x8 - 2) * 8, a.yw / 8);
          a.state = 0;
          a.alive = false;
          sandbox_.world.cells[a.cellIndex] = a.under;
          break;
        }
        if (a.hits == 1 && (phase_ % 4) == 0) spawnDebris(a.yw - 0x40, a.x8 + 2);
        auto draw = [&](int frame) {
          a.v1 = frame;
          if (touchesPlayer(a.x8, a.yw) || touchesPlayer(a.x8 + 2, a.yw)) p.pendingHit = true;
        };
        if (!floorAt(a.x8, a.yw + 0x80)) {
          a.yw += 0x20;
          draw(0);
        } else {
          if (a.state == 1) {
            if (!wallAt(a.x8, a.yw) && wallAt(a.x8, a.yw + 0x80)) {
              if (slow == 0) --a.x8;
              draw(anim);
            } else {
              a.state = 2;
              spawnEnemyShot(a.yw - 0x40, a.x8 + 4, 2);
            }
          }
          if (a.state == 2) {
            if (!wallAt(a.x8 + 4, a.yw) && wallAt(a.x8 + 4, a.yw + 0x80)) {
              if (slow == 0) ++a.x8;
              draw(anim + 2);
            } else {
              a.state = 1;
              spawnEnemyShot(a.yw - 0x40, a.x8 - 2, 1);
              draw(anim + 2);
            }
          }
        }
        break;
      }
      case 0x301A:
        a.v0 = touchesPlayer(a.x8, a.yw) ? 1 : 0;
        if (a.v0 && !offscreen(a.yw, a.x8)) p.pendingHit = true;
        break;
      case 0x302C:
        a.v0 = touchesPlayer(a.x8, a.yw) ? 1 : 0;
        if (a.v0 && !offscreen(a.yw, a.x8)) p.pendingHit = true;
        break;
      case 0x3021:
        // Pushes Duke away from the field instead of hurting him
        if (!offscreen(a.yw, a.x8)) {
          if (phase_ % 4 == 0) sound(21);
          if (touchesPlayer(a.x8 + 1, a.yw) && p.x8 < 28) ++p.x8;
          if (touchesPlayer(a.x8 - 1, a.yw) && p.x8 > 0) --p.x8;
        }
        break;
      case 0x3009:
      case 0x300A: {
        // Counter runs 1..0x41; from 0xf a puff, then (0x19-0x2c) a three piece flame that burns
        ++a.state;
        if (a.state > 0x41) a.state = 1;
        if (a.state >= 0x19 && a.state < 0x2d && !offscreen(a.yw, a.x8)) {
          const int dir = a.id == 0x3009 ? 1 : -1;
          if (touchesPlayer(a.x8, a.yw) || touchesPlayer(a.x8 + 2 * dir, a.yw - 0x20) || touchesPlayer(a.x8 + 4 * dir, a.yw))
            p.pendingHit = true;
        }
        break;
      }
      case 0x303C: {
        if (!floorAt(a.x8, a.yw + 0x80)) a.yw += 0x40;
        if (wallAt(a.x8 - 1, a.yw) || !floorAt(a.x8 - 2, a.yw + 0x80)) a.state = 2;
        if (wallAt(a.x8 + 2, a.yw) || !floorAt(a.x8 + 2, a.yw + 0x80)) a.state = 1;
        const int step = phase_ % 4;
        if (step == 0 || step == 2) a.x8 += a.state == 1 ? -1 : 1;
        bulletHit(a.yw, a.x8);  // fire eats shots
        if (touchesPlayer(a.x8, a.yw)) p.pendingHit = true;
        break;
      }
      case 0x3012: {
        if (!floorAt(a.x8, a.yw + 0x80)) a.yw += 0x80;
        if (a.v0 == 0 && bulletHit(a.yw, a.x8)) {
          spawnSparks(a.yw, a.x8);
          a.v0 = 1;
          if (!floorAt(a.x8, a.yw + 0x80)) addScore(500, (a.x8 - 2) * 8, a.yw / 8);
        }
        if (a.v0 == 0) break;
        const bool visible = !offscreen(a.yw, a.x8);
        ++a.state;
        if (a.state > 0xc) {
          auto blast = [&](int dx) {
            const int x = a.x8 + dx;
            return !wallAt(x, a.yw) && wallAt(x, a.yw + 0x80);
          };
          if (a.state == 0xe) {
            spawnExplosion(a.yw, a.x8, 2);
          } else if (a.state == 0x11) {
            if (blast(-2)) spawnExplosion(a.yw, a.x8 - 2, 2); else a.v1 &= ~1;
            if (blast(2)) spawnExplosion(a.yw, a.x8 + 2, 2); else a.v1 &= ~2;
          } else if (a.state == 0x13 || a.state == 0x15) {
            const int d = a.state == 0x13 ? 4 : 6;
            if ((a.v1 & 1) && (blast(-d))) spawnExplosion(a.yw, a.x8 - d, 2); else if (a.v1 & 1) a.v1 &= ~1;
            if ((a.v1 & 2) && (blast(d))) spawnExplosion(a.yw, a.x8 + d, 2); else if (a.v1 & 2) a.v1 &= ~2;
          } else if (a.state == 0x17) {
            if ((a.v1 & 1) && blast(-8)) spawnExplosion(a.yw, a.x8 - 8, 2);
            if ((a.v1 & 2) && blast(8)) spawnExplosion(a.yw, a.x8 + 8, 2);
          }
          if (a.state == 0x18) {
            a.state = 0;
            a.alive = false;
            sandbox_.world.cells[a.cellIndex] = a.under;
          }
        }
        (void)visible;
        break;
      }
      case 0x3000: {
        if (offscreen(a.yw, a.x8)) break;
        if (!floorAt(a.x8, a.yw + 0x80)) a.yw += 0x80;  // crates fall a row per update
        if (bulletHit(a.yw, a.x8)) {
          a.state = 0;
          a.alive = false;
          sandbox_.world.cells[a.cellIndex] = a.under;
          spawnSparks(a.yw, a.x8);
          if (!floorAt(a.x8, a.yw + 0x80)) addScore(500, (a.x8 - 2) * 8, a.yw / 8);
        }
        break;
      }
      case 0x300B: {
        static const int kDyingLeft[24] = {0, -64, 1, -32, 1, -16, 0, -1, 0, 0, 1, 12, 1, 24, 0, 46, 1, 128, 0, 128, 1, 256, 0, 0};
        static const int kDyingRight[24] = {0, -64, -1, -32, -1, -16, 0, -1, 0, 0, -1, 12, -1, 24, 0, 46, -1, 128, -1, 128, -1, 256, 0, 0};
        if (a.state == 0xfe || a.state == 0xff) {  // knocked out: tumbles along a fixed path, then falls
          const bool facingRight = a.state == 0xfe;
          if (floorAt(a.x8, a.yw + 0x80)) {
            spawnSparks(a.yw, a.x8);
            spawnExplosion(a.yw, a.x8, 1);
            a.state = 0;
            a.alive = false;
            sandbox_.world.cells[a.cellIndex] = a.under;
            break;
          }
          const int limit = facingRight ? 0x15 : 0x14;
          if (a.v0 < limit) {
            const int* t = facingRight ? kDyingLeft : kDyingRight;
            a.x8 += t[a.v0];
            a.yw += t[a.v0 + 1];
            a.v0 += 2;
          } else {
            a.yw += 0x80;
          }
          if (offscreen(a.yw, a.x8)) { a.state = 0; a.alive = false; sandbox_.world.cells[a.cellIndex] = a.under; }
          else spawnDebris(a.yw - 0x80, a.x8);  // smoke trail while it tumbles down
          break;
        }
        if (offscreen(a.yw, a.x8) || offscreen(a.yw - 0x80, a.x8)) break;
        const int shot = bulletHit(a.yw - 0x80, a.x8);
        if (shot == 0 && !touchesPlayer(a.x8, a.yw - 0x80)) {
          if (!floorAt(a.x8, a.yw + 0x80)) a.yw += 8;
          if (floorAt(a.x8, a.yw + 0x80)) {
            a.yw -= 8;
            if (!floorAt(a.x8, a.yw + 0x80) && hoverAnim_ == 0) spawnDebris(a.yw, a.x8);
          }
          if (hoverAnim_ == 0) {
            const int playerX = p.x8 + p.cameraX8;
            if (playerX < a.x8) {
              if (a.state < 2) {
                if (!wallAt(a.x8 - 1, a.yw - 5)) {
                  --a.x8;
                  if (hoverTick_ == 0) spawnEnemyShot(a.yw - 0xa0, a.x8 - 1, 1);
                }
              } else {
                --a.state;
              }
            }
            if (a.x8 < playerX) {
              if (a.state < 6) {
                ++a.state;
              } else if (!wallAt(a.x8 + 2, a.yw - 5)) {
                ++a.x8;
                if (hoverTick_ == 0) spawnEnemyShot(a.yw - 0xa0, a.x8 + 1, 2);
              }
            }
          }
        } else {
          if (touchesPlayer(a.x8, a.yw)) p.pendingHit = true;
          a.state = (phase_ % 4) < 2 ? 0xfe : 0xff;  // original tests the shared tile animation step (0 or 32)
          spawnExplosion(a.yw, a.x8, 1);
          a.v0 = 0;
          addScore(200, (a.x8 - 2) * 8, a.yw / 8);
        }
        break;
      }
      case 0x3016:
      case 0x3017: {
        if (offscreen(a.yw, a.x8)) break;
        if (a.state == 1) {
          a.yw -= 8;
          if (!floorAt(a.x8 - 2, a.yw) || floorAt(a.x8, a.yw)) a.state = 2;
        } else if (a.state == 2) {
          a.yw += 8;
          if (!floorAt(a.x8 - 2, a.yw + 0x80) || floorAt(a.x8, a.yw + 0x80)) a.state = 1;
        } else if (a.state == 3) {
          a.yw -= 8;
          if (!floorAt(a.x8 + 2, a.yw) || floorAt(a.x8, a.yw)) a.state = 4;
        } else if (a.state == 4) {
          a.yw += 8;
          if (!floorAt(a.x8 + 2, a.yw + 0x80) || floorAt(a.x8, a.yw + 0x80)) a.state = 3;
        }
        if (bulletHit(a.yw, a.x8)) {
          spawnExplosion(a.yw, a.x8, 1);
          spawnDebris(a.yw - 0x40, a.x8);
          addScore(100, (a.x8 - 2) * 8, a.yw / 8);
          a.state = 0;
          a.alive = false;
          sandbox_.world.cells[a.cellIndex] = a.under;
          break;
        }
        if (touchesPlayer(a.x8, a.yw)) p.pendingHit = true;
        break;
      }
      case 0x3010: {
        if (offscreen(a.yw, a.x8)) break;
        if (a.state < 3) {
          if (const int hit = bulletHit(a.yw, a.x8)) {
            if (hit == 1) --a.x8;
            if (hit == 2) ++a.x8;
            a.state = 3;
          }
        }
        if (!floorAt(a.x8, a.yw + 0x80)) a.yw += 0x80;  // falls a row at a time
        if (a.state == 1) {
          if (!wallAt(a.x8 - 1, a.yw) && wallAt(a.x8 - 1, a.yw + 0x80)) {
            if (famAnim3010_ == 0) --a.x8;
          } else {
            a.state = 2;
          }
        } else if (a.state == 2) {
          if (!wallAt(a.x8 + 2, a.yw) && wallAt(a.x8 + 2, a.yw + 0x80)) {
            if (famAnim3010_ == 0) ++a.x8;
          } else {
            a.state = 1;
          }
        } else if (a.state > 2) {
          ++a.state;
          if (a.state == 10) {
            a.state = 0;
            a.alive = false;
            sandbox_.world.cells[a.cellIndex] = a.under;
            spawnExplosion(a.yw, a.x8, 1);
            addScore(100, (a.x8 - 2) * 8, a.yw / 8);
            break;
          }
        }
        if (a.state > 0 && a.state < 3 && touchesPlayer(a.x8, a.yw)) p.pendingHit = true;
        break;
      }
      default:
        break;
    }
  }
  bool any = false;
  for (const auto& t : actors_) any = any || (t.alive && t.id == 0x3024);
  targetsCleared_ = !any;
}

void PlayWorld::drawPorted(std::vector<std::uint8_t>& out, int w, int h, int left, int top) const {
  const auto& p0 = sandbox_.player;
  for (const auto& actorRef : actors_) {
    if (!actorRef.alive || !actorRef.ported) continue;
    auto a = actorRef;
    if (a.state > 2 && (a.id == 0x3015 || a.id == 0x3052 || a.id == 0x3018 || a.id == 0x3051)) {
      // A shot-launched can or turkey climbs 8 px on three updates out of four and
      // pauses on the fourth; spread that evenly so the rise looks steady.
      a.yw += 0x10 * (phase_ % 4) - 0x30;
      a.prevYw += 0x10 * ((phase_ + 3) % 4) - 0x30;
    }
    shiftX_ = static_cast<int>(std::lround((a.x8 - a.prevX8) * 8 * (blend_ - 1.0)));
    shiftY_ = static_cast<int>(std::lround((a.yw - a.prevYw) / 8.0 * (blend_ - 1.0)));
    if (std::abs(a.x8 - a.prevX8) > 8 || std::abs(a.yw - a.prevYw) > 0x200) shiftX_ = shiftY_ = 0;
    switch (a.id) {
      case 0x3010:
        if (a.state == 0) break;
        if (a.state < 3)
          drawPiece(out, w, h, left, top, 80 + famAnim3010_ / 0xa0, a.x8, a.yw);
        else
          drawPiece(out, w, h, left, top, 83 + (a.state - 3), a.x8, a.yw);
        break;
      case 0x3000:
        drawObjectPiece(out, w, h, left, top, 0, a.x8, a.yw);
        break;
      case 0x302A: {
        // Idle or fully fallen: plain sign; shaking in between
        const int wiggle = a.state >= 2 && a.state < 0xf ? a.state % 2 : 0;
        shiftY_ += wiggle;
        drawObjectPiece(out, w, h, left, top, 83, a.x8, a.yw);
        drawObjectPiece(out, w, h, left, top, 84, a.x8 + 2, a.yw);
        break;
      }
      case 0x3007: {
        drawObjectPiece(out, w, h, left, top, 11, a.x8, a.yw - 0x180);
        drawObjectPiece(out, w, h, left, top, 12, a.x8, a.yw - 0x100);
        drawObjectPiece(out, w, h, left, top, 12, a.x8, a.yw - 0x80);
        drawObjectPiece(out, w, h, left, top, 13, a.x8, a.yw);
        drawObjectPiece(out, w, h, left, top, 14, a.x8 - 2, a.yw);
        drawObjectPiece(out, w, h, left, top, 15, a.x8 + 2, a.yw);
        if (a.state > 1 && a.state < 10 && (a.state & 1)) drawObjectPiece(out, w, h, left, top, 17, a.x8, a.yw + 0x80);
        if (a.state == 10 && a.yw % 0x100 != 0) {
          drawObjectPiece(out, w, h, left, top, 16, a.x8, a.yw + 0x80);
          drawObjectPiece(out, w, h, left, top, 17, a.x8, a.yw + 0x100);
        }
        break;
      }
      case 0x3042: case 0x3043: {
        const bool faceLeft = p0.x8 + p0.cameraX8 - 2 < a.x8;
        const int frame = a.state == 2 ? (faceLeft ? 0 : 2) : boss_.anim + (faceLeft ? 0 : 2);
        static const int kBottom[4][2] = {{272, 273}, {274, 275}, {278, 279}, {280, 281}};
        drawPiece(out, w, h, left, top, frame < 2 ? 270 : 276, a.x8, a.yw - 0x80);
        drawPiece(out, w, h, left, top, frame < 2 ? 271 : 277, a.x8 + 2, a.yw - 0x80);
        drawPiece(out, w, h, left, top, kBottom[frame][0], a.x8, a.yw);
        drawPiece(out, w, h, left, top, kBottom[frame][1], a.x8 + 2, a.yw);
        break;
      }
      case 0x3034:
        drawObjectPiece(out, w, h, left, top, a.state == 1 ? 105 + phase_ % 8 : 113, a.x8, a.yw);
        break;
      case 0x3035:
        drawObjectPiece(out, w, h, left, top, 116, a.x8 - 2, a.yw);
        drawObjectPiece(out, w, h, left, top, 117, a.x8 + 2, a.yw);
        drawObjectPiece(out, w, h, left, top, (a.v0 == 0 && phase_ % 4 == 0) ? 114 : 115, a.x8, a.yw);
        break;
      case 0x302B: {
        for (int i = 0; i < 3; ++i) {
          const int y = a.yw - 0x80 * i;
          if (a.v0 == 0 && a.v1) {
            drawPiece(out, w, h, left, top, 255, a.x8, y);  // flash when hit
          } else if (a.v0 == 0) {
            drawObjectPiece(out, w, h, left, top, 90 + phase_ % 5, a.x8, y);
          } else {
            drawObjectPiece(out, w, h, left, top, 90, a.x8, y);
          }
        }
        break;
      }
      case 0x302F: case 0x3030: {
        // Pad: three rows of pieces; while teleporting the middle row shows the beam
        const bool beam = a.v0 > 1;
        drawPiece(out, w, h, left, top, 212, a.x8, a.yw - 0x100);
        drawPiece(out, w, h, left, top, 214, a.x8 + 4, a.yw - 0x100);
        drawPiece(out, w, h, left, top, 215, a.x8, a.yw - 0x80);
        drawPiece(out, w, h, left, top, beam ? 224 + phase_ % 2 : 216, a.x8 + 2, a.yw - 0x80);
        drawPiece(out, w, h, left, top, beam ? 226 + phase_ % 2 : 217, a.x8 + 4, a.yw - 0x80);
        drawPiece(out, w, h, left, top, 218, a.x8, a.yw);
        drawPiece(out, w, h, left, top, 219, a.x8 + 2, a.yw);
        drawPiece(out, w, h, left, top, 220, a.x8 + 4, a.yw);
        break;
      }
      case 0x3015: case 0x3052: {
        const int f = (phase_ / 4) % 4;
        if (a.state == 1) drawObjectPiece(out, w, h, left, top, 101, a.x8, a.yw);
        else if (a.state == 2) drawPiece(out, w, h, left, top, 128 + f, a.x8, a.yw);
        else if (a.state > 2) drawPiece(out, w, h, left, top, 132 + f, a.x8, a.yw);
        break;
      }
      case 0x3018: case 0x3051:
        drawObjectPiece(out, w, h, left, top, a.state == 1 ? 101 : (a.state == 2 ? 44 : 45), a.x8, a.yw);
        break;
      case 0x3022: {
        if (a.v0 == 0) break;  // asleep until first on screen: not drawn by the original either
        const int hover = phase_ % 2;
        const int rotor = a.state == 1 ? hover + 2 : (a.state == 2 ? hover : hover + 4);
        const int body = a.state == 1 ? 1 : (a.state == 2 ? 0 : 2);
        for (int i = 0; i < 4; ++i) {
          drawPiece(out, w, h, left, top, 176 + 4 * rotor + i, a.x8 + 2 * i, a.yw - 0x80);
          drawPiece(out, w, h, left, top, 164 + 4 * body + i, a.x8 + 2 * i, a.yw);
        }
        break;
      }
      case 0x303B: {
        if (a.v1 < 0) break;
        const int base = a.v1 < 3 ? 114 + 2 * a.v1 : 228 + 2 * (a.v1 - 3);
        drawPiece(out, w, h, left, top, base, a.x8, a.yw - 0x80);
        drawPiece(out, w, h, left, top, base + 1, a.x8, a.yw);
        break;
      }
      case 0x3023:
        if (a.state == 1) {
          drawObjectPiece(out, w, h, left, top, 100, a.x8, a.yw);
        } else if (a.state == 2) {
          drawObjectPiece(out, w, h, left, top, 69, a.x8, a.yw - 0x80);  // the balloon
          drawObjectPiece(out, w, h, left, top, 70 + a.v0, a.x8, a.yw);  // its wiggling string
        }
        break;
      case 0x3024: {
        const int playerX = p0.x8 + p0.cameraX8;
        const int piece = playerX < a.x8 - 2 ? 200 : (a.x8 + 2 < playerX ? 202 : 201);
        drawPiece(out, w, h, left, top, piece, a.x8, a.yw);
        break;
      }
      case 0x3058: case 0x3059:
        drawObjectPiece(out, w, h, left, top, a.id == 0x3058 ? 148 : 149, a.x8, a.yw);
        break;
      case 0x3029:
        if (a.v0 == 0) drawObjectPiece(out, w, h, left, top, 0, a.x8, a.yw);
        else drawObjectPiece(out, w, h, left, top, 73 + phase_ % 9, a.x8, a.yw);
        break;
      case 0x3037: case 0x3038: case 0x3039: case 0x303A:
        drawObjectPiece(out, w, h, left, top, a.v0 == 0 ? 0 : 118 + (a.id - 0x3037), a.x8, a.yw);
        break;
      case 0x3031: case 0x3057:
        drawPiece(out, w, h, left, top, 223, a.x8, a.yw);
        break;
      case 0x3001:
        drawObjectPiece(out, w, h, left, top, 5, a.x8, a.yw);
        break;
      case 0x3011: {
        const int frame = a.v0 == 0 ? 0 : std::min(a.v0 - 1, 3);
        drawPiece(out, w, h, left, top, 96 + 4 * frame, a.x8, a.yw - 0x80);
        drawPiece(out, w, h, left, top, 97 + 4 * frame, a.x8 + 2, a.yw - 0x80);
        drawPiece(out, w, h, left, top, 98 + 4 * frame, a.x8, a.yw);
        drawPiece(out, w, h, left, top, 99 + 4 * frame, a.x8 + 2, a.yw);
        break;
      }
      case 0x3044: case 0x3045: case 0x3046: case 0x3047:
        drawObjectPiece(out, w, h, left, top, 124 + (a.id - 0x3044), a.x8, a.yw);
        break;
      case 0x3048: case 0x3049: case 0x304A: case 0x304B: {
        const int piece = 137 + (a.id - 0x3048);  // control, one picture per key colour
        drawObjectPiece(out, w, h, left, top, (a.v0 || (phase_ / 4) % 2 == 0) ? piece : 136, a.x8, a.yw);
        break;
      }
      case 0x304C: case 0x304D: case 0x304E: case 0x304F:
        drawObjectPiece(out, w, h, left, top, a.v0 == 0 ? 128 : 127 + a.v0, a.x8, a.yw);
        break;
      case 0x300C: {
        // 2x3 piece layout per frame from the original table (-1 = no piece)
        static const int kPieces[6][6] = {{-1, -1, 10, 11, 12, 13}, {-1, -1, 10, 11, 14, 15}, {16, 17, 18, 19, 20, 21},
                                          {-1, -1, 22, 23, 24, 25}, {-1, -1, 22, 23, 26, 27}, {28, 29, 30, 31, 32, 33}};
        if (a.v1 < 0) break;
        const auto& t = kPieces[a.v1];
        const int rowYw[3] = {a.yw - 0x100, a.yw - 0x80, a.yw};
        for (int r = 0; r < 3; ++r)
          for (int c = 0; c < 2; ++c)
            if (t[r * 2 + c] >= 0) drawPiece(out, w, h, left, top, t[r * 2 + c], a.x8 + 2 * c, rowYw[r]);
        break;
      }
      case 0x300E:
        if (a.v1 < 0) break;
        drawPiece(out, w, h, left, top, 48 + 4 * a.v1, a.x8, a.yw - 0x80);
        drawPiece(out, w, h, left, top, 49 + 4 * a.v1, a.x8 + 2, a.yw - 0x80);
        drawPiece(out, w, h, left, top, 50 + 4 * a.v1, a.x8, a.yw);
        drawPiece(out, w, h, left, top, 51 + 4 * a.v1, a.x8 + 2, a.yw);
        break;
      case 0x300D:
        if (a.v1 < 0) break;
        drawPiece(out, w, h, left, top, 34 + 2 * a.v1, a.x8, a.yw);
        drawPiece(out, w, h, left, top, 35 + 2 * a.v1, a.x8 + 2, a.yw);
        break;
      case 0x301A:
        drawObjectPiece(out, w, h, left, top, (a.v0 ? 54 : 50) + phase_ % 4, a.x8, a.yw);
        break;
      case 0x3021:
        drawObjectPiece(out, w, h, left, top, 65 + phase_ % 4, a.x8, a.yw);
        break;
      case 0x302C:
        drawObjectPiece(out, w, h, left, top, a.v0 ? 96 : 95, a.x8, a.yw);
        break;
      case 0x3009:
      case 0x300A: {
        const int dir = a.id == 0x3009 ? 1 : -1;
        const bool right = a.id == 0x3009;
        if (a.state >= 0xf && a.state < 0x19 && a.state % 2 != 0) {
          drawObjectPiece(out, w, h, left, top, right ? 24 : 29, a.x8, a.yw);
        } else if (a.state >= 0x19 && a.state < 0x2d) {
          const int par = a.state % 2;
          drawObjectPiece(out, w, h, left, top, (right ? 25 : 30) + par, a.x8, a.yw);
          drawObjectPiece(out, w, h, left, top, (right ? 25 : 30) + par, a.x8 + 2 * dir, a.yw);
          drawObjectPiece(out, w, h, left, top, (right ? 27 : 32) + par, a.x8 + 4 * dir, a.yw);
        }
        break;
      }
      case 0x303C: {
        const int frame = (a.state == 1 ? 0 : 3) + (phase_ / 2) % 3;
        drawPiece(out, w, h, left, top, 240 + 2 * frame, a.x8, a.yw - 0x80);
        drawPiece(out, w, h, left, top, 241 + 2 * frame, a.x8, a.yw);
        break;
      }
      case 0x3012:
        if (a.state == 0) break;
        if (a.v0 == 0) {
          drawObjectPiece(out, w, h, left, top, 0, a.x8, a.yw);
        } else if (a.state < 0xc) {
          drawPiece(out, w, h, left, top, 112, a.x8, a.yw);
          if (a.state % 2 != 0) drawPiece(out, w, h, left, top, 113, a.x8, a.yw);
        }
        break;
      case 0x300B: {
        // Body piece (state 1-6 or the two knocked-out poses) above a four-frame propeller piece
        const int bodyPiece = a.state >= 0xfe ? (a.state == 0xfe ? 0 : 5) : a.state - 1;
        const int propeller = a.state >= 0xfe ? 6 : 6 + hoverAnim_;
        drawPiece(out, w, h, left, top, bodyPiece, a.x8, a.yw - 0x80);
        drawPiece(out, w, h, left, top, propeller, a.x8, a.yw);
        break;
      }
      case 0x3016:
      case 0x3017: {
        // Pieces 136-139 (states 1/2) and 140-143 (states 3/4); two animation counters run in opposite directions
        const int base = a.state <= 2 ? 136 : 140;
        const int frame = (a.state == 1 || a.state == 3) ? famAnimUp_ / 0xa0 : famAnimDown_ / 0xa0;
        drawPiece(out, w, h, left, top, base + frame, a.x8, a.yw);
        break;
      }
      default:
        break;
    }
  }
  shiftX_ = shiftY_ = 0;
}

}  // namespace duke1
