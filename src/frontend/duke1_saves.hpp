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

#include "duke1/play.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>


namespace rigel
{

/** A Duke Nukem 1 save: the level, what Duke carried into it, and every update's input since.
 *
 * Loading replays the inputs, which is exact because the game is deterministic.
 */
struct Duke1Save
{
  int mLevelIndex = 0;
  bool mWide = false;
  duke1::PlayWorld::Carry mCarry;
  std::vector<std::uint8_t> mInputs; // one packed byte per game update
};

constexpr int NUM_DUKE1_SLOTS = 8;

std::uint8_t packDuke1Input(const duke1::Input& input);
duke1::Input unpackDuke1Input(std::uint8_t packed);

bool writeDuke1Save(const std::string& slot, const Duke1Save& save);
std::optional<Duke1Save> readDuke1Save(const std::string& slot);
std::string duke1SlotName(int index); // 0..7 -> "dn1_slot1".."dn1_slot8"

/** The most recently written Duke 1 save (quick, auto or numbered) */
std::optional<std::string> newestDuke1Save();

} // namespace rigel
