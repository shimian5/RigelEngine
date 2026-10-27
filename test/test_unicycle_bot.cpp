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


#include "utils.hpp"

#include <base/spatial_types_printing.hpp>
#include <data/game_options.hpp>
#include <data/map.hpp>
#include <data/player_model.hpp>
#include <engine/collision_checker.hpp>
#include <engine/random_number_generator.hpp>
#include <game_logic/entity_factory.hpp>
#include <game_logic/enemies/unicycle_bot.hpp>
#include <game_logic/global_dependencies.hpp>
#include <game_logic/player.hpp>
#include <game_logic/player/components.hpp>

RIGEL_DISABLE_WARNINGS
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
RIGEL_RESTORE_WARNINGS

using namespace rigel;
using namespace engine;
using namespace engine::components;
using namespace game_logic;
namespace ex = entityx;

namespace
{
struct UnicycleSpriteFactory : engine::ISpriteFactory
{
  Sprite createSprite(data::ActorID) override
  {
    static engine::SpriteDrawData drawData{};
    return {&drawData, {}};
  }
  base::Rect<int> actorFrameRect(data::ActorID, int) const override
  {
    return {{}, {3, 3}};
  }
  engine::SpriteFrame actorFrameData(data::ActorID, int) const override { return {}; }
};
} // namespace

TEST_CASE("Unicycle bot orientation and acceleration", "[unicycle]")
{
  auto run = [](const int playerX, const bool blocked) {
    ex::EntityX entityx;
    data::map::Map map{100, 100, data::map::TileAttributeDict{{0x0, 0xF}}};
    for (int x = 0; x < 40; ++x)
    {
      map.setTileAt(0, x, 23, 1);
    }
    if (blocked)
    {
      const auto wallX = playerX > 10 ? 13 : 9;
      for (int y = 0; y < 23; ++y)
      {
        map.setTileAt(0, wallX, y, 1);
      }
    }
    MockServiceProvider serviceProvider;
    engine::RandomNumberGenerator randomGenerator;
    UnicycleSpriteFactory spriteFactory;
    data::GameOptions options;
    EntityFactory entityFactory{
      &spriteFactory,
      &entityx.entities,
      &serviceProvider,
      &randomGenerator,
      &options,
      data::Difficulty::Medium};
    CollisionChecker collisionChecker{&map, entityx.entities, entityx.events};
    auto playerEntity = entityx.entities.create();
    playerEntity.assign<WorldPosition>(playerX, 20);
    playerEntity.assign<Sprite>();
    assignPlayerComponents(playerEntity, Orientation::Left);
    data::PersistentPlayerState persistentState;
    Player player{
      playerEntity,
      data::Difficulty::Medium,
      &persistentState,
      &serviceProvider,
      &options,
      &collisionChecker,
      &map,
      &entityFactory,
      &entityx.events,
      &randomGenerator};
    auto botEntity = entityFactory.spawnActor(data::ActorID::Unicycle_bot, {10, 20});
    auto& botPosition = *botEntity.component<WorldPosition>();
    base::Vec2 cameraPosition{};
    PerFrameState frameState;
    GlobalDependencies dependencies{
      &collisionChecker,
      nullptr,
      &randomGenerator,
      &entityFactory,
      &serviceProvider,
      &entityx.entities,
      &entityx.events};
    GlobalState state{&player, &cameraPosition, &map, &frameState};
    behaviors::UnicycleBot bot;
    for (int i = 0; i < 15; ++i)
    {
      bot.update(dependencies, state, true, botEntity);
    }
    const auto expectedOrientation = playerX > 10 ? Orientation::Right : Orientation::Left;
    CHECK(*botEntity.component<Orientation>() == expectedOrientation);
    REQUIRE(std::holds_alternative<behaviors::UnicycleBot::Accelerating>(bot.mState));
    frameState.mIsOddFrame = false;
    bot.update(dependencies, state, true, botEntity);
    CHECK(botEntity.component<Sprite>()->mFramesToRender[0] == 2);
    if (blocked)
    {
      CHECK(botPosition.x == (playerX > 10 ? 9 : 11));
      CHECK(std::holds_alternative<behaviors::UnicycleBot::Waiting>(bot.mState));
    }
    else
    {
      CHECK(botPosition.x == 10);
      frameState.mIsOddFrame = true;
      bot.update(dependencies, state, true, botEntity);
      CHECK(botEntity.component<Sprite>()->mFramesToRender[0] == 3);
      CHECK(botPosition.x == 10);
      for (int i = 0; i < 8; ++i)
      {
        frameState.mIsOddFrame = !frameState.mIsOddFrame;
        bot.update(dependencies, state, true, botEntity);
        CHECK(botPosition.x == 10);
      }
      REQUIRE(std::holds_alternative<behaviors::UnicycleBot::Moving>(bot.mState));
      const auto direction = playerX > 10 ? 1 : -1;
      frameState.mIsOddFrame = false;
      bot.update(dependencies, state, true, botEntity);
      CHECK(botEntity.component<Sprite>()->mFramesToRender[0] == 2);
      CHECK(botPosition.x == 10 + direction);
      frameState.mIsOddFrame = true;
      bot.update(dependencies, state, true, botEntity);
      CHECK(botEntity.component<Sprite>()->mFramesToRender[0] == 3);
      CHECK(botPosition.x == 10 + 2 * direction);
    }
  };
  const auto playerX = GENERATE(0, 30);
  const auto blocked = GENERATE(false, true);
  CAPTURE(playerX, blocked);
  run(playerX, blocked);
}
