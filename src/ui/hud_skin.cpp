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

#include "hud_skin.hpp"

#include "assets/png_image.hpp"
#include "assets/resource_loader.hpp"
#include "base/spatial_types.hpp"
#include "loguru.hpp"
#include "ui/hud_skin_generator.hpp"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>


namespace rigel::ui
{

namespace
{

// Sprite rectangle tables, recovered from the original Evercade build
// (see re-notes/HUD_OVERLAY.md). Entries are {x, y, w, h} within the atlas.
constexpr HudSpriteRect RECTS_1080[] = {
  {1866, 146, 40, 40}, {1866, 188, 40, 40}, {1908, 197, 40, 40},
  {1950, 197, 40, 40}, {1992, 197, 40, 40}, {2040, 32, 2, 10},
  {2040, 20, 6, 10},   {2044, 32, 2, 10},   {263, 244, 1200, 56},
  {2042, 1, 4, 17},    {2034, 1, 6, 17},    {2034, 20, 4, 17},
  {1542, 204, 85, 38}, {1629, 204, 85, 38}, {1716, 204, 85, 38},
  {1803, 230, 85, 38}, {1890, 239, 85, 38}, {1542, 146, 32, 56},
  {1576, 146, 32, 56}, {263, 1, 421, 221},  {1943, 1, 80, 96},
  {1943, 99, 80, 96},  {1296, 146, 80, 96}, {1378, 146, 80, 96},
  {1460, 146, 80, 96}, {686, 1, 120, 144},  {808, 1, 120, 144},
  {930, 1, 120, 144},  {1052, 1, 120, 144}, {1174, 1, 120, 144},
  {1610, 146, 32, 56}, {1644, 146, 32, 56}, {1678, 146, 32, 56},
  {1712, 146, 32, 56}, {1746, 146, 32, 56}, {1780, 146, 32, 56},
  {1814, 146, 50, 50}, {1977, 239, 30, 30}, {1, 1, 260, 260},
  {1645, 1, 296, 143}, {1296, 1, 347, 143}};

constexpr HudSpriteRect RECTS_480[] = {
  {1805, 1, 20, 20},  {1827, 1, 20, 20},  {1849, 1, 20, 20},
  {1871, 1, 20, 20},  {1893, 1, 20, 20},  {1805, 23, 2, 6},
  {2039, 22, 6, 6},   {1809, 23, 2, 6},   {1319, 77, 460, 26},
  {2011, 22, 4, 16},  {2003, 22, 6, 16},  {2017, 22, 4, 16},
  {1915, 1, 42, 19},  {1959, 1, 42, 19},  {2003, 1, 42, 19},
  {1915, 22, 42, 19}, {1959, 22, 42, 19}, {1623, 1, 16, 30},
  {1641, 1, 16, 30},  {731, 1, 176, 100}, {909, 1, 80, 96},
  {991, 1, 80, 96},   {1073, 1, 80, 96},  {1155, 1, 80, 96},
  {1237, 1, 80, 96},  {1, 1, 120, 144},   {123, 1, 120, 144},
  {245, 1, 120, 144}, {367, 1, 120, 144}, {489, 1, 120, 144},
  {1659, 1, 16, 30},  {1677, 1, 16, 30},  {1695, 1, 16, 30},
  {1713, 1, 16, 30},  {1731, 1, 16, 30},  {1749, 1, 16, 30},
  {1781, 1, 22, 22},  {2023, 22, 14, 14}, {611, 1, 118, 118},
  {1483, 1, 138, 74}, {1319, 1, 162, 74}};

constexpr HudSpriteRect RECTS_272[] = {
  {1912, 1, 12, 12},  {1926, 1, 12, 12},  {1940, 1, 12, 12},
  {1954, 1, 12, 12},  {1968, 1, 12, 12},  {2034, 1, 2, 3},
  {2026, 1, 6, 3},    {2038, 1, 2, 3},    {1430, 1, 214, 20},
  {2004, 1, 4, 12},   {1996, 1, 6, 12},   {2010, 1, 4, 12},
  {1742, 1, 32, 14},  {1776, 1, 32, 14},  {1810, 1, 32, 14},
  {1844, 1, 32, 14},  {1878, 1, 32, 14},  {1646, 1, 10, 18},
  {1658, 1, 10, 18},  {1089, 1, 98, 56},  {611, 1, 80, 96},
  {693, 1, 80, 96},   {775, 1, 80, 96},   {857, 1, 80, 96},
  {939, 1, 80, 96},   {1, 1, 120, 144},   {123, 1, 120, 144},
  {245, 1, 120, 144}, {367, 1, 120, 144}, {489, 1, 120, 144},
  {1670, 1, 10, 18},  {1682, 1, 10, 18},  {1694, 1, 10, 18},
  {1706, 1, 10, 18},  {1718, 1, 10, 18},  {1730, 1, 10, 18},
  {1982, 1, 12, 12},  {2016, 1, 8, 8},    {1021, 1, 66, 66},
  {1340, 1, 88, 47},  {1234, 1, 104, 47}, {1189, 1, 43, 56}};


// Values are in reference pixels. Derived from the original HUD code.
constexpr HudLayout LAYOUT_1080 = {
  1080, 27,  16, 200, 50, 10, 35, 51, 76, 106, 42, 244, 97, 54, 50,
  123,  274, 27, 89,  46, 28, 50, 48, 36, 24,  53, 14,  70, 67, 54};

constexpr HudLayout LAYOUT_480 = {480, 5,  5,  76, 10, 4,  18,  10, 38, 52,
                                  22,  64, 49, 29, 10, 64, 142, 12, 46, 24,
                                  18,  10, 26, 16, 16, 11, 8,   30, 30, 24};

constexpr HudLayout LAYOUT_272 = {272, 3,  3,  42, 5,  3,  12, 5,  24, 34,
                                  14,  41, 31, 19, 5,  41, 94, 6,  29, 16,
                                  12,  5,  16, 12, 12, 6,  3,  17, 18, 14};

} // namespace


HudTier hudTierForWindowHeight(const int windowHeight)
{
  // Development aid: force a specific tier (1080, 480, 272, 240)
  if (const auto* pForced = std::getenv("RIGEL_HUD_TIER"))
  {
    switch (std::atoi(pForced))
    {
      case 1080:
        return HudTier::P1080;
      case 480:
        return HudTier::P480;
      case 272:
        return HudTier::P272;
      case 240:
        return HudTier::P240;
    }
  }

  if (windowHeight >= 900)
  {
    return HudTier::P1080;
  }
  if (windowHeight >= 400)
  {
    return HudTier::P480;
  }
  if (windowHeight >= 256)
  {
    return HudTier::P272;
  }
  return HudTier::P240;
}


const char* hudTierSuffix(const HudTier tier)
{
  switch (tier)
  {
    case HudTier::P1080:
      return "1080";
    case HudTier::P480:
      return "480";
    case HudTier::P272:
      return "272";
    case HudTier::P240:
      return "272"; // shares the 272 artwork
  }
  return "1080";
}


const HudLayout& hudLayout(const HudTier tier)
{
  switch (tier)
  {
    case HudTier::P1080:
      return LAYOUT_1080;
    case HudTier::P480:
      return LAYOUT_480;
    case HudTier::P272:
    case HudTier::P240:
      return LAYOUT_272;
  }
  return LAYOUT_1080;
}


const HudSpriteRect* hudAtlasRects(const HudTier tier, int* pCount)
{
  switch (tier)
  {
    case HudTier::P1080:
      *pCount = int(std::size(RECTS_1080));
      return RECTS_1080;
    case HudTier::P480:
      *pCount = int(std::size(RECTS_480));
      return RECTS_480;
    case HudTier::P272:
    case HudTier::P240:
      *pCount = int(std::size(RECTS_272));
      return RECTS_272;
  }
  *pCount = 0;
  return nullptr;
}


std::optional<HudSkin> HudSkin::tryLoad(
  renderer::Renderer* pRenderer,
  const std::filesystem::path& gamePath,
  const HudTier tier)
{
  const auto fileName =
    std::string{"modern_hud_"} + hudTierSuffix(tier) + ".png";

  // Search order: RIGEL_HUD_SKIN_DIR environment variable (for development),
  // then the hud_skin folder inside the game directory.
  std::vector<std::filesystem::path> searchDirs;
  if (const auto* pEnvDir = std::getenv("RIGEL_HUD_SKIN_DIR"))
  {
    searchDirs.emplace_back(pEnvDir);
  }
  searchDirs.push_back(gamePath / "hud_skin");

  std::filesystem::path path;
  std::error_code ec;
  for (const auto& dir : searchDirs)
  {
    if (std::filesystem::exists(dir / fileName, ec))
    {
      path = dir / fileName;
      break;
    }
  }

  if (path.empty())
  {
    LOG_F(
      INFO,
      "HUD skin: no custom skin %s in %s",
      fileName.c_str(),
      searchDirs.back().u8string().c_str());
    return std::nullopt;
  }

  auto oImage = assets::loadPng(path);
  if (!oImage)
  {
    LOG_F(WARNING, "HUD skin: failed to load %s", path.u8string().c_str());
    return std::nullopt;
  }

  int numRects = 0;
  const auto* pRects = hudAtlasRects(tier, &numRects);

  // Validate that the atlas has the size we expect, otherwise the rectangle
  // table would reference nonexistent areas.
  for (auto i = 0; i < numRects; ++i)
  {
    const auto& r = pRects[i];
    if (r.x + r.w > int(oImage->width()) || r.y + r.h > int(oImage->height()))
    {
      LOG_F(
        WARNING,
        "HUD skin: %s is too small for the expected sprite layout",
        path.u8string().c_str());
      return std::nullopt;
    }
  }

  LOG_F(INFO, "HUD skin: using %s", path.u8string().c_str());

  renderer::Texture atlas{pRenderer, *oImage};
  pRenderer->setFilteringEnabled(atlas.data(), true);
  RectArray rects{};
  std::copy(pRects, pRects + numRects, rects.begin());
  return HudSkin{std::move(atlas), tier, rects, HudSkinSource::UserFiles};
}


HudSkin
  HudSkin::generateDefault(renderer::Renderer* pRenderer, const HudArt& art)
{
  LOG_F(INFO, "HUD skin: using generated default artwork");

  GeneratedHudAtlas generated = generateDefaultHudAtlas(art);
  auto& image = generated.mImage;

#ifdef RIGEL_DEV_SCREENSHOT
  if (const auto* pDump = std::getenv("RIGEL_DUMP_HUD_ATLAS"))
  {
    assets::savePng(pDump, image);
  }
#endif

  renderer::Texture atlas{pRenderer, image};
  pRenderer->setFilteringEnabled(atlas.data(), true);

  return HudSkin{
    std::move(atlas),
    HudTier::P1080,
    generated.mRects,
    HudSkinSource::Generated};
}

} // namespace rigel::ui
