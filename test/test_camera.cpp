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
#include <data/game_traits.hpp>
#include <data/map.hpp>
#include <data/player_model.hpp>
#include <engine/collision_checker.hpp>
#include <engine/random_number_generator.hpp>
#include <engine/visual_components.hpp>
#include <game_logic/camera.hpp>
#include <game_logic/player.hpp>
#include <game_logic/player/components.hpp>

RIGEL_DISABLE_WARNINGS
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
RIGEL_RESTORE_WARNINGS

using namespace rigel;
using namespace game_logic;
using namespace engine::components;

namespace
{

struct CameraEntityFactory : MockEntityFactory
{
  explicit CameraEntityFactory(entityx::EntityManager* entities)
    : MockEntityFactory(entities)
  {
    // enterShip reads both the initial draw order and ship animation bounds.
    mShipDrawData.mDrawOrder = 0;
    mShipDrawData.mFrames = {
      engine::SpriteFrame{0, {}, {6, 3}},
      engine::SpriteFrame{0, {}, {6, 3}}};
  }

  Sprite createSpriteForId(data::ActorID) override
  {
    return {&mShipDrawData, {}};
  }

  engine::SpriteDrawData mShipDrawData;
};

} // namespace


TEST_CASE("Ship camera horizontal dead zone", "[camera]")
{
  entityx::EntityX entities;
  data::map::Map map{200, 200, data::map::TileAttributeDict{{0x0, 0xF}}};
  engine::CollisionChecker collision{&map, entities.entities, entities.events};
  data::PersistentPlayerState persistent;
  MockServiceProvider services;
  CameraEntityFactory factory{&entities.entities};
  engine::RandomNumberGenerator random;
  data::GameOptions options;
  auto entity = entities.entities.create();
  entity.assign<WorldPosition>(61, 59);
  entity.assign<Sprite>(factory.createSpriteForId(data::ActorID::Dukes_ship_RIGHT));
  assignPlayerComponents(entity, Orientation::Right);
  Player player{
    entity,
    data::Difficulty::Medium,
    &persistent,
    &services,
    &options,
    &collision,
    &map,
    &factory,
    &entities.events,
    &random};

  const auto facing = GENERATE(Orientation::Left, Orientation::Right);
  player.enterShip({61, 59}, facing);
  REQUIRE(player.stateIs<InShip>());

  auto viewport = data::GameTraits::mapViewportSize;
  const auto extraWidth = GENERATE(0, 20);
  viewport.width += extraWidth;
  CAPTURE(facing, extraWidth);
  Camera camera{&player, map, entities.events};
  camera.centerViewOnPlayer();
  camera.recenter(viewport);
  const auto origin = camera.position().x;
  REQUIRE(origin > 2);
  const auto centerOffset =
    (viewport.width - data::GameTraits::mapViewportSize.width) / 2;
  auto& position = *entity.component<WorldPosition>();

  int playerOffset = 11;
  int cameraDelta = 0;
  SECTION("Left edge does not scroll") {}
  SECTION("One tile beyond the left edge")
  {
    playerOffset = 10;
    cameraDelta = -1;
  }
  SECTION("Leftward catch-up is capped")
  {
    playerOffset = 7;
    cameraDelta = -2;
  }
  SECTION("Right edge does not scroll") { playerOffset = 15; }
  SECTION("One tile beyond the right edge")
  {
    playerOffset = 16;
    cameraDelta = 1;
  }
  SECTION("Rightward catch-up is capped")
  {
    playerOffset = 19;
    cameraDelta = 2;
  }

  position.x = origin + playerOffset + centerOffset;
  camera.update({}, viewport);
  CHECK(camera.position().x == origin + cameraDelta);
}
