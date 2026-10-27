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

#include <string>
#include <utility>
#include <vector>


namespace rigel::ui
{

struct ConsoleMenuItem
{
  std::string mLabel;
  bool mEnabled = true;
};


/** Selection and scrolling state for a vertical list of menu items */
class ConsoleList
{
public:
  static constexpr int MAX_VISIBLE_ROWS = 6;

  void setItems(std::vector<ConsoleMenuItem> items);
  void move(int direction);

  const std::vector<ConsoleMenuItem>& items() const { return mItems; }
  int selected() const { return mSelected; }
  int firstVisible() const { return mFirstVisible; }
  bool hasSelection() const { return !mItems.empty(); }
  bool selectedIsEnabled() const
  {
    return hasSelection() && mItems[std::size_t(mSelected)].mEnabled;
  }

private:
  std::vector<ConsoleMenuItem> mItems;
  int mSelected = 0;
  int mFirstVisible = 0;
};


struct ConsoleMenuView
{
  std::string mTitle;
  std::string mSubtitle; // optional, shown below the title
  const ConsoleList* mpList = nullptr;

  // Lines for the info panel on the right; (text, isLabel)
  std::vector<std::pair<std::string, bool>> mInfoLines;

  std::string mConfirmLabel = "SELECT";
  std::string mBackLabel; // shown as "B <label>" if not empty
  std::string mStartLabel; // shown as "START <label>" if not empty

  // How strongly to darken what's behind the menu (0-255). The main menu
  // uses 255 and draws its own backdrop.
  int mDimAlpha = 168;
  bool mDrawMainBackdrop = false;
};


/** Draw a menu in the console style, at native resolution (uses Dear ImGui) */
void drawConsoleMenu(const ConsoleMenuView& view);

struct ConsoleResultRowView
{
  std::string mLabel;
  std::string mValue; // empty while the row hasn't been revealed yet
  bool mEarned = false;
};

struct ConsoleResultsView
{
  std::string mTitle;
  std::string mScore;
  std::string mTime;
  std::vector<ConsoleResultRowView> mRows;
  std::string mTotal; // empty until the tally has reached it
  std::string mConfirmLabel = "CONTINUE";
};

/** The level results page shared by both games */
void drawConsoleResults(const ConsoleResultsView& view);

/** Full screen title with a prompt, e.g. "A START". timeSeconds animates it. */
void drawConsoleTitleScreen(
  const std::string& title,
  const std::string& prompt,
  double timeSeconds);

/** A random lighthearted line for the "really quit?" screen */
const char* randomQuitMessage();

std::string formatScoreWithCommas(int score);
std::string formatPlayTime(double seconds);

} // namespace rigel::ui
