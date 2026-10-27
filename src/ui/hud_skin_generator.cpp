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

#include "hud_skin_generator.hpp"

#include "assets/resource_loader.hpp"
#include "data/actor_ids.hpp"
#include "ui/hud_skin.hpp"

#include <algorithm>
#include <cmath>
#include <string>


namespace rigel::ui
{

namespace
{

using base::Color;
using namespace hud_sprite;

constexpr auto ATLAS_WIDTH = 2048;
constexpr auto ATLAS_HEIGHT = 420;

constexpr auto ORANGE = Color{255, 153, 0, 255};
constexpr auto BRACKET = Color{230, 230, 230, 235};


class Canvas
{
public:
  Canvas(const int width, const int height)
    : mWidth(width)
    , mHeight(height)
    , mPixels(std::size_t(width) * height, Color{0, 0, 0, 0})
  {
  }

  void blend(const int x, const int y, const Color& src)
  {
    if (x < 0 || y < 0 || x >= mWidth || y >= mHeight || src.a == 0)
    {
      return;
    }

    auto& dst = mPixels[std::size_t(y) * mWidth + x];
    const auto sa = src.a / 255.0f;
    const auto da = dst.a / 255.0f;
    const auto outA = sa + da * (1.0f - sa);
    if (outA <= 0.0f)
    {
      return;
    }

    auto mix = [&](const std::uint8_t s, const std::uint8_t d) {
      return std::uint8_t(
        std::clamp((s * sa + d * da * (1.0f - sa)) / outA, 0.0f, 255.0f));
    };

    dst = Color{
      mix(src.r, dst.r),
      mix(src.g, dst.g),
      mix(src.b, dst.b),
      std::uint8_t(std::clamp(outA * 255.0f, 0.0f, 255.0f))};
  }

  void fillRect(const HudSpriteRect& r, const Color& color)
  {
    for (auto y = r.y; y < r.y + r.h; ++y)
    {
      for (auto x = r.x; x < r.x + r.w; ++x)
      {
        blend(x, y, color);
      }
    }
  }

  void fillRect(
    const int x,
    const int y,
    const int w,
    const int h,
    const Color& color)
  {
    fillRect(HudSpriteRect{x, y, w, h}, color);
  }

  void rectOutline(const HudSpriteRect& r, const Color& color, const int t)
  {
    fillRect(r.x, r.y, r.w, t, color);
    fillRect(r.x, r.y + r.h - t, r.w, t, color);
    fillRect(r.x, r.y, t, r.h, color);
    fillRect(r.x + r.w - t, r.y, t, r.h, color);
  }

  /** Horizontal alpha gradient from alphaLeft to alphaRight */
  void gradientPanel(
    const HudSpriteRect& r,
    const Color& color,
    const int alphaLeft,
    const int alphaRight)
  {
    for (auto x = 0; x < r.w; ++x)
    {
      const auto t = float(x) / std::max(1, r.w - 1);
      const auto a = int(alphaLeft + (alphaRight - alphaLeft) * t);
      for (auto y = 0; y < r.h; ++y)
      {
        blend(
          r.x + x, r.y + y, Color{color.r, color.g, color.b, std::uint8_t(a)});
      }
    }
  }

  void brackets(const HudSpriteRect& r, const int arm, const int thickness)
  {
    for (auto i = 0; i < thickness; ++i)
    {
      // top-left
      fillRect(r.x, r.y + i, arm, 1, BRACKET);
      fillRect(r.x + i, r.y, 1, arm, BRACKET);
      // top-right
      fillRect(r.x + r.w - arm, r.y + i, arm, 1, BRACKET);
      fillRect(r.x + r.w - 1 - i, r.y, 1, arm, BRACKET);
      // bottom-left
      fillRect(r.x, r.y + r.h - 1 - i, arm, 1, BRACKET);
      fillRect(r.x + i, r.y + r.h - arm, 1, arm, BRACKET);
      // bottom-right
      fillRect(r.x + r.w - arm, r.y + r.h - 1 - i, arm, 1, BRACKET);
      fillRect(r.x + r.w - 1 - i, r.y + r.h - arm, 1, arm, BRACKET);
    }
  }

