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

#include "audio/duke1_speaker.hpp"
#include "assets/duke1_assets.hpp"

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

constexpr double TIMER_HZ =
  1193182.0 / 0x2051; // rate of the original's sound callback
constexpr double SPEAKER_CLOCK = 1193182.0;
constexpr float VOLUME = 0.26f;
// A simple speaker model: the square wave goes through a high-pass (the cone
// and its coupling), with a time constant of about 31 samples at 44.1 kHz, then
// a gentle two-stage low-pass that rounds the edges. The constants were tuned
// by ear against a reference recording; they are not taken from any code.
constexpr double HIGH_PASS_TAU_44K = 31.0;
constexpr double LOW_PASS_TAU_44K = 1.5;
constexpr int SOUNDS_PER_BANK = 24;


std::uint16_t
  word(const std::vector<std::uint8_t>& data, const std::size_t offset)
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
std::vector<Record>
  parseBank(const std::vector<std::uint8_t>& data, const bool secondBank)
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
    // The original rounds odd sequence offsets down, and for the second bank it
    // takes the low byte of the sequence offset as the priority instead of the
    // priority field (a quirk of its loader).
    auto position = std::size_t{word(data, entry)} & ~std::size_t{1};
    record.mPriority = secondBank ? data[entry] : data[entry + 2];
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


Duke1Speaker::Duke1Speaker(
  const std::filesystem::path& dataDir,
  const int episode)
{
  int sampleRate = 0;
  Uint16 format = 0;
  int channels = 0;
  if (Mix_QuerySpec(&sampleRate, &format, &channels) == 0)
  {
    return; // audio is switched off
  }

  std::vector<Record> records = parseBank(
    duke1::read_file(dataDir, duke1::asset_name("DUKE1", episode)), false);
  records.resize(SOUNDS_PER_BANK);
  auto second = parseBank(
    duke1::read_file(dataDir, duke1::asset_name("DUKE1-B", episode)), true);
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
      const auto target =
        (static_cast<double>(tick) + 1.0) * sampleRate / TIMER_HZ;
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

    samples.resize(
      samples.size() + static_cast<std::size_t>(sampleRate / 100),
      0.0f); // let the filter ring out
    {
      const auto scale = sampleRate / 44100.0;
      const auto highPass =
        HIGH_PASS_TAU_44K * scale / (HIGH_PASS_TAU_44K * scale + 1.0);
      const auto lowPass = 1.0 / (LOW_PASS_TAU_44K * scale + 1.0);
      double previousIn = 0.0;
      double previousOut = 0.0;
      double stage1 = 0.0;
      double stage2 = 0.0;
      for (auto& sample : samples)
      {
        previousOut = highPass * (previousOut + sample - previousIn);
        previousIn = sample;
        stage1 += lowPass * (previousOut - stage1);
        stage2 += lowPass * (stage1 - stage2);
        sample = static_cast<float>(stage2);
      }
    }

    sound.mPcm.resize(
      samples.size() * std::size_t(channels) * std::size_t(bytesPerSample));
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
      sound.mpChunk = Mix_QuickLoad_RAW(
        sound.mPcm.data(), static_cast<Uint32>(sound.mPcm.size()));
    }
    mSounds.push_back(std::move(sound));
  }

  // Channels after the engine's own ones are reserved for these sounds
  mFirstChannel = data::NUM_SOUND_IDS;
  if (Mix_AllocateChannels(-1) < mFirstChannel + NUM_CHANNELS)
  {
    Mix_AllocateChannels(mFirstChannel + NUM_CHANNELS);
  }

  // The two force-field/reactor hums are much quieter than the rest
  for (const auto quiet : {21, 36})
  {
    if (
      std::size_t(quiet) < mSounds.size() &&
      mSounds[std::size_t(quiet)].mpChunk)
    {
      Mix_VolumeChunk(
        static_cast<Mix_Chunk*>(mSounds[std::size_t(quiet)].mpChunk),
        MIX_MAX_VOLUME / 4);
    }
  }
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
  if (mFirstChannel < 0 || index < 0 || std::size_t(index) >= mSounds.size())
  {
    return;
  }
  const auto& sound = mSounds[std::size_t(index)];
  if (!sound.mpChunk)
  {
    return;
  }

  // Channel groups: effects in a group cut each other off, the rest play on
  // their own channel
  int channel = 4 + index;
  switch (index)
  {
    case 0:
    case 8:
    case 23:
    case 24:
    case 25:
    case 26:
    case 28:
    case 29:
    case 40:
    case 41:
    case 44:
      channel = 0;
      break;
    case 13:
    case 14:
      channel = 1;
      break;
    case 21:
      channel = 2;
      break;
    case 36:
      channel = 3;
      break;
    default:
      break;
  }
  Mix_PlayChannel(
    mFirstChannel + channel, static_cast<Mix_Chunk*>(sound.mpChunk), 0);
}


void Duke1Speaker::playHintTick()
{
  constexpr int WALKING_SOUND = 22;
  if (
    mFirstChannel < 0 || mSounds.size() <= WALKING_SOUND ||
    !mSounds[WALKING_SOUND].mpChunk)
    return;
  Mix_PlayChannel(
    mFirstChannel + NUM_CHANNELS - 1,
    static_cast<Mix_Chunk*>(mSounds[WALKING_SOUND].mpChunk),
    0);
}


void Duke1Speaker::setVolume(const float volume)
{
  if (mFirstChannel >= 0)
  {
    for (int i = 0; i < NUM_CHANNELS; ++i)
    {
      Mix_Volume(
        mFirstChannel + i,
        static_cast<int>(std::clamp(volume, 0.0f, 1.0f) * MIX_MAX_VOLUME));
    }
  }
}


void Duke1Speaker::stop()
{
  if (mFirstChannel >= 0)
  {
    for (int i = 0; i < NUM_CHANNELS; ++i)
    {
      Mix_HaltChannel(mFirstChannel + i);
    }
  }
}

} // namespace rigel
