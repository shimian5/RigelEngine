#include "console_results.hpp"

#include <algorithm>


namespace rigel::ui
{

namespace
{

constexpr auto START_DELAY = 0.7;
constexpr auto ROW_INTERVAL = 0.65;
constexpr auto COUNT_DURATION = 1.0;
constexpr auto TICK_INTERVAL = 0.05;

} // namespace


ConsoleResultsScreen::ConsoleResultsScreen(
  std::string title,
  std::string time,
  const int scoreBeforeBonuses,
  const int pointsPerBonus,
  std::vector<Row> rows,
  Sounds sounds)
  : mTitle(std::move(title))
  , mTime(std::move(time))
  , mScoreBefore(scoreBeforeBonuses)
  , mPointsPerBonus(pointsPerBonus)
  , mRows(std::move(rows))
  , mSounds(std::move(sounds))
{
  mEarnedCount = int(std::count_if(
    mRows.begin(), mRows.end(), [](const Row& row) { return row.mEarned; }));
}


int ConsoleResultsScreen::revealedRows() const
{
  if (mElapsed < START_DELAY)
  {
    return 0;
  }
  return std::min(
    int(mRows.size()), 1 + int((mElapsed - START_DELAY) / ROW_INTERVAL));
}


bool ConsoleResultsScreen::tallyDone() const
{
  const auto countStart = START_DELAY + double(mRows.size()) * ROW_INTERVAL;
  return mElapsed >= countStart + COUNT_DURATION;
}


void ConsoleResultsScreen::update(const double dt)
{
  if (tallyDone() && mRowsPlayed == int(mRows.size()) && mTotalPlayed)
  {
    return;
  }
  mElapsed += dt;

  for (; mRowsPlayed < revealedRows(); ++mRowsPlayed)
  {
    const auto& play = mRows[std::size_t(mRowsPlayed)].mEarned
      ? mSounds.mRowEarned
      : mSounds.mRowMissed;
    if (play)
    {
      play();
    }
  }

  const auto countStart = START_DELAY + double(mRows.size()) * ROW_INTERVAL;
  if (mElapsed >= countStart && mEarnedCount > 0)
  {
    const auto ticks = std::min(
      int((mElapsed - countStart) / TICK_INTERVAL) + 1,
      int(COUNT_DURATION / TICK_INTERVAL));
    for (; mTicksPlayed < ticks; ++mTicksPlayed)
    {
      if (mSounds.mScoreTick)
      {
        mSounds.mScoreTick();
      }
    }
  }
  if (tallyDone() && !mTotalPlayed)
  {
    mTotalPlayed = true;
    if (mEarnedCount > 0 && mSounds.mTotal)
    {
      mSounds.mTotal();
    }
  }
}


bool ConsoleResultsScreen::confirm()
{
  if (!(tallyDone() && mTotalPlayed))
  {
    // Skip ahead: everything is shown at once, without the sounds
    mRowsPlayed = int(mRows.size());
    mTicksPlayed = int(COUNT_DURATION / TICK_INTERVAL);
    mTotalPlayed = true;
    mElapsed = START_DELAY + double(mRows.size()) * ROW_INTERVAL + COUNT_DURATION;
    return false;
  }
  return true;
}


void ConsoleResultsScreen::draw() const
{
  ConsoleResultsView view;
  view.mTitle = mTitle;
  view.mTime = mTime;

  const auto revealed = revealedRows();
  for (auto i = 0; i < int(mRows.size()); ++i)
  {
    const auto& row = mRows[std::size_t(i)];
    ConsoleResultRowView out;
    out.mLabel = row.mLabel;
    out.mEarned = row.mEarned;
    if (i < revealed)
    {
      out.mValue = row.mEarned ? formatScoreWithCommas(mPointsPerBonus) : "--";
    }
    view.mRows.push_back(std::move(out));
  }

  const auto countStart = START_DELAY + double(mRows.size()) * ROW_INTERVAL;
  auto score = mScoreBefore;
  if (mElapsed >= countStart)
  {
    view.mTotal = formatScoreWithCommas(bonusTotal());
    const auto progress =
      std::clamp((mElapsed - countStart) / COUNT_DURATION, 0.0, 1.0);
    score += int(double(bonusTotal()) * progress);
  }
  view.mScore = formatScoreWithCommas(score);

  drawConsoleResults(view);
}

} // namespace rigel::ui