  void slot(const HudSpriteRect& r, const int arm = 10)
  {
    fillRect(r, Color{0, 0, 0, 80});
    brackets(r, arm, 2);
  }

  /** Radial shape; alpha function takes normalized distance in [0, 1] */
  template <typename F>
  void radial(const HudSpriteRect& r, const Color& color, F&& alphaAt)
  {
    const auto cx = r.x + r.w / 2.0f - 0.5f;
    const auto cy = r.y + r.h / 2.0f - 0.5f;
    const auto radius = std::min(r.w, r.h) / 2.0f;
    for (auto y = 0; y < r.h; ++y)
    {
      for (auto x = 0; x < r.w; ++x)
      {
        const auto dx = r.x + x - cx;
        const auto dy = r.y + y - cy;
        const auto d = std::sqrt(dx * dx + dy * dy) / radius;
        if (d <= 1.0f)
        {
          const auto a = std::clamp(alphaAt(d), 0.0f, 1.0f);
          blend(
            r.x + x,
            r.y + y,
            Color{color.r, color.g, color.b, std::uint8_t(a * color.a)});
        }
      }
    }
  }

  /** Draw an image scaled (nearest neighbor) to fit into r, centered */
  void blitFit(const data::Image& image, const HudSpriteRect& r)
  {
    if (image.width() == 0 || image.height() == 0)
    {
      return;
    }

    const auto scale = std::max(
      1, std::min(r.w / int(image.width()), r.h / int(image.height())));
    const auto w = int(image.width()) * scale;
    const auto h = int(image.height()) * scale;
    const auto ox = r.x + (r.w - w) / 2;
    const auto oy = r.y + (r.h - h) / 2;
    for (auto y = 0; y < h; ++y)
    {
      for (auto x = 0; x < w; ++x)
      {
        blend(
          ox + x,
          oy + y,
          image.pixelData()[(y / scale) * image.width() + (x / scale)]);
      }
    }
  }

  /** Draw part of an image at an integer scale, top-left at (dstX, dstY) */
  void blitCropScaled(
    const data::Image& image,
    const int srcX,
    const int srcY,
    const int srcW,
    const int srcH,
    const int dstX,
    const int dstY,
    const int scale,
    const bool skipWarmColors = false,
    const bool skipBackground = false)
  {
    for (auto y = 0; y < srcH * scale; ++y)
    {
      for (auto x = 0; x < srcW * scale; ++x)
      {
        const auto sx = srcX + x / scale;
        const auto sy = srcY + y / scale;
        if (sx >= int(image.width()) || sy >= int(image.height()))
        {
          continue;
        }
        const auto pixel =
          image.pixelData()[std::size_t(sy) * image.width() + sx];
        // Optionally drop skin/brown tones, e.g. to cut Duke's hand off a gun
        if (skipWarmColors && pixel.r > pixel.g + 30 && pixel.r > pixel.b + 30)
        {
          continue;
        }
        // Treat the color of the crop's top-left pixel as the background
        if (skipBackground)
        {
          const auto bg =
            image.pixelData()[std::size_t(srcY) * image.width() + srcX];
          if (pixel.r == bg.r && pixel.g == bg.g && pixel.b == bg.b)
          {
            continue;
          }
        }
        blend(dstX + x, dstY + y, pixel);
      }
    }
  }

