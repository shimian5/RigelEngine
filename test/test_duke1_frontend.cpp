#include "duke1_frontend_fixture.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

namespace rigel
{
struct Duke1ModeTestAccess
{
  static int episode(const Duke1Mode& mode) { return mode.mEpisode; }
  static std::size_t level(const Duke1Mode& mode) { return mode.mLevelIndex; }
  static duke1::PlayWorld& world(Duke1Mode& mode) { return *mode.mpWorld; }
  static const auto& log(const Duke1Mode& mode) { return mode.mInputLog; }
  static bool finished(const Duke1Mode& mode) { return mode.mEpisodeFinished; }
  static const auto& pages(const Duke1Mode& mode) { return mode.mPages; }
  static bool inResults(const Duke1Mode& mode)
  {
    return mode.mPause == Duke1Mode::PauseScreen::Stats;
  }
  static void start(
    Duke1Mode& mode,
    std::size_t level,
    const duke1::PlayWorld::Carry& carry,
    bool wide = false)
  {
    mode.mWide = wide;
    mode.startLevel(level, &carry);
    mode.mPages.clear();
  }
  static void record(Duke1Mode& mode, const std::vector<std::uint8_t>& inputs)
  {
    for (auto packed : inputs)
    {
      mode.mpWorld->update(unpackDuke1Input(packed));
      mode.mInputLog.push_back(packed);
    }
  }
  static bool save(Duke1Mode& mode) { return mode.saveTo("flow_test"); }
  static bool load(Duke1Mode& mode) { return mode.loadFrom("flow_test"); }
  static void results(Duke1Mode& mode)
  {
    mode.mPages.clear();
    mode.mStatsCarry = mode.mpWorld->carry();
    mode.mpResults = std::make_unique<ui::ConsoleResultsScreen>(
      "TEST",
      "0:01",
      mode.mStatsCarry.score,
      10000,
      std::vector<ui::ConsoleResultsScreen::Row>{{"TEST", true}},
      ui::ConsoleResultsScreen::Sounds{});
    mode.mStatsCarry.score += mode.mpResults->bonusTotal();
    mode.enterPauseScreen(Duke1Mode::PauseScreen::Stats);
  }
  static void confirm(Duke1Mode& mode) { mode.activatePauseSelection(); }
  static bool validTips(const Duke1Mode& mode)
  {
    return mode.mpScreens->valid() &&
      !mode.mpScreens
         ->hallwayMessage(
           mode.mText->levels()[mode.mLevelIndex].intermissionMessage)
         .empty();
  }
};
} // namespace rigel

namespace
{
using namespace rigel;
using Access = rigel::Duke1ModeTestAccess;
using rigel::test::Duke1Fixture;
const duke1::PlayWorld::Carry CARRY{54321, 7, 1, 1, 3};

void checkCarry(
  const duke1::PlayWorld::Carry& actual,
  const duke1::PlayWorld::Carry& expected)
{
  CHECK(actual.score == expected.score);
  CHECK(actual.health == expected.health);
  CHECK(actual.boost == expected.boost);
  CHECK(actual.claw == expected.claw);
  CHECK(actual.capacity == expected.capacity);
}
void compareWorlds(duke1::PlayWorld& first, duke1::PlayWorld& second)
{
  CHECK(first.worldX() == second.worldX());
  CHECK(first.worldY() == second.worldY());
  CHECK(first.prevWorldX() == second.prevWorldX());
  CHECK(first.prevWorldY() == second.prevWorldY());
  CHECK(first.updateCount() == second.updateCount());
  CHECK(first.player().grounded == second.player().grounded);
  CHECK(first.player().falling == second.player().falling);
  CHECK(first.player().jumpIndex == second.player().jumpIndex);
  CHECK(first.player().facing == second.player().facing);
  CHECK(first.player().fireLatch == second.player().fireLatch);
  CHECK(first.wide() == second.wide());
  checkCarry(first.carry(), second.carry());
  std::vector<std::uint8_t> a, b;
  first.render(a);
  second.render(b);
  CHECK(a == b);
}
const std::vector<std::uint8_t> INPUTS{2, 2, 18, 18, 50, 18, 2,  34, 2, 2,
                                       0, 1, 1,  33, 1,  17, 17, 1,  0, 0};
} // namespace

