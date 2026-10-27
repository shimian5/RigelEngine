#include "frontend/duke1_saves.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE(
  "DN1 replay saves retain episode and accept legacy episode one",
  "[duke1]")
{
  rigel::Duke1Save save;
  save.mLevelIndex = 7;
  save.mCarry.score = 12345;
  save.mWide = true;
  save.mInputs = {0, 1, 16, 32, 3};
  for (int episode = 0; episode < 3; ++episode)
  {
    save.mEpisode = episode;
    const auto bytes = rigel::serializeDuke1Save(save);
    const auto restored = rigel::deserializeDuke1Save(bytes);
    REQUIRE(restored);
    CHECK(restored->mEpisode == episode);
    CHECK(restored->mLevelIndex == 7);
    CHECK(restored->mCarry.score == 12345);
    CHECK(restored->mWide);
    CHECK(restored->mInputs == save.mInputs);
    for (std::size_t i = 0; i < bytes.size(); ++i)
    {
      CHECK_FALSE(
        rigel::deserializeDuke1Save({bytes.begin(), bytes.begin() + i}));
    }
  }

  auto legacy = rigel::serializeDuke1Save(save);
  legacy[4] = 2;
  legacy.erase(legacy.begin() + 9, legacy.begin() + 13);
  const auto restored = rigel::deserializeDuke1Save(legacy);
  REQUIRE(restored);
  CHECK(restored->mEpisode == 0);
  CHECK(restored->mLevelIndex == save.mLevelIndex);
  CHECK(restored->mInputs == save.mInputs);

  auto invalid = rigel::serializeDuke1Save(save);
  invalid[9] = 3;
  CHECK_FALSE(rigel::deserializeDuke1Save(invalid));
  invalid = rigel::serializeDuke1Save(save);
  invalid[4] = 255;
  CHECK_FALSE(rigel::deserializeDuke1Save(invalid));
}

TEST_CASE("DN1 saves reject invalid carry before replay", "[duke1]")
{
  rigel::Duke1Save save;
  SECTION("negative level") { save.mLevelIndex = -1; }
  SECTION("negative episode") { save.mEpisode = -1; }
  SECTION("negative score") { save.mCarry.score = -1; }
  SECTION("health above maximum") { save.mCarry.health = 10; }
  SECTION("negative health") { save.mCarry.health = -1; }
  SECTION("invalid boots selector") { save.mCarry.boost = 2; }
  SECTION("invalid claw selector") { save.mCarry.claw = 2; }
  SECTION("zero projectile capacity") { save.mCarry.capacity = 0; }
  SECTION("capacity exceeds the projectile pool") { save.mCarry.capacity = 5; }
  CHECK_FALSE(rigel::deserializeDuke1Save(rigel::serializeDuke1Save(save)));
}
