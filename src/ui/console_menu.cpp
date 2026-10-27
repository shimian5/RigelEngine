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

#include "console_menu.hpp"

#include "ui/imgui_integration.hpp"

#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>


namespace rigel::ui
{

namespace
{

constexpr auto REF_HEIGHT = 1080.0f;

constexpr ImU32 ORANGE = IM_COL32(255, 153, 0, 255);
constexpr ImU32 WHITE = IM_COL32(255, 255, 255, 255);
constexpr ImU32 GREY = IM_COL32(190, 190, 190, 240);
constexpr ImU32 DISABLED = IM_COL32(105, 105, 105, 220);


struct Canvas
{
  ImDrawList* mpList;
  ImFont* mpFont;
  float mScale;
  ImVec2 mSize;

  float s(const float v) const { return v * mScale; }

  ImVec2 textSize(const char* text, const float sizeRef) const
  {
    return mpFont->CalcTextSizeA(s(sizeRef), 100000.0f, 0.0f, text);
  }

  void text(
    const char* text,
    const float xRef,
    const float centerYRef,
    const float sizeRef,
    const ImU32 color,
    const bool centered = false,
    const bool rightAligned = false) const
  {
    const auto size = textSize(text, sizeRef);
    auto x = s(xRef);
    if (centered)
    {
      x -= size.x / 2.0f;
    }
    else if (rightAligned)
    {
      x -= size.x;
    }

    const auto y = s(centerYRef) - size.y / 2.0f;
    mpList->AddText(
      mpFont,
      s(sizeRef),
      {x + s(2.0f), y + s(2.0f)},
      IM_COL32(0, 0, 0, 170),
      text);
    mpList->AddText(mpFont, s(sizeRef), {x, y}, color, text);
  }

  void trefoil(
    const float cxRef,
    const float cyRef,
    const float radiusRef,
    const ImU32 color) const
  {
    const auto cx = s(cxRef);
    const auto cy = s(cyRef);
    const auto rOut = s(radiusRef);
    const auto rIn = rOut * 0.36f;

    for (auto k = 0; k < 3; ++k)
    {
      const auto center = -1.5707963f + float(k) * 2.0943951f;
      constexpr auto half = 0.5f;
      ImVec2 pts[24];
      auto n = 0;
      for (auto i = 0; i <= 8; ++i)
      {
        const auto a = center - half + (2.0f * half) * float(i) / 8.0f;
        pts[n++] = {cx + std::cos(a) * rOut, cy + std::sin(a) * rOut};
      }
      for (auto i = 8; i >= 0; --i)
      {
        const auto a = center - half + (2.0f * half) * float(i) / 8.0f;
        pts[n++] = {cx + std::cos(a) * rIn, cy + std::sin(a) * rIn};
      }
      mpList->AddConvexPolyFilled(pts, n, color);
    }

    mpList->AddCircleFilled({cx, cy}, rIn * 0.55f, color, 24);
  }

  void buttonLegend(
    const char* glyph,
    const char* label,
    const float rightXRef,
    const float yRef,
    const bool pill) const
  {
    const auto labelSize = textSize(label, 52.0f);
    const auto labelX = s(rightXRef) - labelSize.x;
    text(label, rightXRef, yRef, 52.0f, WHITE, false, true);

    const auto gap = s(24.0f);
    if (pill)
    {
      const auto w = s(122.0f);
      const auto h = s(44.0f);
      const auto x1 = labelX - gap;
      mpList->AddRectFilled(
        {x1 - w, s(yRef) - h / 2.0f + s(14.0f)},
        {x1, s(yRef) + h / 2.0f + s(14.0f)},
        WHITE,
        h / 2.0f);
      text(glyph, (x1 - w / 2.0f) / mScale, yRef - 28.0f, 30.0f, WHITE, true);
    }
    else
    {
      const auto r = s(34.0f);
      const auto cx = labelX - gap - r;
      mpList->AddCircleFilled({cx, s(yRef)}, r, WHITE, 32);
      const auto g = textSize(glyph, 46.0f);
      mpList->AddText(
        mpFont,
        s(46.0f),
        {cx - g.x / 2.0f, s(yRef) - g.y / 2.0f},
        IM_COL32(20, 20, 20, 255),
        glyph);
    }
  }