  data::Image toImage() &&
  {
    return data::Image{
      std::move(mPixels), std::size_t(mWidth), std::size_t(mHeight)};
  }

private:
  int mWidth;
  int mHeight;
  data::PixelBuffer mPixels;
};


// 5x7 glyphs for the letters we need
const char* glyphFor(const char c)
{
  switch (c)
  {
    case 'N':
      return "X...X"
             "XX..X"
             "X.X.X"
             "X..XX"
             "X...X"
             "X...X"
             "X...X";
    case 'U':
      return "X...X"
             "X...X"
             "X...X"
             "X...X"
             "X...X"
             "X...X"
             ".XXX.";
    case 'K':
      return "X...X"
             "X..X."
             "X.X.."
             "XX..."
             "X.X.."
             "X..X."
             "X...X";
    case 'E':
      return "XXXXX"
             "X...."
             "X...."
             "XXXX."
             "X...."
             "X...."
             "XXXXX";
    case 'M':
      return "X...X"
             "XX.XX"
             "X.X.X"
             "X.X.X"
             "X...X"
             "X...X"
             "X...X";
    case 'S':
      return ".XXXX"
             "X...."
             "X...."
             ".XXX."
             "....X"
             "....X"
             "XXXX.";
    case 'C':
      return ".XXX."
             "X...X"
             "X...."
             "X...."
             "X...."
             "X...X"
             ".XXX.";
    case 'O':
      return ".XXX."
             "X...X"
             "X...X"
             "X...X"
             "X...X"
             "X...X"
             ".XXX.";
    case 'B':
      return "XXXX."
             "X...X"
             "X...X"
             "XXXX."
             "X...X"
             "X...X"
             "XXXX.";
    case 'R':
      return "XXXX."
             "X...X"
             "X...X"
             "XXXX."
             "X.X.."
             "X..X."
             "X...X";
    default:
      return ".....................................";
  }
}


void drawGlyph(
  Canvas& canvas,
  const char c,
  const int x,
  const int y,
  const int scale,
  const Color& color)
{
  const auto* pGlyph = glyphFor(c);
  for (auto gy = 0; gy < 7; ++gy)
  {
    for (auto gx = 0; gx < 5; ++gx)
    {
      if (pGlyph[gy * 5 + gx] == 'X')
      {
        canvas.fillRect(x + gx * scale, y + gy * scale, scale, scale, color);
      }
    }
  }
}


void drawText(
  Canvas& canvas,
  const std::string& text,
  const int x,
  const int y,
  const int scale,
  const Color& color)
{
  for (auto i = 0u; i < text.size(); ++i)
  {
    drawGlyph(canvas, text[i], x + int(i) * 6 * scale, y, scale, color);
  }
}


void drawBigLetter(
  Canvas& canvas,
  const char c,
  const HudSpriteRect& r,
  const Color& fill)
{
  constexpr auto scale = 17;
  const auto w = 5 * scale;
  const auto h = 7 * scale;
  const auto ox = r.x + (r.w - w) / 2;
  const auto oy = r.y + (r.h - h) / 2;
  // outline, then fill
  for (auto dy = -4; dy <= 4; ++dy)
  {
    for (auto dx = -4; dx <= 4; ++dx)
    {
      if (dx * dx + dy * dy <= 16)
      {
        drawGlyph(canvas, c, ox + dx, oy + dy, scale, Color{0, 0, 0, 255});
      }
    }
  }
  drawGlyph(canvas, c, ox, oy, scale, fill);
  // simple highlight on the upper half
  drawGlyph(canvas, c, ox, oy, scale, Color{255, 255, 255, 45});
}


data::Image cropTiles(
  const data::Image& sheet,
  const int tileX,
  const int tileY,
  const int tilesW,
  const int tilesH)
{
  data::PixelBuffer pixels;
  const auto w = tilesW * 8;
  const auto h = tilesH * 8;
  pixels.reserve(std::size_t(w) * h);
  for (auto y = 0; y < h; ++y)
  {
    for (auto x = 0; x < w; ++x)
    {
      pixels.push_back(
        sheet.pixelData()
          [std::size_t(tileY * 8 + y) * sheet.width() + tileX * 8 + x]);
    }
  }
  return data::Image{std::move(pixels), std::size_t(w), std::size_t(h)};
}

} // namespace


HudArt HudArt::fromDuke2(const assets::ResourceLoader& resources)
{
  using data::ActorID;

  HudArt art;

  const ActorID items[] = {
    ActorID::White_box_circuit_card,
    ActorID::White_box_blue_key,
    ActorID::Rapid_fire_icon,
    ActorID::Special_hint_globe_icon,
    ActorID::Cloaking_device_icon};
  for (auto i = 0; i < 5; ++i)
  {
    const auto actor = resources.loadActor(items[i]);
    if (!actor.mFrames.empty())
    {
      art.mItemIcons[std::size_t(i)] = actor.mFrames.front().mFrameImage;
    }
  }

  const auto duke = resources.loadActor(ActorID::Duke_RIGHT);
  if (!duke.mFrames.empty())
  {
    art.mPlayerSprite = duke.mFrames.front().mFrameImage;
  }

  // The status bar's weapon icons are the projectiles
  const auto sheet = resources.loadUiSpriteSheet();
  for (auto i = 0; i < 4; ++i)
  {
    const auto tileIndex = i * 4 + 4 + 5 * 40;
    art.mProjectiles[std::size_t(i)] =
      cropTiles(sheet, tileIndex % 40, tileIndex / 40, 4, 2);
  }

  return art;
}


GeneratedHudAtlas generateDefaultHudAtlas(const HudArt& art)
{
  int numRects = 0;
  const auto* pRects = hudAtlasRects(HudTier::P1080, &numRects);

  std::array<HudSpriteRect, hud_sprite::CountWithExtra> rects{};
  std::copy(pRects, pRects + numRects, rects.begin());

  // The default skin uses larger cells for the weapon and projectile icons than
  // the sprite table of the original artwork, so that they are easier to see.
  // The cells live in a band below the original layout.
  for (auto i = 0; i <= WeaponLast - WeaponFirst; ++i)
  {
    rects[std::size_t(WeaponFirst + i)] = {4 + i * 148, 310, 140, 48};
  }
  for (auto i = 0; i < 5; ++i)
  {
    rects[std::size_t(ProjectileNormal + i)] = {4 + i * 108, 364, 100, 48};
  }

  auto R = [&](const int index) {
    return rects[std::size_t(index)];
  };

  Canvas c{ATLAS_WIDTH, ATLAS_HEIGHT};

  // -- Health pips ----------------------------------------------------
  {
    const auto& empty = R(PipEmpty);
    c.fillRect(empty, Color{0, 0, 0, 120});
    c.rectOutline(empty, Color{255, 255, 255, 70}, 2);

    const auto& full = R(PipFull);
    for (auto y = 0; y < full.h; ++y)
    {
      const auto t = float(y) / full.h;
      c.fillRect(
        full.x,
        full.y + y,
        full.w,
        1,
        Color{255, std::uint8_t(175 - 40 * t), 10, 255});
    }
    c.rectOutline(full, Color{255, 220, 150, 140}, 2);

    for (auto i = 0; i <= PipLowHealthLast - PipLowHealthFirst; ++i)
    {
      const auto& r = R(PipLowHealthFirst + i);
      const auto t = float(i) / 4.0f;
      c.fillRect(
        r,
        Color{std::uint8_t(110 + 145 * t), std::uint8_t(20 + 40 * t), 30, 235});
      c.rectOutline(r, Color{255, 255, 255, 60}, 2);
    }

    const auto& bg = R(PipBackground);
    c.fillRect(bg, Color{0, 0, 0, 130});
    c.rectOutline(bg, Color{255, 255, 255, 60}, 2);

    c.radial(
      R(PipGlow), ORANGE, [](float d) { return (1.0f - d) * (1.0f - d); });
  }

  // -- Ammo bar and boss bar pieces ------------------------------------
  for (const auto index : {AmmoBarLeft, AmmoBarMiddle, AmmoBarRight})
  {
    c.fillRect(R(index), Color{130, 215, 0, 255});
  }
  for (const auto index : {BossFillLeft, BossFillMiddle, BossFillRight})
  {
    c.fillRect(R(index), Color{220, 40, 30, 255});
  }
  {
    const auto& boss = R(BossBar);
    c.gradientPanel(boss, Color{0, 0, 0, 255}, 20, 190);
    drawText(c, "BOSS", boss.x + 12, boss.y + 17, 3, Color{235, 235, 235, 255});
    // The dark track the health fill is drawn into (see overlay_hud.cpp)
    c.fillRect(boss.x + 94, boss.y + 18, 1083, 20, Color{0, 0, 0, 170});
    c.rectOutline(
      {boss.x + 94, boss.y + 18, 1083, 20}, Color{255, 255, 255, 60}, 1);
  }

  // -- Panels -----------------------------------------------------------
  {
    // Top left: status
    const auto& p = R(StatusPanel);
    c.gradientPanel(p, Color{8, 8, 12, 255}, 200, 20);
    // Both slots are centered where the overlay renderer puts the icons
    c.slot({p.x + 26, p.y + 72, 156, 50});
    c.slot({p.x + 190, p.y + 72, 108, 50});
    c.fillRect(p.x + 50, p.y + 123, 274, 10, Color{0, 0, 0, 140});
    c.rectOutline({p.x + 50, p.y + 123, 274, 10}, Color{255, 255, 255, 50}, 1);
  }

  {
    // Top right: score and letters
    const auto& p = R(ScorePanel);
    c.gradientPanel(p, Color{8, 8, 12, 255}, 20, 200);
    drawText(
      c,
      "SCORE",
      p.x + p.w - 20 - 5 * 6 * 3,
      p.y + 6,
      3,
      Color{190, 190, 190, 230});
    for (auto i = 0; i < 5; ++i)
    {
      c.slot({p.x + 21 + 46 * i, p.y + 83, 40, 40}, 8);
    }
  }

  {
    // Bottom left: item grid and radar
    const auto& p = R(InventoryPanel);
    c.gradientPanel(p, Color{8, 8, 12, 255}, 200, 20);
    for (auto row = 0; row < 3; ++row)
    {
      for (auto col = 0; col < 2; ++col)
      {
        c.slot({p.x + 50 + 70 * col, p.y + 11 + 67 * row, 60, 60}, 10);
      }
    }

    // Radar disc, centered where the overlay renderer expects it
    const auto center = HudSpriteRect{p.x + 200, p.y, 220, 220};
    c.radial(center, Color{0, 0, 0, 255}, [](float) { return 0.5f; });
    c.radial(center, ORANGE, [](float d) { return d > 0.955f ? 1.0f : 0.0f; });
    c.radial({center.x + 40, center.y + 40, 140, 140}, ORANGE, [](float d) {
      return (d > 0.97f && d < 1.0f) ? 0.45f : 0.0f;
    });
    c.radial({center.x + 75, center.y + 75, 70, 70}, ORANGE, [](float d) {
      return (d > 0.94f && d < 1.0f) ? 0.45f : 0.0f;
    });
    c.fillRect(center.x + 10, center.y + 109, 200, 2, Color{255, 255, 255, 25});
    c.fillRect(center.x + 109, center.y + 10, 2, 200, Color{255, 255, 255, 25});
  }

  c.radial(R(RadarRing), Color{255, 255, 255, 255}, [](float d) {
    const auto dist = std::abs(d - 0.92f);
    return std::max(0.0f, 1.0f - dist / 0.06f);
  });
  c.radial(R(RadarDot), Color{255, 214, 140, 255}, [](float d) {
    return (1.0f - d) * (1.0f - d) * 1.4f;
  });

  // -- Letters -----------------------------------------------------------
  {
    constexpr char NAMES[] = {'N', 'U', 'K', 'E', 'M'};
    const Color fills[] = {
      {205, 205, 205, 255},
      {40, 80, 235, 255},
      {230, 60, 30, 255},
      {40, 175, 60, 255},
      {235, 235, 235, 255}};
    for (auto i = 0; i < 5; ++i)
    {
      drawBigLetter(c, NAMES[i], R(LetterFirst + i), fills[i]);
    }
  }

  // -- Icons from the game's own sprites ----------------------------------
  {
    for (auto i = 0; i < 5; ++i)
    {
      if (const auto& icon = art.mItemIcons[std::size_t(i)])
      {
        c.blitFit(*icon, R(ItemFirst + i));
      }
    }

    // Weapon slot: Duke's own gun. In the game it is only partly visible, since
    // his hands and arm cover the rear half, so the barrel is taken from his
    // sprite and the hidden receiver, grip and end cap are drawn in the same
    // style and palette. Each weapon gets a different colored status light.
    {
      const auto& duke = art.mPlayerSprite;

      // Pick the green of the receiver from the sprite itself
      Color green{0, 170, 0, 255};
      if (duke)
      {
        const auto& img = *duke;
        auto best = -1;
        for (auto y = 14; y < 24; ++y)
        {
          for (auto x = 12; x < int(img.width()); ++x)
          {
            const auto px = img.pixelData()[std::size_t(y) * img.width() + x];
            if (
              px.a > 0 && px.g > px.r + 40 && px.g > px.b + 40 &&
              int(px.g) > best)
            {
              best = px.g;
              green = px;
            }
          }
        }
      }

      const auto scaleColor = [](const Color& color, const float f) {
        return Color{
          std::uint8_t(std::min(255.0f, color.r * f)),
          std::uint8_t(std::min(255.0f, color.g * f)),
          std::uint8_t(std::min(255.0f, color.b * f)),
          color.a};
      };

      const Color lights[] = {
        {255, 153, 0, 255}, // normal: orange
        {90, 170, 255, 255}, // laser: blue
        {235, 60, 50, 255}, // rocket: red
        {255, 225, 70, 255}}; // flame thrower: yellow

      for (auto i = 0; i <= WeaponLast - WeaponFirst; ++i)
      {
        const auto& r = R(WeaponFirst + i);
        if (art.mGun)
        {
          constexpr auto gunScale = 4;
          const auto w = int(art.mGun->width());
          const auto h = int(art.mGun->height());
          c.blitCropScaled(
            *art.mGun,
            0,
            0,
            w,
            h,
            r.x + (r.w - w * gunScale) / 2,
            r.y + (r.h - h * gunScale) / 2,
            gunScale);
          continue;
        }
        constexpr auto gridW = 29;
        constexpr auto gridH = 10;
        constexpr auto scale = 4;

        Canvas g{gridW, gridH};
        const Color black{0, 0, 0, 255};
        auto part = [&](
                      const int x,
                      const int y,
                      const int w,
                      const int h,
                      const Color& fill) {
          g.fillRect(x - 1, y - 1, w + 2, h + 2, black);
          g.fillRect(x, y, w, h, fill);
        };

        const Color grey{150, 150, 160, 255};
        const Color darkGrey{70, 70, 82, 255};

        // Hidden rear half: end cap, receiver body, grip
        part(2, 3, 3, 4, grey);
        g.fillRect(2, 3, 3, 1, Color{205, 205, 215, 255});
        part(5, 2, 9, 5, scaleColor(green, 0.85f));
        g.fillRect(5, 2, 9, 1, scaleColor(green, 1.25f));
        g.fillRect(5, 6, 9, 1, scaleColor(green, 0.55f));
        part(7, 7, 4, 3, darkGrey); // grip
        g.fillRect(8, 7, 2, 1, grey);

        // Weapon-specific status light on the receiver
        const auto& light = lights[std::min(i, 3)];
        g.fillRect(9, 3, 2, 2, light);
        g.fillRect(9, 3, 1, 1, scaleColor(light, 1.3f));

        // The visible part: barrel from the sprite (without Duke's hand)
        if (duke)
        {
          g.blitCropScaled(*duke, 17, 16, 15, 7, 13, 1, 1, true);
        }
        else
        {
          part(14, 3, 13, 3, grey);
          g.fillRect(14, 3, 13, 1, Color{205, 205, 215, 255});
        }

        c.blitCropScaled(
          std::move(g).toImage(),
          0,
          0,
          gridW,
          gridH,
          r.x + (r.w - gridW * scale) / 2,
          r.y + (r.h - gridH * scale) / 2,
          scale);
      }
    }
  }

  // -- Projectile icons ----------------------------------------------------
  // The game's own status bar weapon icons are the projectiles, shown at 1x
  // here (small slot) and 2x in the weapon slot above.
  {
    const Color projectileColors[] = {
      {255, 153, 0, 255}, {90, 170, 255, 255}, {235, 60, 50, 255}, {255, 225, 70, 255}};
    for (auto i = 0; i < 4; ++i)
    {
      const auto& r = R(ProjectileNormal + i);
      if (const auto& icon = art.mProjectiles[std::size_t(i)])
      {
        c.blitCropScaled(
          *icon,
          0,
          0,
          32,
          16,
          r.x + (r.w - 96) / 2,
          r.y + (r.h - 48) / 2,
          3,
          false,
          true);
      }
      else
      {
        c.fillRect(
          r.x + (r.w - 60) / 2,
          r.y + (r.h - 18) / 2,
          60,
          18,
          projectileColors[i]);
      }
    }

    const auto& rapid = R(ProjectileRapid);
    c.fillRect(
      rapid.x + (rapid.w - 70) / 2,
      rapid.y + (rapid.h - 14) / 2,
      70,
      14,
      Color{255, 220, 60, 255});
  }

  return {std::move(c).toImage(), rects};
}

} // namespace rigel::ui
