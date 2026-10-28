#include "play.hpp"
#include "assets/duke1_exe_text.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace rigel::duke1
{

namespace
{

#include "actor_table.inc"


constexpr int VIEW_X = 16, VIEW_Y = 16, VIEW_W = 208, VIEW_H = 160;

struct Placement
{
  int x, y, tile;
};

// Tiles of BORDER.DN1 forming the grey frame and panel titles around the 13x10
// play area. Layout read from an unscaled capture of the original game;
// contains no pixel data.
const Placement CHROME[] = {
  {0, 0, 4},      {16, 0, 16},    {32, 0, 2},     {40, 0, 2},
  {48, 0, 2},     {56, 0, 2},     {64, 0, 2},     {72, 0, 2},
  {80, 0, 2},     {88, 0, 2},     {96, 0, 2},     {104, 0, 2},
  {112, 0, 2},    {120, 0, 2},    {128, 0, 2},    {136, 0, 2},
  {144, 0, 2},    {152, 0, 2},    {160, 0, 2},    {168, 0, 2},
  {176, 0, 2},    {184, 0, 2},    {192, 0, 2},    {200, 0, 2},
  {208, 0, 2},    {224, 0, 5},    {240, 0, 8},    {256, 0, 38},
  {272, 0, 39},   {288, 0, 8},    {296, 0, 8},    {304, 0, 9},
  {224, 40, 14},  {240, 40, 8},   {256, 40, 36},  {272, 40, 37},
  {288, 40, 8},   {304, 40, 15},  {224, 80, 14},  {248, 80, 33},
  {264, 80, 34},  {280, 80, 35},  {304, 80, 15},  {224, 128, 14},
  {248, 128, 30}, {264, 128, 31}, {280, 128, 32}, {304, 128, 15},
  {0, 176, 7},    {16, 176, 3},   {24, 176, 3},   {32, 176, 3},
  {40, 176, 3},   {48, 176, 3},   {56, 176, 3},   {64, 176, 3},
  {72, 176, 3},   {88, 176, 26},  {104, 176, 27}, {120, 176, 28},
  {136, 176, 29}, {152, 176, 3},  {160, 176, 3},  {168, 176, 3},
  {176, 176, 3},  {184, 176, 3},  {192, 176, 3},  {200, 176, 3},
  {208, 176, 3},  {224, 176, 6},  {240, 176, 8},  {248, 176, 8},
  {256, 176, 8},  {264, 176, 8},  {272, 176, 8},  {280, 176, 8},
  {288, 176, 8},  {296, 176, 8},  {304, 176, 11},
};

struct Box
{
  int x0, y0, x1, y1;
};
const Box PANELS[] = {
  {239, 16, 306, 42},
  {239, 54, 306, 82},
  {239, 94, 306, 130},
  {239, 144, 306, 178}};


void fillRect(
  std::vector<std::uint8_t>& img,
  int x0,
  int y0,
  int x1,
  int y1,
  std::uint8_t r,
  std::uint8_t g,
  std::uint8_t b)
{
  for (int y = std::max(0, y0); y < std::min(SCREEN_H, y1); ++y)
    for (int x = std::max(0, x0); x < std::min(SCREEN_W, x1); ++x)
    {
      auto* p = &img[(static_cast<std::size_t>(y) * SCREEN_W + x) * 4];
      p[0] = r;
      p[1] = g;
      p[2] = b;
      p[3] = 255;
    }
}

} // namespace

void PlayWorld::blit(
  std::vector<std::uint8_t>& dst,
  int dx,
  int dy,
  const std::uint8_t* src,
  int srcStride,
  int sx,
  int sy,
  int w,
  int h,
  int cx0,
  int cy0,
  int cx1,
  int cy1,
  int dstW,
  int dstH) const
{
  (void)dstH;
  for (int y = 0; y < h; ++y)
  {
    const int ty = dy + y;
    if (ty < cy0 || ty >= cy1)
      continue;
    for (int x = 0; x < w; ++x)
    {
      const int tx = dx + x;
      if (tx < cx0 || tx >= cx1)
        continue;
      const auto* s =
        src + (static_cast<std::size_t>(sy + y) * srcStride + (sx + x)) * 4;
      if (s[3] == 0)
        continue;
      auto* d = &dst[(static_cast<std::size_t>(ty) * dstW + tx) * 4];
      d[0] = s[0];
      d[1] = s[1];
      d[2] = s[2];
      d[3] = 255;
    }
  }
}

void PlayWorld::drawTile(
  std::vector<std::uint8_t>& dst,
  const Tile& tile,
  int x,
  int y) const
{
  const auto px = tile.rgba();
  blit(dst, x, y, px.data(), 16, 0, 0, 16, 16, 0, 0, SCREEN_W, SCREEN_H);
}

PlayWorld::PlayWorld(
  const std::filesystem::path& dir,
  const std::string& levelName,
  const std::array<std::string, 2>* context,
  const int episode)
{
  mEpisode = std::filesystem::path(levelName).extension().empty()
    ? episode
    : episode_from_asset(levelName);
  const auto levelFile = std::filesystem::path(levelName).extension().empty()
    ? asset_name(levelName, mEpisode)
    : levelName;
  const auto executable = ExeText::load(dir, mEpisode);
  if (!executable)
    throw std::runtime_error(
      "missing or unsupported Duke 1 episode executable");
  const auto level = parse_level(read_file(dir, levelFile));
  mSandbox.world = CollisionWorld(0);
  std::copy(
    level.cells.begin(), level.cells.end(), mSandbox.world.cells.begin());

  // Player start marker: place camera and player like the original loader does.
  int startCol = -1, startRow = -1;
  for (std::size_t i = 0; i < mSandbox.world.cells.size(); ++i)
    if (mSandbox.world.cells[i] == 0x3032)
    {
      startCol = static_cast<int>(i % 128);
      startRow = static_cast<int>(i / 128);
    }
  if (startCol >= 0)
  {
    auto& cells = mSandbox.world.cells;
    const auto idx = static_cast<std::size_t>(startRow * 128 + startCol);
    cells[idx] = startCol > 0 ? cells[idx - 1] : 0;
    auto& p = mSandbox.player;
    p.cameraX8 = std::clamp(2 * startCol - 14, 0, 230);
    p.x8 = 2 * startCol - p.cameraX8;
    p.cameraYWords = std::max(0, 128 * startRow - 384);
    p.y = 16 * startRow + 16 - p.cameraYWords / 8;
  }

  // Replace active IDs with the cell value the original loader leaves behind,
  // and remember the actors so their sprites can be drawn on top.
  auto& cells = mSandbox.world.cells;
  for (std::size_t i = 0; i < cells.size(); ++i)
  {
    const auto v = cells[i];
    if (v < 0x3000 || v == 0x3032)
      continue;
    for (const auto& a : kActors)
    {
      // The registered finale uses the same loader record and sprite family as
      // the escaping boss.
      if (a.id != v && !(v == 0x3042 && a.id == 0x3043))
        continue;
      const bool keepSolid = v == 0x302B; // the column stays a solid cell
      int row = static_cast<int>(i / 128);
      const int col = static_cast<int>(i % 128);
      // The loader leaves a copy of a neighbouring cell (or a fixed tile)
      // behind the actor.
      std::uint16_t under = a.cell;
      if (a.copyMode == 1)
        under = col > 0 ? cells[i - 1] : 0;
      else if (a.copyMode == 2)
        under = col < 127 ? cells[i + 1] : 0;
      else if (a.copyMode == 3)
        under = i + 128 < cells.size() ? cells[i + 128] : 0;
      else if (a.copyMode == 4)
        under = i >= 128 ? cells[i - 128] : 0;
      if (under >= 0x3000)
        under = col > 0 && cells[i - 1] < 0x3000 ? cells[i - 1] : 0;
      if (!keepSolid)
        cells[i] = under;
      mActors.push_back({v, col, row, true, false, a.hits, under, i});
      initPorted(mActors.back());
      switch (v)
      {
        case 0x3024:
          ++mBonusTotals[0];
          break;
        case 0x302A:
          ++mBonusTotals[1];
          break;
        case 0x3007:
          ++mBonusTotals[2];
          break;
        case 0x3013:
          ++mBonusTotals[3];
          break;
        case 0x303B:
          ++mBonusTotals[4];
          break;
        default:
          break;
      }
      const bool fallsOnScreen = v == 0x3015 || v == 0x3052 || v == 0x3018 ||
        v == 0x3051 || v == 0x3000 || v == 0x3023;
      if (a.settle && mActors.back().ported && !fallsOnScreen)
      {
        // These start out resting; plain boxes, crates and food fall visibly
        // once they come on screen
        int r = row;
        while (r < 89 &&
               cells[static_cast<std::size_t>((r + 1) * 128 + col)] < 0x1800)
          ++r;
        mActors.back().row = r;
        mActors.back().yw += (r - row) * 128;
        mActors.back().prevYw = mActors.back().yw;
      }
      break;
    }
  }

  // Active IDs and anything not yet understood render as empty space; the
  // sandbox treats them as map values.
  for (int solid = 0; solid < 2; ++solid)
    for (int n = 0; n < 4; ++n)
    {
      const auto set = parse_tiles(read_file(
        dir,
        asset_name(
          std::string(solid ? "SOLID" : "BACK") + std::to_string(n),
          mEpisode)));
      auto& dst = mBanks[static_cast<std::size_t>(solid * 4 + n)].rgba;
      dst.assign(96 * 128 * 4, 0);
      for (int j = 0; j < 48 && j < static_cast<int>(set.tiles.size()); ++j)
      {
        // Map tiles are drawn as solid blocks: the mask is ignored (it only
        // matters for sprites)
        const auto px = set.tiles[static_cast<std::size_t>(j)].rgba();
        for (int y = 0; y < 16; ++y)
          for (int x = 0; x < 16; ++x)
          {
            auto* d = &dst
                        [(static_cast<std::size_t>((j / 6) * 16 + y) * 96 +
                          (j % 6) * 16 + x) *
                         4];
            std::copy_n(&px[(static_cast<std::size_t>(y) * 16 + x) * 4], 3, d);
            d[3] = 255;
          }
      }
    }

  for (auto& b : mBackdrops)
    b.assign(VIEW_W * VIEW_H * 4, 0);
  if (context)
    mPair = *context;
  else if (const auto pair = executable->backdropPair(levelFile))
    mPair = *pair;
  if (!mPair[0].empty())
  {
    for (int i = 0; i < 2; ++i)
    {
      const auto bd = parse_backdrop_compat(
        read_file(dir, mPair[static_cast<std::size_t>(i)]),
        mPair[static_cast<std::size_t>(i)]);
      if (!bd.warning.empty())
        mWarnings.push_back(bd.warning);
      for (unsigned y = 0; y < std::min(160u, bd.image.height); ++y)
        for (unsigned x = 0; x < std::min(208u, bd.image.width); ++x)
          std::copy_n(
            &bd.image
               .rgba[(static_cast<std::size_t>(y) * bd.image.width + x) * 4],
            4,
            &mBackdrops[static_cast<std::size_t>(i)]
                       [(static_cast<std::size_t>(y) * VIEW_W + x) * 4]);
    }
  }
  else
  {
    mWarnings.push_back("Flat backdrop: map-to-pair relationship unverified");
  }
  // De-duplicate warnings (both pair members can report the same condition)
  std::sort(mWarnings.begin(), mWarnings.end());
  mWarnings.erase(
    std::unique(mWarnings.begin(), mWarnings.end()), mWarnings.end());

  {
    static const char* names[9] = {
      "ANIM0",
      "ANIM1",
      "ANIM2",
      "ANIM3",
      "ANIM4",
      "ANIM5",
      "OBJECT0",
      "OBJECT1",
      "OBJECT2"};
    for (int b = 0; b < 9; ++b)
    {
      const auto set =
        parse_tiles(read_file(dir, asset_name(names[b], mEpisode)));
      for (const auto& t : set.tiles)
        mSprites[static_cast<std::size_t>(b)].push_back(t.rgba());
    }
  }
  {
    const auto set =
      parse_tiles(read_file(dir, asset_name("NUMBERS", mEpisode)));
    for (const auto& t : set.tiles)
      mNumbers.push_back(t.rgba());
  }
  for (int i = 0; i < 5; ++i)
    mMan[static_cast<std::size_t>(i)] = parse_tiles(
      read_file(dir, asset_name("MAN" + std::to_string(i), mEpisode)));
  mBorder = parse_tiles(read_file(dir, asset_name("BORDER", mEpisode)));
  buildFrame();
  mPrevX = worldX();
  mPrevY = worldY();
}

void PlayWorld::buildFrame()
{
  mFrame.assign(static_cast<std::size_t>(SCREEN_W) * SCREEN_H * 4, 255);
  fillRect(mFrame, 0, 0, SCREEN_W, SCREEN_H, 170, 170, 170);
  for (const auto& p : CHROME)
  {
    if (p.tile < static_cast<int>(mBorder.tiles.size()))
      drawTile(
        mFrame, mBorder.tiles[static_cast<std::size_t>(p.tile)], p.x, p.y);
  }
  for (const auto& b : PANELS)
    fillRect(mFrame, b.x0, b.y0, b.x1, b.y1, 0, 0, 0);
  fillRect(mFrame, 0, 192, SCREEN_W, SCREEN_H, 0, 0, 0);
  fillRect(mFrame, VIEW_X, VIEW_Y, VIEW_X + VIEW_W, VIEW_Y + VIEW_H, 0, 0, 0);
}

std::array<std::string, 2> PlayWorld::backdropPairFor(
  const std::filesystem::path& dataDir,
  const std::string& levelName,
  const int episode)
{
  const auto executable = ExeText::load(dataDir, episode);
  if (!executable)
    return {};
  const auto pair = executable->backdropPair(levelName);
  return pair ? *pair : std::array<std::string, 2>{};
}

PlayWorld::Carry PlayWorld::carry() const
{
  const auto& p = mSandbox.player;
  return {mScore, p.health, p.boost, p.claw, p.capacity};
}

void PlayWorld::applyCarry(const Carry& c)
{
  auto& p = mSandbox.player;
  mScore = c.score;
  p.health = c.health;
  p.boost = c.boost;
  p.claw = c.claw;
  p.capacity = c.capacity;
}

void PlayWorld::update(const Input& input)
{
  mPrevX = worldX();
  mPrevY = worldY();
  ++mPhase;
  ++mUpdates;
  if (mWide)
    setActiveView(worldX() - 240, worldY() - 140, 480, 220);
  else
    mSandbox.wideCull = false;
  if (mSandbox.player.health <= 0)
  {
    // Duke is gone for a moment: a burst of sparks around him for about 30
    // updates, then the level restarts
    mSandbox.player.pendingHit = false;
    if (++mDyingTick == 1)
      sound(0);
    if (mDyingTick >= 2)
    {
      const auto& p = mSandbox.player;
      const int px = p.x8 + p.cameraX8;
      const int rowWords = ((p.y - 16) >> 4) * 0x80;
      const int a = rowWords + p.cameraYWords;
      const int b = 0x80 - rowWords + p.cameraYWords;
      struct Burst
      {
        int dx, yKind, yAdd;
      }; // yKind 0: a, 1: b
      static const std::vector<Burst> kBurst[19] = {
        {{0, 0, 0}, {0, 0, 0x80}},
        {{0, 0, 0x80}},
        {{-2, 1, 0}, {2, 1, 0}},
        {{2, 1, 0}},
        {{-3, 0, 0}, {3, 0, 0}},
        {{3, 0, 0}},
        {{0, 0, 0x80}},
        {{0, 0, 0}},
        {{0, 1, 0}},
        {{-2, 1, 0}, {3, 1, 0}},
        {{3, 1, 0}},
        {{-3, 0, 0}},
        {{2, 0, 0}, {0, 1, 0}},
        {{0, 1, 0}},
        {{-2, 1, 0}, {3, 1, 0}},
        {{3, 1, 0}},
        {{-3, 0, 0}},
        {{2, 0, 0}, {0, 1, 0}},
        {{0, 1, 0}}};
      const auto idx = static_cast<std::size_t>(mDyingTick - 1);
      if (idx < 19)
        for (const auto& s : kBurst[idx])
          spawnSparks((s.yKind ? b : a) + s.yAdd, px + s.dx);
    }
    updatePorted();
    updatePools();
    return;
  }
  if (!input.up)
    mUpConsumed = false;
  mSandbox.input = mFrozen ? Input{} : input;
  const int pullUpBefore = mSandbox.player.pullUp;
  const int immunityBefore = mSandbox.player.immunity;
  mSandbox.player.phase = (mPhase % 4) *
    32; // the shared animation step the original's player code looks at
  mSandbox.tick();
  mPullUpDrawn = pullUpBefore > 0 || mSandbox.player.pullUp > 0;
  if (mSandbox.player.immunity > immunityBefore)
    mDamageTaken = true;
  if (mSandbox.player.immunity > immunityBefore)
    mHitFlash =
      1; // just hit: the original shows the skeleton for a single update
  else if (mHitFlash > 0)
    --mHitFlash;
  for (const auto& e : mSandbox.events)
    if (e.kind == Event::Score)
      mScore += e.value;
    else if (e.kind == Event::Break)
      spawnDebris((e.value / 128) * 128, (e.value % 128) * 2 + 2);
    else if (e.kind == Event::Sound)
      sound(e.value);
  mSandbox.events.clear();
  // DOS wall contacts allocate the small enemy-impact effect without a sound.
  for (const auto& impact : mSandbox.impacts)
    mExplosions.push_back({1, impact.first, impact.second, 1});
  mSandbox.impacts.clear();
  for (auto& pop : mPopups)
    ++pop.age;
  mPopups.erase(
    std::remove_if(
      mPopups.begin(),
      mPopups.end(),
      [](const Popup& q) { return q.age > 40; }),
    mPopups.end());
  mAirTicks =
    (mSandbox.player.grounded || mSandbox.player.cling) ? 0 : mAirTicks + 1;
  updatePorted();
  updatePools();
  interact();
  if (mDiscontinuity)
  {
    mPrevX = worldX();
    mPrevY = worldY();
    for (auto& actor : mActors)
    {
      actor.prevX8 = actor.x8;
      actor.prevYw = actor.yw;
    }
  }
}

void PlayWorld::render(std::vector<std::uint8_t>& out) const
{
  out = mFrame;
  const auto& p = mSandbox.player;
  std::vector<std::uint8_t> view;
  renderWorld(
    view,
    VIEW_W,
    VIEW_H,
    p.cameraX8 * 8,
    p.cameraYWords / 8,
    worldX(),
    worldY());
  for (int y = 0; y < VIEW_H; ++y)
    std::copy_n(
      &view[static_cast<std::size_t>(y) * VIEW_W * 4],
      VIEW_W * 4,
      &out[(static_cast<std::size_t>(VIEW_Y + y) * SCREEN_W + VIEW_X) * 4]);

  drawInventory(out, SCREEN_W, SCREEN_H, 244, 150);

  // Health: nine green blocks in the health panel.
  for (int i = 0; i < 9; ++i)
    fillRect(
      out,
      242 + i * 7,
      60,
      247 + i * 7,
      74,
      i < p.health ? 0 : 40,
      i < p.health ? 170 : 40,
      0);
}

void PlayWorld::addScore(int value, int x, int y)
{
  mScore += value;
  mPopups.push_back({x, y, value, 0});
}

void PlayWorld::interact()
{
  const auto entryFor = [](std::uint16_t id) -> const ActorEntry* {
    for (const auto& a : kActors)
      if (a.id == id)
        return &a;
    return nullptr;
  };
  auto& p = mSandbox.player;

  for (auto& actor : mActors)
  {
    if (!actor.alive || actor.ported)
      continue;
    const auto* entry = entryFor(actor.id);
    if (!entry)
      continue;
    actor.prevRow = actor.row;
    // Boxes and items fall a row per update while they are on screen and
    // nothing solid is below them Like the original: boxes only fall while they
    // are in view (matches the console release)
    if (
      entry->settle && actor.row < 89 &&
      !offscreen(actor.row * 128, actor.col * 2 + 2))
    {
      const auto below =
        mSandbox.world
          .cells[static_cast<std::size_t>((actor.row + 1) * 128 + actor.col)];
      if (below < 0x1800)
        ++actor.row;
    }
    const int ax8 =
      actor.col * 2 + 2; // the loader adds two units (the frame border)
    const int ayWords = actor.row * 128;

    // Shots (shared bullet predicate: horizontal distance < 2 units, vertical
    // within one row)
    const bool wasOpen =
      actor.open; // contents are only collected if the box was already open at
                  // the start of the update
    const bool plainCrate = false;
    if (
      entry->shotScore > 0 || plainCrate || (entry->isContainer && !actor.open))
    {
      for (auto& b : mSandbox.bullets)
      {
        if (b.kind == 0)
          continue;
        const int dx = b.x8 - ax8, dy = b.yWords - ayWords;
        if (dx > -2 && dx < 2 && dy >= -128 && dy < 128)
        {
          b.kind = 0;
          if (plainCrate)
          {
            actor.alive = false;
            mSandbox.world.cells[actor.cellIndex] = actor.under;
          }
          else if (entry->isContainer)
          {
            actor.open = true;
            spawnSparks(actor.row * 128, actor.col * 2 + 2);
            if (
              actor.row >= 89 ||
              mSandbox.world.cells[static_cast<std::size_t>(
                (actor.row + 1) * 128 + actor.col)] < 0x1800)
              addScore(
                500,
                actor.col * 16,
                actor.row * 16); // a box shot open in mid-air pays extra
          }
          else if (--actor.hits <= 0)
          {
            actor.alive = false;
            mSandbox.world.cells[actor.cellIndex] = actor.under;
            addScore(
              static_cast<int>(entry->shotScore),
              actor.col * 16,
              actor.row * 16);
          }
          break;
        }
      }
      if (!actor.alive)
        continue;
    }

    // Touch: items that are out in the open, or containers that have been shot
    // open
    const bool collectable = (entry->isPickup && entry->touchScore > 0) ||
      (entry->isContainer && wasOpen);
    if (collectable)
    {
      if (touchesPlayer(ax8, ayWords))
      {
        actor.alive = false;
        mSandbox.world.cells[actor.cellIndex] = actor.under;
        int award = static_cast<int>(
          entry->isContainer ? entry->openScore : entry->touchScore);
        // Flag and radio items pay 100, 500, 2000 or 5000 depending on the
        // shared animation phase.
        if (
          actor.id == 0x302D || actor.id == 0x302E || actor.id == 0x3055 ||
          actor.id == 0x3056)
        {
          static const int kPhasePay[4] = {100, 500, 2000, 5000};
          award = kPhasePay[mPhase % 4];
        }
        addScore(award, actor.col * 16, actor.row * 16);
        if (entry->heal && p.health < 9)
          ++p.health;
        switch (actor.id)
        {
          case 0x3006:
          case 0x3008:
          case 0x300F:
          case 0x3020:
          case 0x3033:
            sound(12);
            break;
          default:
            sound(entry->heal ? 1 : 16);
            break;
        }
        switch (actor.id)
        {
          case 0x3020:
            mHasItem = true;
            break; // item that powers the wall machines
          case 0x3033:
            mHasCard = true;
            break; // access card
          case 0x3006:
            p.boost = 1;
            break; // boots: higher jump
          case 0x3008:
            p.claw = 1;
            break; // claw: ceiling cling
          case 0x300F:
            p.capacity = std::min(4, p.capacity + 1);
            break; // gun: more shots at once
          default:
            break;
        }
      }
    }
  }
}

void PlayWorld::renderWorld(
  std::vector<std::uint8_t>& out,
  int w,
  int h,
  int left,
  int top,
  int playerX,
  int playerY) const
{
  out.assign(static_cast<std::size_t>(w) * h * 4, 0);
  for (std::size_t i = 3; i < out.size(); i += 4)
    out[i] = 255;

  const int col0 = std::max(0, left / 16 - (left < 0 ? 1 : 0)),
            col1 = std::min(127, (left + w) / 16);
  const int row0 = std::max(0, top / 16 - (top < 0 ? 1 : 0)),
            row1 = std::min(89, (top + h) / 16);
  for (int row = row0; row <= row1; ++row)
  {
    for (int col = col0; col <= col1; ++col)
    {
      unsigned v =
        mSandbox.world.cells[static_cast<std::size_t>(row * 128 + col)];
      if (v > 0x20 && v < 0x5E0)
        v += 32 * (mPhase % 4); // animated tiles cycle through four images
      const int sx = col * 16 - left, sy = row * 16 - top;
      if (v == 0 || v == 0x20)
      {
        // Backdrops are fixed to the screen and repeat across the wider view.
        const auto& bd = mBackdrops[v == 0 ? 0 : 1];
        for (int y = 0; y < 16; ++y)
        {
          const int ty = sy + y;
          if (ty < 0 || ty >= h)
            continue;
          for (int x = 0; x < 16; ++x)
          {
            const int tx = sx + x;
            if (tx < 0 || tx >= w)
              continue;
            // In a taller view the picture is stretched to cover the whole
            // height (it repeats only sideways)
            const int by = h > VIEW_H ? std::min(VIEW_H - 1, ty * VIEW_H / h)
                                      : ((ty % VIEW_H) + VIEW_H) % VIEW_H;
            const auto* s = &bd
                              [(static_cast<std::size_t>(by) * VIEW_W +
                                ((tx % VIEW_W) + VIEW_W) % VIEW_W) *
                               4];
            if (s[3] == 0)
              continue;
            auto* d = &out[(static_cast<std::size_t>(ty) * w + tx) * 4];
            d[0] = s[0];
            d[1] = s[1];
            d[2] = s[2];
          }
        }
      }
      else if (v < 0x3000 && v % 32 == 0)
      {
        const int group = v < 0x1800
          ? static_cast<int>(v / 0x600)
          : 4 + static_cast<int>((v - 0x1800) / 0x600);
        const int index = static_cast<int>((v % 0x600) / 32);
        if (group < 8)
          blit(
            out,
            sx,
            sy,
            mBanks[static_cast<std::size_t>(group)].rgba.data(),
            96,
            (index % 6) * 16,
            (index / 6) * 16,
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
  }

  for (const auto& actor : mActors)
  {
    if (!actor.alive || actor.ported)
      continue;
    const ActorEntry* entry = nullptr;
    for (const auto& a : kActors)
      if (a.id == actor.id)
      {
        entry = &a;
        break;
      }
    if (!entry)
      continue;
    ActorUpdate upd{0, 0};
    if (actor.open && entry->exposedId)
    {
      for (const auto& e2 : kActors)
        if (e2.id == entry->exposedId && e2.updateCount)
        {
          upd = kActorUpdates[e2.firstUpdate + mPhase % e2.updateCount];
          if (upd.count == 0)
            upd = kActorUpdates[e2.firstUpdate];
        }
    }
    else if (actor.open && entry->openUpdate != 0xFFFF)
    {
      upd = kActorUpdates[entry->openUpdate];
    }
    else if (entry->updateCount != 0)
    {
      upd = kActorUpdates[entry->firstUpdate + mPhase % entry->updateCount];
      if (upd.count == 0)
        upd = kActorUpdates[entry->firstUpdate]; // drawn once and then left in
                                                 // place
    }
    const int ax = actor.col * 16, ay = actor.row * 16;
    // Falling boxes and items glide between rows instead of jumping a whole row
    // per update
    const int fallShift = actor.prevRow >= 0 && actor.prevRow != actor.row
      ? static_cast<int>(
          std::lround((actor.row - actor.prevRow) * 16 * (mBlend - 1.0)))
      : 0;
    if (
      ax + 48 < left || ax - 48 > left + w || ay + 48 < top ||
      ay - 48 > top + h)
      continue;
    for (int k = 0; k < upd.count; ++k)
    {
      const auto& dr = kActorDraws[upd.first + k];
      const auto& bank = mSprites[dr.bank];
      if (dr.frame >= bank.size())
        continue;
      blit(
        out,
        ax + dr.dx - left,
        ay + dr.dy - top + fallShift,
        bank[dr.frame].data(),
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

  drawPorted(out, w, h, left, top);
  drawPools(out, w, h, left, top);

  // Score sprites: a two-colour picture of the value, rising and then gone
  for (const auto& pop : mPopups)
  {
    int base = -1;
    switch (pop.value)
    {
      case 100:
        base = 0;
        break;
      case 200:
        base = 2;
        break;
      case 500:
        base = 4;
        break;
      case 1000:
        base = 6;
        break;
      case 2000:
        base = 8;
        break;
      case 5000:
        base = 10;
        break;
      case 10000:
        base = 12;
        break;
      default:
        break;
    }
    if (base < 0)
      continue;
    const std::size_t idx = static_cast<std::size_t>(base + pop.age % 2);
    if (idx >= mNumbers.size())
      continue;
    blit(
      out,
      pop.x - left,
      pop.y - top - pop.age,
      mNumbers[idx].data(),
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

  const auto& p = mSandbox.player;
  const bool blink = p.immunity > 0 && (p.immunity % 2) == 0 && mHitFlash == 0;
  if (!blink && mExitState < 2 && p.health > 0)
  {
    const bool leftFacing = p.facing == 0;
    // MAN0 groups 0-3: walking left, 4-7: walking right; MAN1 groups 0/1:
    // standing left/right
    const bool stationary =
      p.grounded && !(mSandbox.input.left || mSandbox.input.right);
    const bool standing = stationary && !mSandbox.input.fire;
    const Tileset* pSet = standing ? &mMan[1] : &mMan[0];
    const int step = stationary && mSandbox.input.fire ? 3 : p.walkFrame % 4;
    int base =
      standing ? (leftFacing ? 0 : 4) : 4 * ((leftFacing ? 0 : 4) + step);
    auto useFrame = [&](const int frame) {
      pSet = &mMan[static_cast<std::size_t>(frame / 12)];
      base = 4 * (frame % 12);
    };
    if (mPullUpDrawn)
    {
      // Hauling himself up from a ceiling bar: frames 0x28 - n (left) or 0x2b -
      // n (right), n = what is left of the pull-up
      useFrame((leftFacing ? 0x28 : 0x2b) - p.pullUp);
    }
    else if (p.cling)
    {
      // Hanging from a bar: hand-over-hand frames 0x1c-0x1f (left) / 0x20-0x23
      // (right); firing uses 0x24 / 0x25
      if (mSandbox.input.fire)
        useFrame(leftFacing ? 0x24 : 0x25);
      else
        useFrame((leftFacing ? 0x1c : 0x20) + p.walkFrame % 4);
    }
    else if (p.jumpTravel)
    {
      // Boots flip (the core decides when): frames 14-20 (left) or 21-27
      // (right) of the 4-piece player frames, two updates each
      useFrame(
        (p.jumpTravel == 1 ? 14 : 21) + std::min(p.jumpTravelAge, 13) / 2);
    }
    else if (!p.grounded)
    {
      // Jump poses (identified from an original DOS capture): MAN0 groups 8/9
      // while rising, 10/11 while falling
      const bool rising =
        worldY() < mPrevY || (worldY() == mPrevY && mAirTicks <= 4);
      base = 4 * ((rising ? 8 : 10) + (leftFacing ? 0 : 1));
      pSet = &mMan[0];
    }
    if (mHitFlash > 0)
    { // Duke is hit: his skeleton flashes (MAN3 pieces 0x20-0x27)
      pSet = &mMan[3];
      base = leftFacing ? 32 : 36;
    }
    const auto& set = *pSet;
    const int px = playerX - left, py = playerY - top;
    // DOS draws at x8-1/x8+1 on a screen whose playfield starts at x=16.
    const int offs[4][2] = {{-24, -32}, {-8, -32}, {-24, -16}, {-8, -16}};
    for (int i = 0; i < 4; ++i)
    {
      const auto idx = static_cast<std::size_t>(base + i);
      if (idx >= set.tiles.size())
        continue;
      const auto rgba = set.tiles[idx].rgba();
      blit(
        out,
        px + offs[i][0],
        py + offs[i][1],
        rgba.data(),
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
  // The original projectile pass draws after the player.
  // Shots: the wave piece (OBJECT0 pieces 6..9) cycles while the bullet
  // travels.
  for (const auto& b : mSandbox.bullets)
  {
    if (b.kind == 0)
      continue;
    const auto& bank = mSprites[6];
    const bool leftFacing = b.kind == 1;
    const bool muzzle = b.newFlag || b.age <= 1;
    const std::size_t frame =
      muzzle ? (leftFacing ? 46 : 47) : (leftFacing ? 6 : 8) + mPhase % 2;
    if (frame >= bank.size())
      continue;
    // Shots travel 16 px per update; draw them where they were that far back
    // along the path. A shot that was just fired has no earlier position (it
    // would be drawn inside Duke), so it shows where the first update put it.
    const int shotShift = (b.newFlag || b.age <= 1)
      ? 0
      : static_cast<int>(std::lround(16 * (1.0 - mBlend))) *
        (b.kind == 1 ? 1 : -1);
    blit(
      out,
      (b.x8 - 2) * 8 - left + shotShift,
      b.yWords / 8 + 3 - top,
      bank[frame].data(),
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

} // namespace rigel::duke1

namespace rigel::duke1
{

std::vector<std::uint8_t> PlayWorld::standingPortrait() const
{
  std::vector<std::uint8_t> out(32 * 32 * 4, 0);
  const int offs[4][2] = {{0, 0}, {16, 0}, {0, 16}, {16, 16}};
  for (int i = 0; i < 4; ++i)
  {
    const auto idx = static_cast<std::size_t>(4 + i);
    if (idx >= mMan[1].tiles.size())
      continue;
    const auto rgba = mMan[1].tiles[idx].rgba();
    for (int y = 0; y < 16; ++y)
      for (int x = 0; x < 16; ++x)
        for (int c = 0; c < 4; ++c)
          out
            [(static_cast<std::size_t>(offs[i][1] + y) * 32 +
              static_cast<std::size_t>(offs[i][0] + x)) *
               4 +
             c] = rgba
              [(static_cast<std::size_t>(y) * 16 +
                static_cast<std::size_t>(x)) *
                 4 +
               c];
  }
  return out;
}


PlayWorld::BonusReport PlayWorld::bonusReport() const
{
  BonusReport r;
  auto allGone = [&](const std::uint16_t id) {
    for (const auto& a : mActors)
      if (a.id == id && a.alive)
        return false;
    return true;
  };
  r.cameraTotal = mBonusTotals[0];
  const auto camerasRemaining =
    std::count_if(mActors.begin(), mActors.end(), [](const auto& actor) {
      return actor.id == 0x3024 && actor.alive;
    });
  r.cameraDestroyed = std::max(0, r.cameraTotal - int(camerasRemaining));
  r.present[Cameras] = mBonusTotals[0] > 0;
  r.earned[Cameras] = mBonusTotals[0] > 0 && allGone(0x3024);
  r.present[AcmeSigns] = mBonusTotals[1] > 0;
  r.earned[AcmeSigns] = mBonusTotals[1] > 0 && allGone(0x302A);
  r.present[Rockets] = mBonusTotals[2] > 0;
  r.earned[Rockets] = mBonusTotals[2] > 0 && allGone(0x3007);
  r.present[SnakeBots] = mBonusTotals[3] > 0;
  r.earned[SnakeBots] = mBonusTotals[3] > 0 && mSnakesDone;
  r.present[BunnyBots] = mBonusTotals[4] > 0;
  r.earned[BunnyBots] = mBonusTotals[4] > 0 && allGone(0x303B);
  r.present[NoDamage] = true;
  r.earned[NoDamage] = !mDamageTaken;
  r.present[DukeLetters] = true;
  r.earned[DukeLetters] = mDukeBonus;
  return r;
}

} // namespace rigel::duke1
