/* Copyright (C) 2026, RigelEngine contributors. All rights reserved.
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

#pragma once

#include "base/spatial_types.hpp"
#include "renderer/texture.hpp"

#include <array>
#include <filesystem>
#include <optional>


namespace rigel::assets
{
class ResourceLoader;
}


namespace rigel::ui
{

/** Resolution class the overlay HUD is laid out for.
 *
 * Each tier has its own atlas image, its own sprite rectangle table, and its
 * own set of layout constants. All layout values are in "reference pixels" of
 * the tier, i.e. they get scaled by (window height / referenceHeight).
 */
enum class HudTier
{
  P1080,
  P480,
  P272,
  P240
};


HudTier hudTierForWindowHeight(int windowHeight);
const char* hudTierSuffix(HudTier tier);


/** Sprite indices within an overlay HUD atlas */
namespace hud_sprite
{

enum Index
{
  ProjectileNormal = 0,
  ProjectileLaser,
  ProjectileRocket,
  ProjectileFlame,
  ProjectileRapid,
  AmmoBarLeft,
  AmmoBarMiddle,
  AmmoBarRight,
  BossBar,
  BossFillLeft,
  BossFillMiddle,
  BossFillRight,
  WeaponFirst, // 12..16
  WeaponLast = 16,
  PipEmpty,
  PipFull,
  InventoryPanel, // 19: slot grid + radar disc
  ItemFirst, // 20..24
  ItemLast = 24,
  LetterFirst, // 25..29: N U K E M
  LetterLast = 29,
  PipLowHealthFirst, // 30..34 pulsing
  PipLowHealthLast = 34,
  PipBackground, // 35
  PipGlow,
  RadarDot,
  RadarRing,
  ScorePanel, // 39: score + letter slots
  StatusPanel, // 40: pips + weapon + ammo

  Count = 41,
  CountWithExtra = 42 // The 272 tier has one additional sprite
};

} // namespace hud_sprite


struct HudSpriteRect
{
  int x;
  int y;
  int w;
  int h;
};


/** Layout constants, in reference pixels of the tier */
struct HudLayout
{
  int referenceHeight;
  int margin;

  // Radar
  int radarInset; // reduction of the radar extent used for dot placement
  int radarOffsetX; // radar origin relative to inventory panel

  // Top left status panel
  int pipX;
  int pipY;
  int pipStep;
  int weaponSlotX;
  int weaponSlotY;
  int weaponSlotW;
  int weaponSlotH;
  int ammoIconCenterX;
  int ammoIconCenterY;
  int ammoIconStep;
  int ammoBarX;
  int ammoBarY;
  int ammoBarWidth;

  // Top right panel
  int letterX;
  int letterY;
  int letterStep;
  int letterSize;
  int scoreInsetRight;
  int scoreCenterY;
  int scoreFontSize;
  int timerFontSize;

  // Bottom left panel
  int gridX;
  int gridY;
  int gridStepX;
  int gridStepY;
  int gridIconSize;
};


const HudLayout& hudLayout(HudTier tier);
const HudSpriteRect* hudAtlasRects(HudTier tier, int* pCount);


enum class HudSkinSource
{
  UserFiles,
  Generated
};


/** Texture atlas + rectangle table for the overlay HUD
 *
 * The atlas follows the layout described in re-notes/HUD_OVERLAY.md. It can
 * come from files supplied by the user (hud_skin/modern_hud_<tier>.png in the
 * game directory, either their own artwork or imported from a legally owned
 * cartridge dump).
 */
class HudSkin
{
public:
  static std::optional<HudSkin> tryLoad(
    renderer::Renderer* pRenderer,
    const std::filesystem::path& gamePath,
    HudTier tier);

  /** Our own default artwork, used when no custom skin is available */
  static HudSkin generateDefault(
    renderer::Renderer* pRenderer,
    const assets::ResourceLoader& resources);

  HudTier tier() const { return mTier; }
  HudSkinSource source() const { return mSource; }
  const renderer::Texture& atlas() const { return mAtlas; }
  const HudSpriteRect& rect(int index) const
  {
    return mRects[std::size_t(index)];
  }

private:
  using RectArray = std::array<HudSpriteRect, hud_sprite::CountWithExtra>;

  HudSkin(
    renderer::Texture atlas,
    HudTier tier,
    const RectArray& rects,
    HudSkinSource source)
    : mAtlas(std::move(atlas))
    , mTier(tier)
    , mRects(rects)
    , mSource(source)
  {
  }

  renderer::Texture mAtlas;
  HudTier mTier;
  RectArray mRects;
  HudSkinSource mSource;
};

} // namespace rigel::ui
