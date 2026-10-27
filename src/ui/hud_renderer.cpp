/* Copyright (C) 2016, Nikolai Wuttke. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "hud_renderer.hpp"

#include "assets/palette.hpp"
#include "assets/resource_loader.hpp"
#include "data/game_options.hpp"
#include "data/game_traits.hpp"
#include "data/unit_conversions.hpp"
#include "engine/sprite_factory.hpp"
#include "renderer/upscaling.hpp"
#include "renderer/viewport_utils.hpp"
#include "ui/menu_element_renderer.hpp"

#include <algorithm>
#include <cmath>
#include <string>


namespace rigel::ui
{

using namespace rigel::data;
using namespace rigel::engine;
using namespace rigel::renderer;


namespace
{

constexpr auto HEALTH_BAR_LABEL_START_X = 0;
constexpr auto HEALTH_BAR_LABEL_START_Y = 0;
constexpr auto HEALTH_BAR_TILE_INDEX = 4 * 40 + 1;
constexpr auto HEALTH_BAR_START_PX = base::Vec2{data::tilesToPixels(5), 0};

constexpr auto NUM_HEALTH_SLICES = 8;

constexpr auto RADAR_SIZE_PX = 32;
constexpr auto RADAR_CENTER_POS_X = 288;
constexpr auto RADAR_CENTER_POS_Y = 136;

constexpr auto RADAR_POS_X = RADAR_CENTER_POS_X - RADAR_SIZE_PX / 2 -
  data::GameTraits::inGameViewportOffset.x;
constexpr auto RADAR_POS_Y = RADAR_CENTER_POS_Y - RADAR_SIZE_PX / 2 -
  data::GameTraits::inGameViewportOffset.y;
constexpr auto RADAR_CENTER_OFFSET_RELATIVE =
  base::Vec2{RADAR_SIZE_PX / 2, RADAR_SIZE_PX / 2 + 1};

constexpr auto HUD_START_TOP_RIGHT =
  base::Vec2{data::GameTraits::mapViewportWidthTiles, 0};
constexpr auto HUD_START_BOTTOM_LEFT =
  base::Vec2{0, data::GameTraits::mapViewportHeightTiles};
constexpr auto HUD_START_BOTTOM_RIGHT = base::Vec2{
  HUD_START_BOTTOM_LEFT.x + 28,
  data::GameTraits::mapViewportHeightTiles};

constexpr auto INVENTORY_START_POS = base::Vec2{HUD_START_TOP_RIGHT.x + 1, 2};

// The letter collection indicator actors already contain an offset in the
// actor info that positions them correctly. Unfortunately, that offset is
// relative to the entire screen, but in our HUD renderer, everything is
// relative to the start of the map viewport, i.e. offset by {1, 1} tiles.  We
// have to account for that and render the indicators further up/left to negate
// this offset. On top of that, we need to offset one more to the left and one
// more up, because that's how the original game's coordinate system works - a
// coordinate for actor rendering actually refers to the actor's bottom-left,
// so we have to subtract one on the Y axis (each letter indicator is 2 tiles
// tall), and then we also have to subtract one on the X axis since the X
// coordinate refers to the tile after the actor's left-most tile for whatever
// reason. It's not clear why, the original executable's code literally has `x
// - 1` in the function that's used for drawing actors in the UI/HUD.  The
// in-game sprite drawing code doesn't have that behavior (but it does have the
// 'Y refers to bottom-most tile' part).  TODO: Maybe don't hardcode the height
// of the indicators?
constexpr auto LETTER_INDICATOR_POSITION = base::Vec2{-2, -2};

constexpr auto NUM_RADAR_BLINK_STEPS = 4;
constexpr auto RADAR_BLINK_START_COLOR_INDEX = 3;
const auto RADAR_DOT_COLOR = data::GameTraits::INGAME_PALETTE[15];


constexpr auto OVERLAY_BACKGROUND_COLOR = []() {
  auto color = data::GameTraits::INGAME_PALETTE[1];
  color.a = 240;
  return color;
}();


void drawNumbersBig(
  const int number,
  const int maxDigits,
  const base::Vec2& tlPosition,
  const TiledTexture& spriteSheet)
{
  auto remainingNumber = number;

  for (auto digitIndex = 0; digitIndex < maxDigits; ++digitIndex)
  {
    // Draw digits from right to left, until the number is fully drawn or
    // we run out of digits to draw.
    const auto tlPositionForDigit =
      tlPosition + base::Vec2{(maxDigits - 1 - digitIndex) * 2, 0};

    const auto digitValue = remainingNumber % 10;
    remainingNumber /= 10;

    // clang-format off
    spriteSheet.renderTileQuad(digitValue*2 + 7*40, tlPositionForDigit);
    // clang-format on

    if (remainingNumber == 0)
    {
      break;
    }
  }
}


void drawScore(
  const int score,
  const TiledTexture& spriteSheet,
  const base::Vec2& position)
{
  drawNumbersBig(score, 7, position, spriteSheet);
}


void drawWeaponIcon(
  const WeaponType type,
  const TiledTexture& spriteSheet,
  const base::Vec2& position)
{
  const auto weaponIndex = static_cast<int>(type);
  spriteSheet.renderTileDoubleQuad(weaponIndex * 4 + 4 + 5 * 40, position);
}


void drawAmmoBar(
  const int currentAmmo,
  const int maxAmmo,
  const TiledTexture& spriteSheet,
  const base::Vec2& position)
{
  // The sprite sheet has 17 bar sizes; index 0 is full, 16 is empty.
  // Starting at col 0, row 23. Each bar is 2 tiles high

  const auto quantizedAmmoCount = static_cast<int>(
    std::ceil(static_cast<float>(currentAmmo) / maxAmmo * 16.0f));

  const auto ammoBarIndex = 16 - quantizedAmmoCount;
  spriteSheet.renderTileSlice(ammoBarIndex + 23 * 40, position);
}


void drawLevelNumber(
  const int number,
  const TiledTexture& spriteSheet,
  const base::Vec2& position)
{
  drawNumbersBig(number, 1, position, spriteSheet);
}


void drawWideHudFrameExtensions(
  const renderer::Texture& texture,
  int screenWidth,
  int yPos)
{
  if (screenWidth > texture.width())
  {
    const auto gapWidth = (screenWidth - texture.width()) / 2;

    // Left side
    texture.render({0, yPos}, {{}, {8, texture.height()}});
    texture.render(
      base::Rect<int>{{8, 0}, {8, texture.height()}},
      {{8, yPos}, {gapWidth, texture.height()}});

    // Right side
    const auto xPos = screenWidth - 8;
    const auto rightEdgeSrcPos = base::Vec2{texture.width() - 8, 0};
    texture.render({xPos, yPos}, {rightEdgeSrcPos, {8, texture.height()}});
    texture.render(
      base::Rect<int>{
        rightEdgeSrcPos - base::Vec2{8, 0}, {8, texture.height()}},
      {{xPos - gapWidth, yPos}, {gapWidth, texture.height()}});
  }
}

} // namespace


bool canUseHudStyle(
  const WidescreenHudStyle style,
  const renderer::Renderer* pRenderer)
{
  const auto availableWidth =
    renderer::determineLowResBufferWidth(pRenderer, true);

  switch (style)
  {
    case data::WidescreenHudStyle::Modern:
      return availableWidth >= assets::WIDE_HUD_INNER_WIDTH;

    case data::WidescreenHudStyle::Ultrawide:
      return availableWidth >= assets::ULTRAWIDE_HUD_INNER_WIDTH;

    default:
      return true;
  }
}


WidescreenHudStyle effectiveHudStyle(
  WidescreenHudStyle style,
  const renderer::Renderer* pRenderer)
{
  return canUseHudStyle(style, pRenderer) ? style
                                          : data::WidescreenHudStyle::Classic;
}


void drawBossHealthBar(
  int health,
  int startingHealth,
  int maxWidthPx,
  const ui::MenuElementRenderer& textRenderer,
  const engine::TiledTexture& uiSpriteSheet)
{
  textRenderer.drawSmallWhiteText(
    HEALTH_BAR_LABEL_START_X, HEALTH_BAR_LABEL_START_Y, "BOSS");

  const auto maxHealthBarSize = maxWidthPx - HEALTH_BAR_START_PX.x;

  const auto healthInPx = startingHealth > maxHealthBarSize
    ? base::round(float(health) / startingHealth * maxHealthBarSize)
    : health;

  const auto healthBarSize = base::Size{healthInPx, data::GameTraits::tileSize};
  uiSpriteSheet.renderTileStretched(
    HEALTH_BAR_TILE_INDEX, {HEALTH_BAR_START_PX, healthBarSize});
}


HudRenderer::HudRenderer(
  const std::optional<int> levelNumber,
  const data::GameOptions* pOptions,
  renderer::Renderer* pRenderer,
  engine::TiledTexture* pStatusSpriteSheet,
  renderer::Texture wideHudFrameTexture,
  renderer::Texture ultrawideHudFrameTexture,
  const engine::SpriteFactory* pSpriteFactory,
  const MenuElementRenderer* pTextRenderer)
  : mLevelNumber(levelNumber)
  , mpRenderer(pRenderer)
  , mpOptions(pOptions)
  , mWideHudFrameTexture(std::move(wideHudFrameTexture))
  , mUltrawideHudFrameTexture(std::move(ultrawideHudFrameTexture))
  , mpStatusSpriteSheetRenderer(pStatusSpriteSheet)
  , mpSpriteFactory(pSpriteFactory)
  , mpTextRenderer(pTextRenderer)
  , mRadarSurface(pRenderer, RADAR_SIZE_PX, RADAR_SIZE_PX)
{
}


void HudRenderer::updateAnimation()
{
  ++mElapsedFrames;
}


void HudRenderer::renderClassicHud(
  const data::PersistentPlayerState& persistentPlayerState,
  const base::ArrayView<base::Vec2> radarPositions)
{
  // We group drawing into what texture is used to minimize the amount of
  // OpenGL state switches needed.

  // These use the actor sprite sheet texture.
  drawActorFrame(
    ActorID::HUD_frame_background, 0, data::tilesToPixels(HUD_START_TOP_RIGHT));
  drawActorFrame(
    ActorID::HUD_frame_background,
    1,
    data::tilesToPixels(HUD_START_BOTTOM_LEFT));
  drawActorFrame(
    ActorID::HUD_frame_background,
    2,
    data::tilesToPixels(HUD_START_BOTTOM_RIGHT));
  drawInventory(
    persistentPlayerState.inventory(),
    data::tilesToPixels(INVENTORY_START_POS));
  drawCollectedLetters(
    persistentPlayerState, data::tilesToPixels(LETTER_INDICATOR_POSITION));

  // These use the UI sprite sheet texture.
  drawScore(
    persistentPlayerState.score(),
    *mpStatusSpriteSheetRenderer,
    {2, GameTraits::mapViewportSize.height + 1});
  drawWeaponIcon(
    persistentPlayerState.weapon(),
    *mpStatusSpriteSheetRenderer,
    {17, GameTraits::mapViewportSize.height + 1});
  drawAmmoBar(
    persistentPlayerState.ammo(),
    persistentPlayerState.currentMaxAmmo(),
    *mpStatusSpriteSheetRenderer,
    {22, GameTraits::mapViewportSize.height + 1});
  drawHealthBar(
    persistentPlayerState, {24, GameTraits::mapViewportSize.height + 1});

  if (mLevelNumber)
  {
    drawLevelNumber(
      *mLevelNumber,
      *mpStatusSpriteSheetRenderer,
      {GameTraits::mapViewportSize.width + 2,
       GameTraits::mapViewportSize.height});
  }

  drawRadar(radarPositions, {RADAR_POS_X, RADAR_POS_Y});
}


void HudRenderer::renderWidescreenHud(
  const int viewportWidth,
  const data::WidescreenHudStyle style,
  const data::PersistentPlayerState& persistentPlayerState,
  const base::ArrayView<base::Vec2> radarPositions)
{
  auto drawClassicWidescreenHud = [&]() {
    drawLeftSideExtension(viewportWidth);

    const auto extraTiles =
      viewportWidth - data::GameTraits::mapViewportWidthTiles;
    const auto hudOffset =
      (extraTiles - HUD_WIDTH_RIGHT) * data::GameTraits::tileSize;

    auto guard = renderer::saveState(mpRenderer);
    renderer::setLocalTranslation(mpRenderer, {hudOffset, 0});

    renderClassicHud(persistentPlayerState, radarPositions);
  };


  switch (effectiveHudStyle(style, mpRenderer))
  {
    case data::WidescreenHudStyle::Classic:
      drawClassicWidescreenHud();
      break;

    case data::WidescreenHudStyle::Modern:
      drawModernHud(viewportWidth, persistentPlayerState, radarPositions);
      break;

    case data::WidescreenHudStyle::Ultrawide:
      drawUltrawideHud(viewportWidth, persistentPlayerState, radarPositions);
      break;

    case data::WidescreenHudStyle::Overlay:
      drawOverlayHud(viewportWidth, persistentPlayerState, radarPositions);
      break;
  }
}


void HudRenderer::drawModernHud(
  int viewportWidth,
  const data::PersistentPlayerState& persistentPlayerState,
  base::ArrayView<base::Vec2> radarPositions)
{
  const auto screenWidth =
    renderer::determineLowResBufferWidth(mpRenderer, true);

  const auto hudWidthPx = mWideHudFrameTexture.width();
  const auto paddingForCentering = (screenWidth - hudWidthPx) / 2;

  // Radar and inventory, floating
  const auto rightEdgeForFloatingParts =
    screenWidth - std::max(0, paddingForCentering);
  drawFloatingInventory(
    persistentPlayerState.inventory(), {rightEdgeForFloatingParts - 2, 2});

  if (mpOptions->mShowRadarInModernHud)
  {
    const auto radarPosX = rightEdgeForFloatingParts - RADAR_SIZE_PX - 2;

    // padding + height of inventory + padding
    const auto radarPosY = 2 + data::tilesToPixels(2) + 2;

    mpRenderer->drawFilledRectangle(
      {{radarPosX, radarPosY}, {RADAR_SIZE_PX, RADAR_SIZE_PX}},
      OVERLAY_BACKGROUND_COLOR);
    drawRadar(radarPositions, {radarPosX, 20});
  }

  // HUD frame
  const auto hudStartY =
    data::tilesToPixels(data::GameTraits::mapViewportHeightTiles);

  mWideHudFrameTexture.render(
    paddingForCentering, data::tilesToPixels(HUD_START_BOTTOM_LEFT.y));
  drawWideHudFrameExtensions(mWideHudFrameTexture, screenWidth, hudStartY);

  // Contents of HUD frame
  // These all use the UI sprite sheet texture.
  auto guard = renderer::saveState(mpRenderer);
  renderer::setLocalTranslation(mpRenderer, {paddingForCentering + 29, 0});

  drawCollectedLetters(persistentPlayerState, {33, -23});

  drawScore(
    persistentPlayerState.score(),
    *mpStatusSpriteSheetRenderer,
    {2, GameTraits::mapViewportSize.height + 1});
  drawWeaponIcon(
    persistentPlayerState.weapon(),
    *mpStatusSpriteSheetRenderer,
    {17, GameTraits::mapViewportSize.height + 1});
  drawAmmoBar(
    persistentPlayerState.ammo(),
    persistentPlayerState.currentMaxAmmo(),
    *mpStatusSpriteSheetRenderer,
    {22, GameTraits::mapViewportSize.height + 1});
  drawHealthBar(
    persistentPlayerState, {24, GameTraits::mapViewportSize.height + 1});

  if (mLevelNumber)
  {
    renderer::setLocalTranslation(mpRenderer, {4, 2});
    drawLevelNumber(
      *mLevelNumber,
      *mpStatusSpriteSheetRenderer,
      {GameTraits::mapViewportSize.width + 2,
       GameTraits::mapViewportSize.height + 1});
  }
}


void HudRenderer::drawOverlayHud(
  int viewportWidth,
  const data::PersistentPlayerState& persistentPlayerState,
  base::ArrayView<base::Vec2> radarPositions)
{
  using base::Color;

  constexpr auto ORANGE = Color{255, 153, 0, 255};
  constexpr auto ORANGE_DIM = Color{110, 66, 0, 170};
  constexpr auto RED = Color{230, 30, 20, 255};
  constexpr auto BRACKET = Color{235, 235, 235, 230};
  constexpr auto GREEN = Color{120, 205, 0, 255};
  constexpr auto SLOT_BG = Color{0, 0, 0, 90};
  constexpr auto WHITE = Color{255, 255, 255, 255};
  constexpr auto LETTER_OFF = Color{150, 150, 150, 235};
  constexpr auto PLATE = Color{0, 0, 0, 120};

  constexpr auto margin = 4;
  constexpr auto tile = data::GameTraits::tileSize;
  // Use the viewport width, since the low-res buffer can be wider than the
  // area that's actually visible/clipped.
  const auto screenWidth = data::tilesToPixels(viewportWidth);
  const auto mapHeightPx =
    data::tilesToPixels(data::GameTraits::viewportHeightTiles - 1);

  // Corner brackets around a slot
  auto drawBrackets = [&, this](const base::Rect<int>& r) {
    constexpr auto arm = 3;
    const auto x0 = r.topLeft.x;
    const auto y0 = r.topLeft.y;
    const auto x1 = r.topLeft.x + r.size.width - 1;
    const auto y1 = r.topLeft.y + r.size.height - 1;
    for (const auto& corner : {base::Vec2{x0, y0}, base::Vec2{x1, y0},
                                base::Vec2{x0, y1}, base::Vec2{x1, y1}})
    {
      const auto left = corner.x == x0;
      const auto top = corner.y == y0;
      mpRenderer->drawFilledRectangle(
        {{left ? corner.x : corner.x - (arm - 1), corner.y}, {arm, 1}},
        BRACKET);
      mpRenderer->drawFilledRectangle(
        {{corner.x, top ? corner.y : corner.y - (arm - 1)}, {1, arm}},
        BRACKET);
    }
  };

  auto drawSlot = [&](const base::Rect<int>& r) {
    mpRenderer->drawFilledRectangle(r, SLOT_BG);
    drawBrackets(r);
  };

  auto drawText = [&](const base::Vec2& pxPos, std::string_view text, const Color& color) {
    auto guard = renderer::saveState(mpRenderer);
    renderer::setLocalTranslation(mpRenderer, pxPos);
    mpTextRenderer->drawBigText(0, 1, text, color);
  };

  // Top left: health pips
  {
    constexpr auto pipSize = 6;
    constexpr auto pipStep = tile;
    const auto numFull = persistentPlayerState.health() - 1;
    const auto lowHealthBlink = (mElapsedFrames / 4) % 2 == 0;

    mpRenderer->drawFilledRectangle(
      {{margin - 2, margin - 2}, {NUM_HEALTH_SLICES * pipStep + 2, pipSize + 4}},
      PLATE);

    for (auto i = 0; i < NUM_HEALTH_SLICES; ++i)
    {
      const auto color = numFull > 0 ? (i < numFull ? ORANGE : ORANGE_DIM)
                                     : (lowHealthBlink ? RED : ORANGE_DIM);
      mpRenderer->drawFilledRectangle(
        {{margin + i * pipStep, margin}, {pipSize, pipSize}}, color);
    }
  }

  // Top left: weapon and ammo
  {
    const auto box = base::Rect<int>{{margin, margin + 10}, {NUM_HEALTH_SLICES * tile - 2, 20}};
    drawSlot(box);

    {
      auto guard = renderer::saveState(mpRenderer);
      renderer::setLocalTranslation(
        mpRenderer, {box.topLeft.x + 1, box.topLeft.y + 2});
      drawWeaponIcon(
        persistentPlayerState.weapon(), *mpStatusSpriteSheetRenderer, {0, 0});
    }

    constexpr auto barWidth = 38;
    const auto barX = box.topLeft.x + 20;
    const auto barY = box.topLeft.y + 14;
    const auto maxAmmo = std::max(1, persistentPlayerState.currentMaxAmmo());
    const auto filled =
      base::round(float(persistentPlayerState.ammo()) / maxAmmo * barWidth);
    mpRenderer->drawFilledRectangle(
      {{barX, barY}, {barWidth, 3}}, Color{0, 0, 0, 150});
    mpRenderer->drawFilledRectangle({{barX, barY}, {filled, 3}}, GREEN);
  }

  // Top right: score and inventory slots
  const auto rightEdge = screenWidth - margin;
  {
    const auto scoreText = std::to_string(persistentPlayerState.score());
    const auto textWidthPx = int(scoreText.size()) * tile;
    mpRenderer->drawFilledRectangle(
      {{rightEdge - 5 * tile - 3, margin - 3}, {5 * tile + 6, 2 * tile + 8}},
      PLATE);
    drawText({rightEdge - 5 * tile, margin - 1}, "SCORE", LETTER_OFF);
    drawText({rightEdge - textWidthPx, margin + 8}, scoreText, WHITE);

    constexpr auto numSlots = 5;
    constexpr auto slotSize = 2 * tile;
    const auto slotsY = margin + 22;
    for (auto i = 0; i < numSlots; ++i)
    {
      drawSlot({{rightEdge - (i + 1) * (slotSize + 2) + 2, slotsY}, {slotSize, slotSize}});
    }

    drawFloatingInventory(
      persistentPlayerState.inventory(),
      {rightEdge - (2 + 0) + 0, slotsY},
      false,
      slotSize + 2);
  }

  // Bottom right: collected N U K E M letters
  {
    const auto collected = persistentPlayerState.collectedLetters();
    auto has = [&](data::CollectableLetterType t) {
      return std::find(collected.begin(), collected.end(), t) != collected.end();
    };
    using L = data::CollectableLetterType;
    const L letters[] = {L::N, L::U, L::K, L::E, L::M};
    const char names[] = {'N', 'U', 'K', 'E', 'M'};
    mpRenderer->drawFilledRectangle(
      {{rightEdge - 5 * tile - 3, mapHeightPx - margin - tile - 8},
       {5 * tile + 6, tile + 6}},
      PLATE);
    for (auto i = 0; i < 5; ++i)
    {
      drawText(
        {rightEdge - (5 - i) * tile, mapHeightPx - margin - tile - 7},
        std::string(1, names[i]),
        has(letters[i]) ? ORANGE : LETTER_OFF);
    }
  }

  // Bottom left: round radar
  if (mpOptions->mShowRadarInModernHud)
  {
    // Pixels are taller than wide with aspect ratio correction (x1.2), so the
    // circle needs to be squashed vertically to look round on screen.
    constexpr auto radius = 16;
    constexpr auto radiusY = 13;
    const auto radarPos = base::Vec2{margin + 2, mapHeightPx - margin - 2 * radius - 2};
    const auto center = radarPos + base::Vec2{radius, radius + 1};

    for (auto dy = -radiusY; dy <= radiusY; ++dy)
    {
      const auto half = base::round(
        radius * std::sqrt(1.0f - float(dy * dy) / float(radiusY * radiusY)));
      mpRenderer->drawFilledRectangle(
        {{center.x - half, center.y + dy}, {half * 2 + 1, 1}}, Color{0, 0, 0, 130});
    }

    drawRadar(radarPositions, radarPos);

    for (auto step = 0; step < 120; ++step)
    {
      const auto angle = float(step) / 120.0f * 6.2831853f;
      for (const auto shrink : {0.0f, 1.0f})
      {
        mpRenderer->drawPoint(
          {center.x + base::round(std::cos(angle) * (radius - shrink)),
           center.y + base::round(std::sin(angle) * (radiusY - shrink))},
          ORANGE);
      }
    }
  }
}


void HudRenderer::drawUltrawideHud(
  int viewportWidth,
  const data::PersistentPlayerState& persistentPlayerState,
  base::ArrayView<base::Vec2> radarPositions)
{
  const auto screenWidth =
    renderer::determineLowResBufferWidth(mpRenderer, true);
  const auto paddingForCentering =
    (screenWidth - assets::ULTRAWIDE_HUD_INNER_WIDTH) / 2;

  constexpr auto yPos = data::GameTraits::viewportHeightPx -
    data::GameTraits::inGameViewportOffset.y - assets::ULTRAWIDE_HUD_HEIGHT;

  // HUD frame
  mUltrawideHudFrameTexture.render(
    paddingForCentering -
      (assets::ULTRAWIDE_HUD_WIDTH - assets::ULTRAWIDE_HUD_INNER_WIDTH) / 2,
    yPos);
  drawWideHudFrameExtensions(mUltrawideHudFrameTexture, screenWidth, yPos);

  // Contents of HUD frame

  // These use the actor sprite sheet texture.
  {
    auto guard = renderer::saveState(mpRenderer);
    renderer::setLocalTranslation(mpRenderer, {paddingForCentering, yPos});

    drawInventory(persistentPlayerState.inventory(), {6, 15});
    drawCollectedLetters(persistentPlayerState, {64, -138});
  }

  auto guard = renderer::saveState(mpRenderer);
  renderer::setLocalTranslation(mpRenderer, {paddingForCentering, yPos - 2});

  // These use the UI sprite sheet texture.
  drawScore(
    persistentPlayerState.score(), *mpStatusSpriteSheetRenderer, {12, 6});
  drawWeaponIcon(
    persistentPlayerState.weapon(), *mpStatusSpriteSheetRenderer, {27, 6});
  drawAmmoBar(
    persistentPlayerState.ammo(),
    persistentPlayerState.currentMaxAmmo(),
    *mpStatusSpriteSheetRenderer,
    {32, 6});
  drawHealthBar(persistentPlayerState, {34, 6});

  if (mLevelNumber)
  {
    drawLevelNumber(*mLevelNumber, *mpStatusSpriteSheetRenderer, {44, 5});
  }

  drawRadar(radarPositions, {385, 36});
}


void HudRenderer::drawLeftSideExtension(const int viewportWidth) const
{
  const auto gapWidth = data::tilesToPixels(viewportWidth - HUD_WIDTH_TOTAL);
  const auto hudStartY =
    data::tilesToPixels(data::GameTraits::mapViewportHeightTiles);
  constexpr auto hudHeightPx = data::tilesToPixels(HUD_HEIGHT_BOTTOM);
  constexpr auto tileSize = data::GameTraits::tileSize;

  mWideHudFrameTexture.render({0, hudStartY}, {{}, {tileSize, hudHeightPx}});
  mWideHudFrameTexture.render(
    base::Rect<int>{{8, 0}, {tileSize, hudHeightPx}},
    {{tileSize, hudStartY}, {gapWidth - tileSize - 2, hudHeightPx}});
  mWideHudFrameTexture.render(
    base::Rect<int>{{27, 0}, {2, hudHeightPx}},
    {{gapWidth - 2, hudStartY}, {2, hudHeightPx}});
}


void HudRenderer::drawInventory(
  const std::vector<data::InventoryItemType>& inventory,
  const base::Vec2& position) const
{
  auto iItem = inventory.begin();
  for (int row = 0; row < 3; ++row)
  {
    for (int col = 0; col < 2; ++col)
    {
      if (iItem != inventory.end())
      {
        const auto itemType = *iItem++;
        const auto drawPos =
          position + data::tilesToPixels(base::Vec2{col * 2, row * 2});

        switch (itemType)
        {
          case InventoryItemType::CircuitBoard:
            drawActorFrame(ActorID::White_box_circuit_card, 0, drawPos);
            break;

          case InventoryItemType::BlueKey:
            drawActorFrame(ActorID::White_box_blue_key, 0, drawPos);
            break;

          case InventoryItemType::RapidFire:
            drawActorFrame(ActorID::Rapid_fire_icon, 0, drawPos);
            break;

          case InventoryItemType::SpecialHintGlobe:
            drawActorFrame(ActorID::Special_hint_globe_icon, 0, drawPos);
            break;

          case InventoryItemType::CloakingDevice:
            drawActorFrame(ActorID::Cloaking_device_icon, 0, drawPos);
            break;
        }
      }
    }
  }
}


void HudRenderer::drawFloatingInventory(
  const std::vector<data::InventoryItemType>& inventory,
  const base::Vec2& position,
  const bool drawBackground,
  const int slotStepPx) const
{
  if (drawBackground)
  {
    const auto numItems = int(inventory.size());
    const auto numItemSlots =
      mpOptions->mShowRadarInModernHud ? std::max(numItems, 2) : numItems;
    const auto backgroundSize =
      data::tilesToPixels(base::Size{numItemSlots * 2, 2});
    mpRenderer->drawFilledRectangle(
      {position - base::Vec2{backgroundSize.width, 0}, backgroundSize},
      OVERLAY_BACKGROUND_COLOR);
  }

  auto drawPos = position - base::Vec2{data::tilesToPixels(2), 0};

  for (const auto itemType : inventory)
  {
    switch (itemType)
    {
      case InventoryItemType::CircuitBoard:
        drawActorFrame(ActorID::White_box_circuit_card, 0, drawPos);
        break;

      case InventoryItemType::BlueKey:
        drawActorFrame(ActorID::White_box_blue_key, 0, drawPos);
        break;

      case InventoryItemType::RapidFire:
        drawActorFrame(ActorID::Rapid_fire_icon, 0, drawPos);
        break;

      case InventoryItemType::SpecialHintGlobe:
        drawActorFrame(ActorID::Special_hint_globe_icon, 0, drawPos);
        break;

      case InventoryItemType::CloakingDevice:
        drawActorFrame(ActorID::Cloaking_device_icon, 0, drawPos);
        break;
    }

    drawPos.x -= slotStepPx;
  }
}


void HudRenderer::drawHealthBar(
  const data::PersistentPlayerState& persistentPlayerState,
  const base::Vec2& position) const
{
  // Health slices start at col 20, row 4. The first 9 are for the "0 health"
  // animation

  // The model has a range of 1-9 for health, but the HUD shows only 8
  // slices, with a special animation for having 1 point of health.
  const auto numFullSlices = persistentPlayerState.health() - 1;
  if (numFullSlices > 0)
  {
    for (int i = 0; i < NUM_HEALTH_SLICES; ++i)
    {
      const auto sliceIndex = i < numFullSlices ? 9 : 10;
      mpStatusSpriteSheetRenderer->renderTileSlice(
        sliceIndex + 20 + 4 * 40, position + base::Vec2{i, 0});
    }
  }
  else
  {
    const auto animationOffset = mElapsedFrames;

    for (int i = 0; i < NUM_HEALTH_SLICES; ++i)
    {
      const auto sliceIndex = (i + animationOffset) % 9;
      mpStatusSpriteSheetRenderer->renderTileSlice(
        sliceIndex + 20 + 4 * 40, position + base::Vec2{i, 0});
    }
  }
}


void HudRenderer::drawCollectedLetters(
  const data::PersistentPlayerState& persistentPlayerState,
  const base::Vec2& position) const
{
  auto guard = renderer::saveState(mpRenderer);

  // The sprites used for the letter collection indicators don't just consist
  // of the lit up letter, but also contain the surrounding parts of the HUD.
  // Unfortunately, the color used in the sprites is slightly different from
  // what's used in the HUD. This causes a subtle discoloration in the HUD when
  // letters are collected.  To fix this, we set a clip rect to draw just the
  // part of the sprite which contains the lit up letter.
  renderer::setLocalClipRect(
    mpRenderer,
    {position + data::tilesToPixels(base::Vec2{35, 24}) + base::Vec2{1, 5},
     {29, 6}});

  for (const auto letter : persistentPlayerState.collectedLetters())
  {
    // The draw position is the same for all cases, because each actor
    // includes a draw offset in its actor info that positions it correctly.
    switch (letter)
    {
      case CollectableLetterType::N:
        drawActorFrame(ActorID::Letter_collection_indicator_N, 0, position);
        break;

      case CollectableLetterType::U:
        drawActorFrame(ActorID::Letter_collection_indicator_U, 0, position);
        break;

      case CollectableLetterType::K:
        drawActorFrame(ActorID::Letter_collection_indicator_K, 0, position);
        break;

      case CollectableLetterType::E:
        drawActorFrame(ActorID::Letter_collection_indicator_E, 0, position);
        break;

      case CollectableLetterType::M:
        drawActorFrame(ActorID::Letter_collection_indicator_M, 0, position);
        break;
    }
  }
}


void HudRenderer::drawRadar(
  const base::ArrayView<base::Vec2> positions,
  const base::Vec2& drawPosition) const
{
  auto drawDots = [&]() {
    for (const auto& position : positions)
    {
      const auto dotPosition = position + RADAR_CENTER_OFFSET_RELATIVE;
      mpRenderer->drawPoint(dotPosition, RADAR_DOT_COLOR);
    }

    const auto blinkColorIndex =
      mElapsedFrames % NUM_RADAR_BLINK_STEPS + RADAR_BLINK_START_COLOR_INDEX;
    const auto blinkColor = data::GameTraits::INGAME_PALETTE[blinkColorIndex];
    mpRenderer->drawPoint(RADAR_CENTER_OFFSET_RELATIVE, blinkColor);
  };


  if (mpOptions->mPerElementUpscalingEnabled)
  {
    {
      const auto saved = mRadarSurface.bindAndReset();
      mpRenderer->clear({0, 0, 0, 0});
      drawDots();
    }

    mRadarSurface.render(drawPosition);
  }
  else
  {
    const auto saved = renderer::saveState(mpRenderer);
    mpRenderer->setGlobalTranslation(
      mpRenderer->globalTranslation() + drawPosition);

    drawDots();
  }
}


void HudRenderer::drawActorFrame(
  const ActorID id,
  const int frame,
  const base::Vec2& pixelPos) const
{
  const auto& frameData = mpSpriteFactory->actorFrameData(id, frame);
  const auto destRect = base::Rect<int>{
    pixelPos + data::tilesToPixels(frameData.mDrawOffset),
    data::tilesToPixels(frameData.mDimensions)};
  mpSpriteFactory->textureAtlas().draw(frameData.mImageId, destRect);
}

} // namespace rigel::ui
