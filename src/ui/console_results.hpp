#pragma once

#include "ui/console_menu.hpp"

#include <functional>
#include <string>
#include <vector>


namespace rigel::ui
{

/** The level results page of the console-style UI, used by both games.
 *
 * Lists the secret bonus categories, reveals them one after the other and
 * then adds the total to the score. The first confirm press finishes the
 * tally immediately, the second one leaves the page.
 */
class ConsoleResultsScreen
{
public:
  struct Row
  {
    std::string mLabel;
    bool mEarned = false;
    std::string mProgress = {};
  };

  struct Sounds
  {
    std::function<void()> mRowEarned;
    std::function<void()> mRowMissed;
    std::function<void()> mScoreTick;
    std::function<void()> mTotal;
  };

  ConsoleResultsScreen(
    std::string title,
    std::string time,
    int scoreBeforeBonuses,
    int pointsPerBonus,
    std::vector<Row> rows,
    Sounds sounds);

  void update(double dt);
  void draw() const;

  /** Returns true if this press left the page */
  bool confirm();

  int bonusTotal() const { return mPointsPerBonus * mEarnedCount; }

private:
  int revealedRows() const;
  bool tallyDone() const;

  std::string mTitle;
  std::string mTime;
  int mScoreBefore;
  int mPointsPerBonus;
  int mEarnedCount = 0;
  std::vector<Row> mRows;
  Sounds mSounds;

  double mElapsed = 0.0;
  int mRowsPlayed = 0;
  int mTicksPlayed = 0;
  bool mTotalPlayed = false;
};

} // namespace rigel::ui
