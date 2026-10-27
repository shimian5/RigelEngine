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

#include "overlay_hud.hpp"

#include "ui/imgui_integration.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>


namespace rigel::ui
{

namespace
{

constexpr int radarMaskSize(const HudTier tier)
{
  switch (tier)
  {
    case HudTier::P1080:
      return 220;
    case HudTier::P480:
      return 100;
    default:
      return 56;
  }
}


constexpr int LOW_HEALTH_ANIMATION[] = {30, 31, 32, 33, 34, 33, 32, 31, 30};


float easeOutCubic(const float t)
{
  const auto inv = 1.0f - t;
  return 1.0f - inv * inv * inv;
}


class Painter
{
public:
  Painter(
    renderer::Renderer* pRenderer,
    const HudSkin& skin,
    const int alpha)
    : mpRenderer(pRenderer)
    , mSkin(skin)
    , mLayout(hudLayout(skin.tier()))
    , mAlpha(alpha)
  {
    mScale = float(pRenderer->windowSize().height) / mLayout.referenceHeight;
  }

  int s(const int value) const { return int(std::lround(value * mScale)); }

  int sf(const float value) const { return int(std::lround(value * mScale)); }

  const HudLayout& layout() const { return mLayout; }

  float scale() const { return mScale; }

  /** Size of a sprite on screen */
  base::Size sizeOf(const int index) const
  {
    const auto& r = mSkin.rect(index);
    return {s(r.w), s(r.h)};
  }

  void draw(const int index, const int x, const int y) const
  {
    drawScaled(index, x, y, sizeOf(index));
  }

  void drawScaled(
    const int index,
    const int x,
    const int y,
    const base::Size& size) const
  {
    const auto& r = mSkin.rect(index);
    mSkin.atlas().render(
      base::Rect<int>{{r.x, r.y}, {r.w, r.h}},
      base::Rect<int>{{x, y}, size});
  }

  void drawCentered(const int index, const int cx, const int cy) const
  {
    const auto size = sizeOf(index);
    draw(index, cx - size.width / 2, cy - size.height / 2);
  }

  void setAlpha(const int alpha) const
  {
    mpRenderer->setColorModulation(
      {255, 255, 255, std::uint8_t(std::clamp(alpha, 0, 255))});
  }