TEST_CASE(
  "DN1 episode discovery requires matching EXE and usable map",
  "[duke1][flow]")
{
  Duke1Fixture fixture;
  CHECK(
    rigel::Duke1Mode::availableEpisodes(fixture.mData) ==
    std::vector<int>{0, 1, 2});
  std::filesystem::remove(fixture.mData / "DN2.EXE");
  rigel::test::writeBytes(fixture.mData / "WORLDAL1.DN3", {0});
  CHECK(
    rigel::Duke1Mode::availableEpisodes(fixture.mData) == std::vector<int>{0});
  rigel::Duke1Mode mode(fixture.context(), fixture.mData, std::nullopt, 0, 1);
  CHECK(Access::world(mode).backdropPair()[0] == "DROP2.DN1");
  CHECK(Access::world(mode).worldX() == 12 * 16);
}

TEST_CASE(
  "DN1 results advance directly and preserve carry and autosave",
  "[duke1][flow]")
{
  const int episode = GENERATE(0, 1, 2);
  Duke1Fixture fixture;
  rigel::Duke1Mode mode(
    fixture.context(), fixture.mData, std::nullopt, episode);
  Access::start(mode, 0, CARRY);
  Access::record(mode, INPUTS);
  REQUIRE(Access::validTips(mode)); // would expose the old extra page
  Access::results(mode);
  Access::confirm(mode);
  CHECK(Access::inResults(mode)); // first press finishes the tally only
  CHECK(Access::level(mode) == 0);
  Access::confirm(mode);
  CHECK_FALSE(Access::inResults(mode));
  CHECK_FALSE(Access::finished(mode));
  CHECK(Access::level(mode) == 1);
  CHECK(Access::pages(mode).empty());
  CHECK(Access::log(mode).empty());
  auto expected = Access::world(mode).carry();
  CHECK(expected.score == CARRY.score + 10000);
  checkCarry(expected, {CARRY.score + 10000, 7, 1, 1, 3});
  CHECK(
    Access::world(mode).backdropPair()[0] ==
    duke1::asset_name("DROP2", episode));
  const auto autosave = rigel::readDuke1Save("dn1_autosave");
  REQUIRE(autosave);
  CHECK(autosave->mEpisode == episode);
  CHECK(autosave->mLevelIndex == 1);
  CHECK(autosave->mInputs.empty());
  checkCarry(autosave->mCarry, expected);
}

TEST_CASE("DN1 final results route each episode to its ending", "[duke1][flow]")
{
  const int episode = GENERATE(0, 1, 2);
  Duke1Fixture fixture;
  rigel::Duke1Mode mode(
    fixture.context(), fixture.mData, std::nullopt, episode, 1);
  REQUIRE(Access::pages(mode).size() == 1); // selected level: ready page only
  auto* worldBefore = &Access::world(mode);
  const auto before = rigel::readDuke1Save("dn1_autosave");
  REQUIRE(before);
  Access::results(mode);
  Access::confirm(mode);
  Access::confirm(mode);
  CHECK(Access::finished(mode));
  CHECK(Access::level(mode) == 1);
  CHECK(&Access::world(mode) == worldBefore);
  CHECK(Access::pages(mode).size() == (episode == 2 ? 1u : 5u));
  const auto after = rigel::readDuke1Save("dn1_autosave");
  REQUIRE(after);
  CHECK(
    rigel::serializeDuke1Save(*before) == rigel::serializeDuke1Save(*after));
}

