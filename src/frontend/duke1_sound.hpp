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

#include <cstdint>
#include <filesystem>
#include <vector>


namespace rigel
{

/** Duke Nukem 1 sound effects.
 *
 * The original game drives the PC speaker: each sound is a list of timer
 * divisors played back one per timer tick (about 144 per second). This reads
 * the two sound banks from the player's data files and renders every sound
 * into a buffer for SDL_mixer, using the original's priority rule.
 */
class Duke1Speaker
{
public:
  explicit Duke1Speaker(const std::filesystem::path& dataDir);
  ~Duke1Speaker();

  Duke1Speaker(const Duke1Speaker&) = delete;
  Duke1Speaker& operator=(const Duke1Speaker&) = delete;

  /** Play sound request 0-47 (0-23 first bank, 24-47 second bank) */
  void play(int index);
  void stop();

private:
  struct Sound
  {
    int mPriority = 0;
    std::vector<std::uint8_t> mPcm;
    void* mpChunk = nullptr; // Mix_Chunk*
  };

  std::vector<Sound> mSounds;
  int mChannel = -1;
  int mCurrent = -1;
};

} // namespace rigel
