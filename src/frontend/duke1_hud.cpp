// Overlay HUD for Duke Nukem 1: artwork built from the game's own sprites and
// the state handed to the shared overlay renderer.


#include "duke1_mode.hpp"

#include "console_launcher_mode.hpp"

#include "assets/resource_loader.hpp"
#include "base/image.hpp"
#include "frontend/game_service_provider.hpp"
#include "frontend/user_profile.hpp"
#include "renderer/texture.hpp"
#include "renderer/upscaling.hpp"
#include "ui/console_menu.hpp"
#include "ui/hud_skin_generator.hpp"
#include "ui/overlay_hud.hpp"

#include <imgui.h>
#include <loguru.hpp>

#include <algorithm>
#include <cmath>
#include <string_view>

namespace rigel
{

ui::HudArt Duke1Mode::duke1HudArt() const
{
  // In the game Duke's hand covers the rear half of his gun. The barrel is cut
  // out of his standing sprite; the receiver, end cap and grip that his hand
  // hides are drawn in the same style and palette.
  constexpr int BARREL_X = 22;
  constexpr int BARREL_Y = 13;
  constexpr int BARREL_W = 10;
  constexpr int BARREL_H = 6;

  // clang-format off
  static const char* const REAR[] = {
    "..............",
    ".AAAAAAAAAAAAA",
    ".ADDAGGGGGGGGG",
    ".ADDAFFFFFFFFF",
    ".AFFAFFFEEFFFF",
    ".AFFAFFFEEFFFF",
    ".AAAADDDDDDDDD",
    ".....AFFFFA...",
    ".....ADDDDA...",
    ".....AAAAAA..."};
  // clang-format on
  constexpr int REAR_W = 14;
  constexpr int GRID_W = REAR_W + BARREL_W;
  constexpr int GRID_H = 10;

  const auto colorFor = [](const char c) {
    switch (c)
    {
      case 'A':
        return base::Color{0, 0, 0, 255};
      case 'D':
        return base::Color{85, 85, 85, 255};
      case 'F':
        return base::Color{170, 170, 170, 255};
      case 'G':
        return base::Color{255, 255, 255, 255};
      case 'E':
        return base::Color{170, 0, 170, 255};
      default:
        return base::Color{0, 0, 0, 0};
    }
  };

  const auto portrait = mpWorld->standingPortrait();
  data::PixelBuffer pixels(
    std::size_t(GRID_W) * GRID_H, base::Color{0, 0, 0, 0});
  for (int y = 0; y < GRID_H; ++y)
  {
    for (int x = 0; x < REAR_W; ++x)
    {
      pixels[std::size_t(y) * GRID_W + std::size_t(x)] = colorFor(REAR[y][x]);
    }
  }
  for (int y = 0; y < BARREL_H; ++y)
  {
    for (int x = 0; x < BARREL_W; ++x)
    {
      const auto k =
        (std::size_t(BARREL_Y + y) * 32 + std::size_t(BARREL_X + x)) * 4;
      pixels[std::size_t(1 + y) * GRID_W + std::size_t(REAR_W + x)] =
        base::Color{
          portrait[k], portrait[k + 1], portrait[k + 2], portrait[k + 3]};
    }
  }

  // The shot is drawn as a green wave; its visible part is centered in the
  // 32x16 projectile icon
  const auto& wave = mpWorld->shotIcon();
  int minX = 16, maxX = -1, minY = 16, maxY = -1;
  for (int y = 0; y < 16; ++y)
  {
    for (int x = 0; x < 16; ++x)
    {
      if (wave[(std::size_t(y) * 16 + std::size_t(x)) * 4 + 3] != 0)
      {
        minX = std::min(minX, x);
        maxX = std::max(maxX, x);
        minY = std::min(minY, y);
        maxY = std::max(maxY, y);
      }
    }
  }
  const auto offsetX = 16 - (minX + maxX + 1) / 2;
  const auto offsetY = 8 - (minY + maxY + 1) / 2;
  data::PixelBuffer shot(32 * 16, base::Color{0, 0, 0, 0});
  for (int y = 0; y < 16; ++y)
  {
    for (int x = 0; x < 16; ++x)
    {
      const auto k = (std::size_t(y) * 16 + std::size_t(x)) * 4;
      const auto dx = x + offsetX;
      const auto dy = y + offsetY;
      if (wave[k + 3] != 0 && dx >= 0 && dx < 32 && dy >= 0 && dy < 16)
      {
        shot[std::size_t(dy) * 32 + std::size_t(dx)] =
          base::Color{wave[k], wave[k + 1], wave[k + 2], wave[k + 3]};
      }
    }
  }

  ui::HudArt art;
  art.mGun = data::Image{std::move(pixels), GRID_W, GRID_H};
  art.mProjectiles[0] = data::Image{std::move(shot), 32, 16};
  return art;
}


void Duke1Mode::submitOverlayHud()
{
  const auto tier =
    ui::hudTierForWindowHeight(mContext.mpRenderer->windowSize().height);
  if (!mHudSkinTierTried || *mHudSkinTierTried != tier)
  {
    mHudSkin = ui::HudSkin::tryLoad(mContext.mpRenderer, gamePath(), tier);
    if (!mHudSkin && !mGeneratedHudSkin)
    {
      mGeneratedHudSkin =
        ui::HudSkin::generateDefault(mContext.mpRenderer, duke1HudArt());
    }
    mHudSkinTierTried = tier;
  }

  const auto* pSkin =
    mHudSkin ? &*mHudSkin : (mGeneratedHudSkin ? &*mGeneratedHudSkin : nullptr);
  if (!pSkin)
  {
    return;
  }

  const auto& player = mpWorld->player();
  ui::OverlayHudState state;
  state.health = player.health;
  state.score = mpWorld->score();
  state.ammo = player.capacity;
  state.maxAmmo = 4;
  state.showRadar = false;
  state.showRadarDisc = false;
  for (int i = 0; i < 8; ++i)
  {
    if (!mpWorld->hasKey(i))
    {
      continue;
    }
    if (!mKeyIcons[std::size_t(i)])
    {
      data::PixelBuffer pixels;
      const auto rgba = mpWorld->keyIcon(i);
      for (std::size_t k = 0; k + 3 < rgba.size(); k += 4)
      {
        pixels.emplace_back(rgba[k], rgba[k + 1], rgba[k + 2], rgba[k + 3]);
      }
      mKeyIcons[std::size_t(i)] = std::make_shared<renderer::Texture>(
        mContext.mpRenderer, data::Image{std::move(pixels), 16, 16});
    }
    state.customIcons.push_back(mKeyIcons[std::size_t(i)]);
  }
  state.timeSeconds = double(SDL_GetTicks()) / 1000.0;

  mContext.mpServiceProvider->setNativeOverlay(
    [pRenderer = mContext.mpRenderer, pSkin, state](const int alpha) {
      ui::drawOverlayHud(pRenderer, *pSkin, state, alpha);
    });
}

} // namespace rigel
