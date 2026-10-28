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
#include "data/player_model.hpp"
#include "ui/hud_skin.hpp"

#include "renderer/texture.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>


namespace rigel::ui
{

/** Everything the overlay HUD needs to know for drawing one frame
 *
 * This is a plain value type, so it can be captured and drawn later,
 * after the low-res game image has been upscaled to the screen.
 */
struct OverlayHudState
{
  int health = 0;
  int score = 0;
  int ammo = 0;
  int maxAmmo = 1;
  data::WeaponType weapon = data::WeaponType::Normal;
  bool rapidFire = false;
  std::vector<data::InventoryItemType> inventory;

  // Extra icons for games with their own items (Duke Nukem 1 keys), drawn after
  // the regular inventory
  std::vector<std::shared_ptr<renderer::Texture>> customIcons;
  std::vector<data::CollectableLetterType> letters;

  // Positions relative to the player in tiles, each within [-16, 16)
  std::vector<base::Vec2> radarDots;
  bool showRadar = true;

  // The inventory panel artwork includes a radar disc. Games without radar
  // (Duke Nukem 1) draw just the slot grid.
  bool showRadarDisc = true;

  // Set while a boss is active
  std::optional<float> bossHealthRatio;

  bool showTimer = false;
  double levelSeconds = 0.0;

  // The panels at the top move down by this fraction (0-1) of the distance
  // between their resting position and topRowBottomPx, so that they don't
  // cover the message line while one is showing
  float topSlide = 0.0f;
  int topRowBottomPx = 0;

  std::uint32_t animationFrame = 0;
  double timeSeconds = 0.0;
};


/** Draw the overlay HUD at native window resolution
 *
 * alpha (0-255) is used for fading along with the rest of the screen.
 */
void drawOverlayHud(
  renderer::Renderer* pRenderer,
  const HudSkin& skin,
  const OverlayHudState& state,
  int alpha);

} // namespace rigel::ui
