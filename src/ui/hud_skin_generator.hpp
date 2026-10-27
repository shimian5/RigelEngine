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

#include "base/image.hpp"
#include "ui/hud_skin.hpp"

#include <array>

namespace rigel::assets
{
class ResourceLoader;
}


namespace rigel::ui
{

/** Generates a default overlay HUD atlas (1080 tier layout)
 *
 * This is used when the user hasn't supplied any custom skin. The artwork is
 * drawn procedurally, with item and weapon icons taken from the game's own
 * sprites. The result uses the same sprite layout as skins in the format
 * described in re-notes/HUD_OVERLAY.md, so it can be used interchangeably.
 */
struct GeneratedHudAtlas
{
  data::Image mImage;
  std::array<HudSpriteRect, hud_sprite::CountWithExtra> mRects;
};

GeneratedHudAtlas
  generateDefaultHudAtlas(const assets::ResourceLoader& resources);

} // namespace rigel::ui
