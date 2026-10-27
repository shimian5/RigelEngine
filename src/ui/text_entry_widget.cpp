/* Copyright (C) 2019, Nikolai Wuttke. All rights reserved.
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

#include "text_entry_widget.hpp"

#include <string_view>

#include "base/color.hpp"
#include "ui/menu_element_renderer.hpp"


namespace rigel::ui
{

namespace
{

constexpr auto TEXT_COLOR = base::Color{109, 109, 109, 255};


bool isAsciiChar(const char chValue)
{
  const auto value = static_cast<unsigned char>(chValue);
  return value <= 127;
}

} // namespace


TextEntryWidget::TextEntryWidget(
  const ui::MenuElementRenderer* pUiRenderer,
  const int posX,
  const int posY,
  const int maxTextLength,
  const Style textStyle,
  const std::string_view initialText)
  : mText(initialText)
  , mpUiRenderer(pUiRenderer)
  , mPosX(posX)
  , mPosY(posY)
  , mMaxTextLength(maxTextLength)
  , mTextStyle(textStyle)
{
}


void TextEntryWidget::handleEvent(const SDL_Event& event)
{
  if (
    event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_BACKSPACE &&
    !mText.empty())
  {
    mText.pop_back();
  }

  // Gamepad entry, arcade style: up/down cycles the last letter, right adds
  // a letter, left removes one
  if (event.type == SDL_CONTROLLERBUTTONDOWN)
  {
    static constexpr std::string_view LETTERS =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -";
    const auto capacity = static_cast<size_t>(mMaxTextLength);

    switch (event.cbutton.button)
    {
      case SDL_CONTROLLER_BUTTON_DPAD_UP:
      case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
        {
          if (mText.empty() && capacity > 0)
          {
            mText += 'A';
            break;
          }
          if (!mText.empty())
          {
            const auto step =
              event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_UP ? 1 : -1;
            const auto pos = LETTERS.find(mText.back());
            const auto size = static_cast<int>(LETTERS.size());
            const auto current = pos == std::string_view::npos ? 0 : int(pos);
            mText.back() = LETTERS[size_t((current + step + size) % size)];
          }
          break;
        }

      case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
        if (mText.size() < capacity)
        {
          mText += mText.empty() ? 'A' : mText.back();
        }
        break;

      case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
        if (!mText.empty())
        {
          mText.pop_back();
        }
        break;

      default:
        break;
    }
  }

  if (event.type == SDL_TEXTINPUT)
  {
    const auto newChar = event.text.text[0];
    if (!isAsciiChar(newChar))
    {
      return;
    }

    if (mText.size() < static_cast<size_t>(mMaxTextLength))
    {
      mText += newChar;
    }
  }
}


void TextEntryWidget::updateAndRender(const engine::TimeDelta dt)
{
  auto drawText = [this](std::string_view text) {
    if (mTextStyle == Style::BigText)
    {
      mpUiRenderer->drawBigText(mPosX, mPosY, text, TEXT_COLOR);
    }
    else
    {
      mpUiRenderer->drawText(mPosX, mPosY, text);
    }
  };


  mElapsedTime += dt;

  // Text
  {
    // TODO: Instead of drawing an empty string to clear, draw a black
    // rectangle
    const auto spaces =
      std::string(static_cast<size_t>(mMaxTextLength + 1), ' ');
    drawText(spaces);
    drawText(mText);
  }

  // Cursor
  {
    const auto cursorOffset = static_cast<int>(mText.size());
    mpUiRenderer->drawTextEntryCursor(
      mPosX + cursorOffset, mPosY, mElapsedTime);
  }
}


std::string_view TextEntryWidget::text() const
{
  return mText;
}

} // namespace rigel::ui