  int baseAlpha() const { return mAlpha; }

private:
  renderer::Renderer* mpRenderer;
  const HudSkin& mSkin;
  const HudLayout& mLayout;
  int mAlpha;
  float mScale = 1.0f;
};


/** Draws a bar from 3 sprites: fixed width caps and a stretched middle. If
 * height is not given, the height of the left cap is used. */
void drawBar(
  const Painter& p,
  const int left,
  const int middle,
  const int right,
  const int x,
  const int y,
  const int totalWidth,
  const std::optional<int> height = std::nullopt)
{
  const auto leftSize = p.sizeOf(left);
  const auto rightSize = p.sizeOf(right);
  const auto h = height ? *height : leftSize.height;
  if (totalWidth <= 0)
  {
    return;
  }

  if (totalWidth <= leftSize.width + rightSize.width)
  {
    p.drawScaled(middle, x, y, {totalWidth, h});
    return;
  }

  p.drawScaled(left, x, y, {leftSize.width, h});
  p.drawScaled(
    middle,
    x + leftSize.width,
    y,
    {totalWidth - leftSize.width - rightSize.width, h});
  p.drawScaled(right, x + totalWidth - rightSize.width, y, {rightSize.width, h});
}


void drawScoreText(
  const Painter& p,
  const std::string& text,
  const int fontSizeRef,
  const int rightX,
  const int centerY,
  const int alpha)
{
  auto* pFont = static_cast<ImFont*>(imgui_integration::hudFont());
  if (!pFont)
  {
    return;
  }

  const auto fontSize = float(p.s(fontSizeRef));
  const auto textSize = pFont->CalcTextSizeA(fontSize, 10000.0f, 0.0f, text.c_str());
  auto* pDrawList = ImGui::GetBackgroundDrawList();
  const auto pos = ImVec2{
    float(rightX) - textSize.x, float(centerY) - textSize.y / 2.0f};

  // Soft shadow for legibility over bright backgrounds
  pDrawList->AddText(
    pFont, fontSize, {pos.x + 1.5f, pos.y + 1.5f}, IM_COL32(0, 0, 0, alpha * 160 / 255), text.c_str());
  pDrawList->AddText(
    pFont, fontSize, pos, IM_COL32(255, 255, 255, alpha), text.c_str());
}

} // namespace


void drawOverlayHud(
  renderer::Renderer* pRenderer,
  const HudSkin& skin,
  const OverlayHudState& state,
  const int alpha)
{
  using namespace hud_sprite;

  auto guard = renderer::saveState(pRenderer);
  pRenderer->setGlobalScale({1.0f, 1.0f});
  pRenderer->setGlobalTranslation({0, 0});
  pRenderer->setClipRect({});

  Painter p{pRenderer, skin, alpha};
  p.setAlpha(alpha);

  const auto& L = p.layout();
  const auto windowSize = pRenderer->windowSize();
  const auto margin = p.s(L.margin);

  // ------------------------------------------------------------------
  // Top left: status panel with health pips, weapon and ammo
  // ------------------------------------------------------------------
  p.draw(StatusPanel, 0, margin);

  {
    const auto pipX = p.s(L.pipX);
    const auto pipY = margin + p.s(L.pipY);
    const auto pipStep = p.s(L.pipStep);
    constexpr auto NUM_PIPS = 8;

    if (state.health < 2)
    {
      for (auto i = 0; i < NUM_PIPS; ++i)
      {
        const auto x = pipX + i * pipStep;
        p.draw(PipBackground, x, pipY);
        const auto index = LOW_HEALTH_ANIMATION[(i + state.animationFrame) % 9];
        p.draw(index, x, pipY);
      }
    }
    else
    {
      for (auto i = 0; i < NUM_PIPS; ++i)
      {
        p.draw(i < state.health - 1 ? PipFull : PipEmpty, pipX + i * pipStep, pipY);
      }
    }
  }

  {
    // Weapon icon, centered in its slot
    const auto weaponIndex =
      WeaponFirst + std::min(int(state.weapon), WeaponLast - WeaponFirst);
    const auto slotCenterX = p.s(L.weaponSlotX) + p.s(L.weaponSlotW) / 2;
    const auto slotCenterY = margin + p.s(L.weaponSlotY) + p.s(L.weaponSlotH) / 2;
    p.drawCentered(weaponIndex, slotCenterX, slotCenterY);

    // Projectile icon(s) next to it. Rapid fire and the flame thrower show
    // three of them.
    const auto projectileIndex =
      ProjectileNormal + std::min(int(state.weapon), 4);
    const auto cx = p.s(L.ammoIconCenterX);
    const auto cy = margin + p.s(L.ammoIconCenterY);
    if (state.rapidFire || state.weapon == data::WeaponType::FlameThrower)
    {
      const auto step = p.s(L.ammoIconStep);
      for (auto i = -1; i <= 1; ++i)
      {
        p.drawCentered(projectileIndex, cx + i * step, cy);
      }
    }
    else
    {
      p.drawCentered(projectileIndex, cx, cy);
    }

    // Ammo bar
    const auto ratio =
      std::clamp(float(state.ammo) / float(std::max(1, state.maxAmmo)), 0.0f, 1.0f);
    drawBar(
      p,
      AmmoBarLeft,
      AmmoBarMiddle,
      AmmoBarRight,
      p.s(L.ammoBarX),
      margin + p.s(L.ammoBarY),
      int(std::lround(p.s(L.ammoBarWidth) * ratio)));
  }

  // ------------------------------------------------------------------
  // Top right: score panel with collected letters
  // ------------------------------------------------------------------
  {
    const auto panelSize = p.sizeOf(ScorePanel);
    const auto panelX = windowSize.width - panelSize.width;
    p.draw(ScorePanel, panelX, margin);

    for (const auto letter : state.letters)
    {
      const auto i = int(letter);
      p.drawScaled(
        LetterFirst + i,
        panelX + p.s(L.letterX) + i * p.s(L.letterStep),
        margin + p.s(L.letterY),
        {p.s(L.letterSize), p.s(L.letterSize)});
    }

    drawScoreText(
      p,
      std::to_string(state.score),
      L.scoreFontSize,
      windowSize.width - p.s(L.scoreInsetRight),
      margin + p.s(L.scoreCenterY),
      alpha);

    // Level timer below the panel
    if (state.showTimer)
    {
      const auto total = std::max(0.0, state.levelSeconds);
      char buffer[32];
      std::snprintf(
        buffer,
        sizeof(buffer),
        "%02d:%02d.%d",
        int(total) / 60,
        int(total) % 60,
        int(total * 10.0) % 10);
      drawScoreText(
        p,
        buffer,
        L.timerFontSize,
        windowSize.width - p.s(L.scoreInsetRight),
        margin + panelSize.height + p.s(L.timerFontSize) ,
        alpha);
    }
  }

  // ------------------------------------------------------------------
  // Boss health bar, bottom center
  // ------------------------------------------------------------------
  if (state.bossHealthRatio)
  {
    const auto& rect = skin.rect(BossBar);

    // Keep clear of the bottom left panel (inventory and radar)
    const auto leftLimit =
      p.sizeOf(InventoryPanel).width + margin;
    const auto available = windowSize.width - leftLimit - margin;
    const auto sc = std::min(p.scale(), float(available) / float(rect.w));
    const auto barW = int(std::lround(rect.w * sc));
    const auto barH = int(std::lround(rect.h * sc));
    const auto barX = leftLimit + (available - barW) / 2;
    const auto barY = windowSize.height - barH - margin;
    p.drawScaled(BossBar, barX, barY, {barW, barH});

    // The dark track inside the bar, as fractions of the bar's size
    const auto trackX = barX + int(std::lround(barW * 94.0 / 1200.0));
    const auto trackW = int(std::lround(barW * 1083.0 / 1200.0));
    const auto trackY = barY + int(std::lround(barH * 18.0 / 56.0));
    const auto trackH = int(std::lround(barH * 20.0 / 56.0));
    const auto ratio = std::clamp(*state.bossHealthRatio, 0.0f, 1.0f);
    drawBar(
      p,
      BossFillLeft,
      BossFillMiddle,
      BossFillRight,
      trackX,
      trackY,
      int(std::lround(trackW * ratio)),
      trackH);
  }

  // ------------------------------------------------------------------
  // Bottom left: inventory grid and radar
  // ------------------------------------------------------------------
  {
    const auto panelSize = p.sizeOf(InventoryPanel);
    const auto panelX = 0;
    const auto panelY = windowSize.height - panelSize.height - margin;
    if (state.showRadarDisc)
    {
      p.draw(InventoryPanel, panelX, panelY);
    }
    else
    {
      // Only the part with the slot grid, left of the radar disc
      const auto& r = skin.rect(InventoryPanel);
      const auto cropW = std::min(r.w, p.layout().radarOffsetX);
      skin.atlas().render(
        base::Rect<int>{{r.x, r.y}, {cropW, r.h}},
        base::Rect<int>{{panelX, panelY}, {p.s(cropW), panelSize.height}});
    }

    // 3 rows x 2 columns of items
    const auto iconSize = p.s(L.gridIconSize);
    for (auto cell = 0; cell < 6; ++cell)
    {
      const auto row = cell / 2;
      const auto col = cell % 2;
      const auto x = panelX + p.s(L.gridX) + col * p.s(L.gridStepX);
      const auto y = panelY + p.s(L.gridY) + row * p.s(L.gridStepY);
      const auto itemCount = static_cast<int>(state.inventory.size());
      if (cell < itemCount)
      {
        p.drawScaled(
          ItemFirst + int(state.inventory[std::size_t(cell)]),
          x,
          y,
          {iconSize, iconSize});
      }
      else if (cell - itemCount < static_cast<int>(state.customIcons.size()))
      {
        const auto& pIcon = state.customIcons[std::size_t(cell - itemCount)];
        pIcon->render(
          base::Rect<int>{{0, 0}, {pIcon->width(), pIcon->height()}},
          base::Rect<int>{{x, y}, {iconSize, iconSize}});
      }
    }

    if (state.showRadar)
    {
      const auto maskSize = p.s(radarMaskSize(skin.tier()));
      const auto centerX = panelX + p.s(L.radarOffsetX) + maskSize / 2;
      const auto centerY = panelY + maskSize / 2;

      // Expanding "sonar" ring
      const auto cycle = std::fmod(state.timeSeconds * 0.25, 1.0);
      const auto t = easeOutCubic(float(std::clamp(cycle * 2.0, 0.0, 1.0)));
      const auto ringSize = int(maskSize * (0.25f + 0.7f * t));
      const auto pulse = std::abs(std::cos(state.timeSeconds * 2.0));
      p.setAlpha(int(alpha * (1.0f - t) * (0.5 + 0.5 * pulse)));
      p.drawScaled(
        RadarRing, centerX - ringSize / 2, centerY - ringSize / 2, {ringSize, ringSize});
      p.setAlpha(alpha);

      // Dots
      const auto unit = float(maskSize - p.s(L.radarInset)) / 32.0f;
      const auto radius = float(maskSize) / 2.0f;
      for (const auto& dot : state.radarDots)
      {
        const auto dx = float(dot.x) * unit;
        const auto dy = float(dot.y) * unit;
        if (dx * dx + dy * dy > radius * radius)
        {
          continue;
        }
        p.drawCentered(RadarDot, centerX + int(dx), centerY + int(dy));
      }
    }
  }
}

} // namespace rigel::ui