TEST_CASE(
  "DN1 replay load switches episodes and resumes deterministically",
  "[duke1][flow]")
{
  const int savedEpisode = GENERATE(0, 1, 2);
  Duke1Fixture fixture;
  rigel::Duke1Mode original(
    fixture.context(), fixture.mData, std::nullopt, savedEpisode);
  Access::start(original, 1, CARRY, true);
  Access::record(original, INPUTS);
  REQUIRE(Access::save(original));
  rigel::Duke1Mode loaded(
    fixture.context(), fixture.mData, std::nullopt, (savedEpisode + 1) % 3);
  Access::start(loaded, 0, {});
  const auto autosaveBefore = rigel::readDuke1Save("dn1_autosave");
  REQUIRE(autosaveBefore);
  REQUIRE(Access::load(loaded));
  const auto autosaveAfter = rigel::readDuke1Save("dn1_autosave");
  REQUIRE(autosaveAfter);
  CHECK(
    rigel::serializeDuke1Save(*autosaveBefore) ==
    rigel::serializeDuke1Save(*autosaveAfter));
  CHECK(Access::episode(loaded) == savedEpisode);
  CHECK(Access::level(loaded) == 1);
  CHECK(Access::log(loaded) == INPUTS);
  compareWorlds(Access::world(original), Access::world(loaded));
  Access::record(original, INPUTS);
  Access::record(loaded, INPUTS);
  compareWorlds(Access::world(original), Access::world(loaded));
}

TEST_CASE(
  "DN1 rejected loads leave the current episode and progress intact",
  "[duke1][flow]")
{
  Duke1Fixture fixture;
  rigel::Duke1Mode mode(fixture.context(), fixture.mData);
  Access::start(mode, 1, CARRY);
  Access::record(mode, INPUTS);
  auto* before = &Access::world(mode);
  const int x = before->worldX(), y = before->worldY();
  rigel::Duke1Save save;
  save.mEpisode = 1;
  SECTION("corrupt file") { fixture.writeSave({0, 1, 2}); }
  SECTION("level absent from selected episode")
  {
    save.mLevelIndex = 2;
    fixture.writeSave(rigel::serializeDuke1Save(save));
  }
  SECTION("selected episode is unavailable")
  {
    std::filesystem::remove(fixture.mData / "DN2.EXE");
    fixture.writeSave(rigel::serializeDuke1Save(save));
  }
  CHECK_FALSE(Access::load(mode));
  CHECK(Access::episode(mode) == 0);
  CHECK(Access::level(mode) == 1);
  CHECK(&Access::world(mode) == before);
  CHECK(before->worldX() == x);
  CHECK(before->worldY() == y);
  CHECK(Access::log(mode) == INPUTS);
}

TEST_CASE(
  "DN1 legacy replay loads episode one through the frontend",
  "[duke1][flow]")
{
  Duke1Fixture fixture;
  // Frozen version-2 header: independent of the current serializer.
  std::vector<std::uint8_t> bytes{'D',  'N',  '1', 'S', 2, 1, 0, 0, 0, // level
                                  0x31, 0xd4, 0,   0, // score 54321
                                  7,    0,    0,   0, // health
                                  1,    0,    0,   0, // boots
                                  1,    0,    0,   0, // claw
                                  3,    0,    0,   0, // projectile capacity
                                  0,    0,    0,   0, // normal viewport
                                  20,   0,    0,   0}; // input count
  bytes.insert(bytes.end(), INPUTS.begin(), INPUTS.end());
  fixture.writeSave(bytes);
  rigel::Duke1Mode mode(fixture.context(), fixture.mData, "flow_test", 2);
  CHECK(Access::episode(mode) == 0);
  CHECK(Access::level(mode) == 1);
  CHECK(Access::pages(mode).empty()); // loading must not replay the intro
  duke1::PlayWorld expected(fixture.mData, "WORLDAL3", nullptr, 0);
  expected.applyCarry(CARRY);
  for (auto input : INPUTS)
    expected.update(rigel::unpackDuke1Input(input));
  compareWorlds(Access::world(mode), expected);
}

TEST_CASE(
  "DN1 saved episode takes precedence over the launcher selection",
  "[duke1][flow]")
{
  Duke1Fixture fixture;
  rigel::Duke1Save save;
  save.mEpisode = 2;
  save.mLevelIndex = 1;
  save.mCarry = CARRY;
  fixture.writeSave(rigel::serializeDuke1Save(save));
  rigel::Duke1Mode mode(fixture.context(), fixture.mData, "flow_test", 0);
  CHECK(Access::episode(mode) == 2);
  CHECK(Access::level(mode) == 1);
  CHECK(Access::pages(mode).empty());
  checkCarry(Access::world(mode).carry(), CARRY);
}