  /** Approximate width of a legend entry in reference pixels */
  float legendWidthRef(const char* label, const bool pill) const
  {
    return textSize(label, 52.0f).x / mScale + (pill ? 122.0f : 68.0f) + 24.0f;
  }
};


void drawMainBackdrop(const Canvas& c, const float offsetX)
{
  // Dark gradient with a large, faint radiation symbol
  c.mpList->AddRectFilledMultiColor(
    {0, 0},
    c.mSize,
    IM_COL32(26, 26, 32, 255),
    IM_COL32(12, 12, 16, 255),
    IM_COL32(4, 4, 6, 255),
    IM_COL32(14, 14, 18, 255));
  c.trefoil(1540.0f + offsetX, 560.0f, 520.0f, IM_COL32(255, 153, 0, 34));
}

} // namespace


void ConsoleList::setItems(std::vector<ConsoleMenuItem> items)
{
  mItems = std::move(items);
  mSelected = 0;
  mFirstVisible = 0;

  // Start on the first enabled item
  for (auto i = 0; i < int(mItems.size()); ++i)
  {
    if (mItems[std::size_t(i)].mEnabled)
    {
      mSelected = i;
      break;
    }
  }
}


void ConsoleList::move(const int direction)
{
  const auto count = int(mItems.size());
  if (count == 0)
  {
    return;
  }

  for (auto step = 0; step < count; ++step)
  {
    mSelected = (mSelected + direction + count) % count;
    if (mItems[std::size_t(mSelected)].mEnabled)
    {
      break;
    }
  }

  if (mSelected < mFirstVisible)
  {
    mFirstVisible = mSelected;
  }
  if (mSelected >= mFirstVisible + MAX_VISIBLE_ROWS)
  {
    mFirstVisible = mSelected - MAX_VISIBLE_ROWS + 1;
  }
}


std::string formatScoreWithCommas(const int score)
{
  auto s = std::to_string(score);
  for (int i = int(s.size()) - 3; i > 0; i -= 3)
  {
    s.insert(std::size_t(i), ",");
  }
  return s;
}


std::string formatPlayTime(const double seconds)
{
  const auto total = int(seconds);
  char buffer[32];
  std::snprintf(buffer, sizeof(buffer), "%02d:%02d", total / 60, total % 60);
  return buffer;
}


void drawConsoleMenu(const ConsoleMenuView& view)
{
  const auto display = ImGui::GetIO().DisplaySize;
  Canvas c{
    ImGui::GetForegroundDrawList(),
    static_cast<ImFont*>(imgui_integration::hudFont()),
    display.y / REF_HEIGHT,
    display};
  if (!c.mpFont || !view.mpList)
  {
    return;
  }

  const auto widthRef = display.x / c.mScale;
  const auto offsetX = (widthRef - 1920.0f) / 2.0f;
  auto X = [&](const float v) {
    return v + offsetX;
  };

  if (view.mDrawMainBackdrop)
  {
    drawMainBackdrop(c, offsetX);
  }
  else
  {
    c.mpList->AddRectFilled(
      {0, 0},
      display,
      IM_COL32(18, 18, 24, std::clamp(view.mDimAlpha, 0, 255)));
  }

  if (view.mDrawMainBackdrop)
  {
    c.text(view.mTitle.c_str(), X(960.0f), 100.0f, 118.0f, ORANGE, true);
  }
  else
  {
    c.text(view.mTitle.c_str(), X(960.0f), 95.0f, 92.0f, WHITE, true);
  }

  if (!view.mSubtitle.empty())
  {
    c.text(view.mSubtitle.c_str(), X(960.0f), 200.0f, 48.0f, GREY, true);
  }

  const auto& list = *view.mpList;
  const auto count = int(list.items().size());
  const auto visible = std::min(count, ConsoleList::MAX_VISIBLE_ROWS);
  const auto pitch = visible > 5 ? 128.0f : 150.0f;
  const auto firstRowY = visible > 5 ? 290.0f : 330.0f;

  for (auto row = 0;
       row < ConsoleList::MAX_VISIBLE_ROWS && list.firstVisible() + row < count;
       ++row)
  {
    const auto index = list.firstVisible() + row;
    const auto& item = list.items()[std::size_t(index)];
    const auto y = firstRowY + float(row) * pitch;
    const auto selected = index == list.selected();

    if (selected)
    {
      const auto top = c.s(y - pitch * 0.45f);
      const auto bottom = c.s(y + pitch * 0.45f);
      const auto xRight = c.s(X(1180.0f));
      c.mpList->AddQuadFilled(
        {0.0f, top},
        {xRight, top},
        {xRight - c.s(60.0f), bottom},
        {0.0f, bottom},
        IM_COL32(0, 0, 0, 225));
      c.trefoil(X(108.0f), y, 46.0f, ORANGE);
      c.text(
        item.mLabel.c_str(),
        X(215.0f),
        y,
        82.0f,
        item.mEnabled ? ORANGE : DISABLED);
    }
    else
    {
      c.text(
        item.mLabel.c_str(),
        X(215.0f),
        y,
        72.0f,
        item.mEnabled ? GREY : DISABLED);
    }
  }

  if (list.firstVisible() > 0)
  {
    c.text("^", X(108.0f), firstRowY - pitch * 0.7f, 50.0f, GREY, true);
  }
  if (list.firstVisible() + ConsoleList::MAX_VISIBLE_ROWS < count)
  {
    c.text(
      "v",
      X(108.0f),
      firstRowY + pitch * float(ConsoleList::MAX_VISIBLE_ROWS) - pitch * 0.35f,
      50.0f,
      GREY,
      true);
  }

  // Info panel
  {
    const auto x = X(1380.0f);
    auto y = 290.0f;
    for (const auto& [text, isLabel] : view.mInfoLines)
    {
      if (isLabel)
      {
        y += 28.0f;
        c.text(text.c_str(), x, y, 50.0f, GREY);
      }
      else
      {
        c.text(text.c_str(), x, y, 56.0f, WHITE);
      }
      y += 62.0f;
    }
  }

  // Footer legend, right aligned
  auto right = X(1860.0f);
  constexpr auto footerY = 1000.0f;
  if (!view.mStartLabel.empty())
  {
    c.buttonLegend("START", view.mStartLabel.c_str(), right, footerY, true);
    right -= c.legendWidthRef(view.mStartLabel.c_str(), true) + 60.0f;
  }
  if (!view.mBackLabel.empty())
  {
    c.buttonLegend("B", view.mBackLabel.c_str(), right, footerY, false);
    right -= c.legendWidthRef(view.mBackLabel.c_str(), false) + 60.0f;
  }
  c.buttonLegend("A", view.mConfirmLabel.c_str(), right, footerY, false);
}

void drawConsoleResults(const ConsoleResultsView& view)
{
  const auto display = ImGui::GetIO().DisplaySize;
  Canvas c{
    ImGui::GetForegroundDrawList(),
    static_cast<ImFont*>(imgui_integration::hudFont()),
    display.y / REF_HEIGHT,
    display};
  if (!c.mpFont)
  {
    return;
  }

  const auto offsetX = (display.x / c.mScale - 1920.0f) / 2.0f;
  auto X = [&](const float v) {
    return v + offsetX;
  };

  c.mpList->AddRectFilled({0, 0}, display, IM_COL32(18, 18, 24, 255));
  c.text(view.mTitle.c_str(), X(960.0f), 95.0f, 92.0f, WHITE, true);

  const auto left = X(520.0f);
  const auto right = X(1400.0f);

  c.text("SCORE", left, 245.0f, 62.0f, GREY);
  c.text(view.mScore.c_str(), right, 245.0f, 62.0f, WHITE, false, true);
  c.text("TIME", left, 320.0f, 62.0f, GREY);
  c.text(view.mTime.c_str(), right, 320.0f, 62.0f, WHITE, false, true);

  c.text("SECRET BONUSES", left, 425.0f, 50.0f, ORANGE);
  auto y = 500.0f;
  for (const auto& row : view.mRows)
  {
    const auto shown = !row.mValue.empty();
    c.text(row.mLabel.c_str(), left, y, 56.0f, shown ? WHITE : DISABLED);
    if (shown)
    {
      c.text(
        row.mValue.c_str(),
        right,
        y,
        56.0f,
        row.mEarned ? ORANGE : DISABLED,
        false,
        true);
    }
    y += 68.0f;
  }

  if (!view.mTotal.empty())
  {
    const auto lineY = y - 20.0f;
    c.mpList->AddRectFilled(
      {c.s(left), c.s(lineY)}, {c.s(right), c.s(lineY + 3.0f)}, GREY);
    c.text("BONUS TOTAL", left, y + 30.0f, 60.0f, WHITE);
    c.text(view.mTotal.c_str(), right, y + 30.0f, 60.0f, ORANGE, false, true);
  }

  c.buttonLegend("A", view.mConfirmLabel.c_str(), X(1860.0f), 1000.0f, false);
}


void drawConsoleTitleScreen(
  const std::string& title,
  const std::string& prompt,
  const double timeSeconds)
{
  const auto display = ImGui::GetIO().DisplaySize;
  Canvas c{
    ImGui::GetForegroundDrawList(),
    static_cast<ImFont*>(imgui_integration::hudFont()),
    display.y / REF_HEIGHT,
    display};
  if (!c.mpFont)
  {
    return;
  }

  const auto offsetX = (display.x / c.mScale - 1920.0f) / 2.0f;
  drawMainBackdrop(c, offsetX);

  c.text(title.c_str(), 960.0f + offsetX, 470.0f, 190.0f, ORANGE, true);

  // Pulsing prompt at the bottom
  const auto pulse = 0.55f + 0.45f * std::sin(float(timeSeconds) * 3.0f);
  const auto legendWidth = c.legendWidthRef(prompt.c_str(), false);
  const auto alpha = int(255.0f * pulse);
  (void)alpha; // the legend widget draws at full strength, the pulse is on the
               // plate below
  c.mpList->AddRectFilled(
    {0, c.s(930.0f)},
    {display.x, c.s(1070.0f)},
    IM_COL32(0, 0, 0, int(120.0f * pulse)));
  c.buttonLegend(
    "A", prompt.c_str(), 960.0f + offsetX + legendWidth / 2.0f, 1000.0f, false);
}


const char* randomQuitMessage()
{
  static const char* MESSAGES[] = {
    "LEAVING ALREADY? THE ALIENS WILL BE THRILLED.",
    "BUT WE'RE JUST GETTING STARTED!",
    "EARTH STILL NEEDS ITS HERO.",
    "GIVING UP? DUKE NEVER DOES.",
    "QUIT NOW AND THE ALIENS WIN.",
    "THE COFFEE CAN WAIT. THE ALIENS CAN'T.",
    "ONE MORE LEVEL. YOU KNOW YOU WANT TO."};

  const auto ticks = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now().time_since_epoch())
                       .count();
  return MESSAGES
    [std::size_t(ticks / 7) % (sizeof(MESSAGES) / sizeof(MESSAGES[0]))];
}

} // namespace rigel::ui
