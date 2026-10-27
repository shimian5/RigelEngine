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

#include "duke1_sound.hpp"

#include "data/sound_ids.hpp"

#include <SDL_mixer.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>


namespace rigel
{

namespace
{

constexpr double TIMER_HZ = 1193182.0 / 0x2051; // rate of the original's sound callback
constexpr double SPEAKER_CLOCK = 1193182.0;
constexpr float VOLUME = 0.22f;
constexpr int SOUNDS_PER_BANK = 24;

std::vector<std::uint8_t> readFile(const std::filesystem::path& path)
{
  std::ifstream file(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}


std::uint16_t word(const std::vector<std::uint8_t>& data, const std::size_t offset)
{
  return static_cast<std::uint16_t>(data[offset] | (data[offset + 1] << 8));
}


struct Record
{
  int mPriority = 0;
  std::vector<std::uint16_t> mDivisors;
};


// Sound bank layout: "SND\0", size, count, 16 byte header, then one 16 byte
// entry per sound (offset, priority, rate, name), followed by the sequences,
// each ended by 0xFFFF.
std::vector<Record> parseBank(const std::vector<std::uint8_t>& data)
{
  std::vector<Record> records;
  if (data.size() < 16 || std::memcmp(data.data(), "SND", 4) != 0)
  {
    return records;
  }
  const auto count = word(data, 6);
  for (std::size_t i = 0; i < count; ++i)
  {
    const auto entry = 16 + i * 16;
    if (entry + 16 > data.size())
    {
      break;
    }
    Record record;
    auto position = std::size_t{word(data, entry)};
    record.mPriority = data[entry + 2];
    while (position + 2 <= data.size())
    {
      const auto value = word(data, position);
      position += 2;
      if (value == 0xFFFF)
      {
        break;
      }
      record.mDivisors.push_back(value);
    }
    records.push_back(std::move(record));
  }
  return records;
}

} // namespace


Duke1Speaker::Duke1Speaker(const std::filesystem::path& dataDir)
{
  int sampleRate = 0;
  Uint16 format = 0;
  int channels = 0;
  if (Mix_QuerySpec(&sampleRate, &format, &channels) == 0)
  {
    return; // audio is switched off
  }

  std::vector<Record> records = parseBank(readFile(dataDir / "DUKE1.DN1"));
  records.resize(SOUNDS_PER_BANK);
  auto second = parseBank(readFile(dataDir / "DUKE1-B.DN1"));
  second.resize(SOUNDS_PER_BANK);
  records.insert(records.end(), second.begin(), second.end());

  const auto bytesPerSample = (format & 0xFF) / 8;
  for (const auto& record : records)
  {
    Sound sound;
    sound.mPriority = record.mPriority;

    std::vector<float> samples;
    double phase = 0.0;
    double produced = 0.0;
    for (std::size_t tick = 0; tick < record.mDivisors.size(); ++tick)
    {
      const auto divisor = record.mDivisors[tick];
      const auto target = (static_cast<double>(tick) + 1.0) * sampleRate / TIMER_HZ;
      const auto frequency = divisor ? SPEAKER_CLOCK / divisor : 0.0;
      while (produced < target)
      {
        float value = 0.0f;
        if (frequency > 0.0 && frequency < sampleRate / 2.0)
        {
          phase += frequency / sampleRate;
          phase -= std::floor(phase);
          value = phase < 0.5 ? VOLUME : -VOLUME;
        }
        samples.push_back(value);
        produced += 1.0;
      }
    }

    sound.mPcm.resize(samples.size() * std::size_t(channels) * std::size_t(bytesPerSample));
    auto* pOut = sound.mPcm.data();
    for (const auto sample : samples)
    {
      for (int channel = 0; channel < channels; ++channel)
      {
        if (format == AUDIO_F32SYS)
        {
          std::memcpy(pOut, &sample, sizeof(float));
        }
        else
        {
          const auto value = static_cast<std::int16_t>(sample * 32767.0f);
          std::memcpy(pOut, &value, sizeof(value));
        }
        pOut += bytesPerSample;
      }
    }

    if (!sound.mPcm.empty())
    {
      sound.mpChunk = Mix_QuickLoad_RAW(sound.mPcm.data(), static_cast<Uint32>(sound.mPcm.size()));
    }
    mSounds.push_back(std::move(sound));
  }

  // One more channel than the engine's own sounds use, reserved for the speaker
  if (Mix_AllocateChannels(-1) <= data::NUM_SOUND_IDS)
  {
    Mix_AllocateChannels(data::NUM_SOUND_IDS + 1);
  }
  mChannel = data::NUM_SOUND_IDS;
}


Duke1Speaker::~Duke1Speaker()
{
  stop();
  for (auto& sound : mSounds)
  {
    if (sound.mpChunk)
    {
      Mix_FreeChunk(static_cast<Mix_Chunk*>(sound.mpChunk));
    }
  }
}


void Duke1Speaker::play(const int index)
{
  if (mChannel < 0 || index < 0 || std::size_t(index) >= mSounds.size())
  {
    return;
  }
  const auto& sound = mSounds[std::size_t(index)];
  if (!sound.mpChunk)
  {
    return;
  }

  // A request wins when it has at least the priority of what is still playing
  const auto stillPlaying = mCurrent >= 0 && Mix_Playing(mChannel);
  if (stillPlaying && sound.mPriority < mSounds[std::size_t(mCurrent)].mPriority)
  {
    return;
  }

  Mix_PlayChannel(mChannel, static_cast<Mix_Chunk*>(sound.mpChunk), 0);
  mCurrent = index;
}


void Duke1Speaker::stop()
{
  if (mChannel >= 0)
  {
    Mix_HaltChannel(mChannel);
  }
  mCurrent = -1;
}

} // namespace rigel
