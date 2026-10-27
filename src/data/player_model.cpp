/* Copyright (C) 2018, Nikolai Wuttke. All rights reserved.
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

#include "player_model.hpp"

#include "data/saved_game.hpp"

#include <algorithm>
#include <cassert>


namespace rigel::data
{

PersistentPlayerState::PersistentPlayerState()
  : mWeapon(WeaponType::Normal)
  , mScore(0)
  , mAmmo(MAX_AMMO)
  , mHealth(MAX_HEALTH)
{
}


PersistentPlayerState::PersistentPlayerState(const SavedGame& save)
  : mTutorialMessages(save.mTutorialMessagesAlreadySeen)
  , mWeapon(save.mWeapon)
  , mScore(save.mScore)
  , mAmmo(save.mAmmo)
  , mHealth(MAX_HEALTH)
{
}


PersistentPlayerState::CheckpointState
  PersistentPlayerState::makeCheckpoint() const
{
  return CheckpointState{mWeapon, mAmmo, mHealth};
}


void PersistentPlayerState::restoreFromCheckpoint(const CheckpointState& state)
{
  mHealth = std::max(2, state.mHealth);
  mWeapon = state.mWeapon;
  mAmmo = state.mAmmo;
}


int PersistentPlayerState::score() const
{
  return mScore;
}


void PersistentPlayerState::giveScore(const int amount)
{
  mScore = std::clamp(mScore + amount, 0, MAX_SCORE);
}


int PersistentPlayerState::ammo() const
{
  return mAmmo;
}


int PersistentPlayerState::currentMaxAmmo() const
{
  return mWeapon == WeaponType::FlameThrower ? MAX_AMMO_FLAME_THROWER
                                             : MAX_AMMO;
}


WeaponType PersistentPlayerState::weapon() const
{
  return mWeapon;
}


bool PersistentPlayerState::currentWeaponConsumesAmmo() const
{
  return mWeapon != WeaponType::Normal;
}


void PersistentPlayerState::switchToWeapon(const WeaponType type)
{
  mWeapon = type;
  mAmmo = currentMaxAmmo();
}


void PersistentPlayerState::useAmmo()
{
  if (currentWeaponConsumesAmmo())
  {
    --mAmmo;
    if (mAmmo <= 0)
    {
      switchToWeapon(WeaponType::Normal);
    }
  }
}


void PersistentPlayerState::setAmmo(int amount)
{
  assert(amount >= 0 && amount <= currentMaxAmmo());
  mAmmo = amount;
}


int PersistentPlayerState::health() const
{
  return mHealth;
}


bool PersistentPlayerState::isAtFullHealth() const
{
  return mHealth == MAX_HEALTH;
}


bool PersistentPlayerState::isDead() const
{
  return mHealth <= 0;
}


void PersistentPlayerState::takeDamage(const int amount)
{
  mHealth = std::clamp(mHealth - amount, 0, MAX_HEALTH);
}


void PersistentPlayerState::takeFatalDamage()
{
  mHealth = 0;
}


void PersistentPlayerState::giveHealth(const int amount)
{
  mHealth = std::clamp(mHealth + amount, 0, MAX_HEALTH);
}


const std::vector<InventoryItemType>& PersistentPlayerState::inventory() const
{
  return mInventory;
}


bool PersistentPlayerState::hasItem(const InventoryItemType type) const
{
  using namespace std;

  return find(begin(mInventory), end(mInventory), type) != end(mInventory);
}


void PersistentPlayerState::giveItem(InventoryItemType type)
{
  using namespace std;

  if (
    (type == InventoryItemType::RapidFire ||
     type == InventoryItemType::CloakingDevice) &&
    find(begin(mInventory), end(mInventory), type) != end(mInventory))
  {
    // Duke can only carry one rapid fire or cloaking device at a time.
    // Picking up a 2nd one resets the timer instead, prolonging the item's
    // effect. This is implemented in the Player class.
    return;
  }

  mInventory.push_back(type);
}


void PersistentPlayerState::removeItem(const InventoryItemType type)
{
  using namespace std;

  auto iItem = find(begin(mInventory), end(mInventory), type);
  if (iItem != end(mInventory))
  {
    mInventory.erase(iItem);
  }
}


const std::vector<CollectableLetterType>&
  PersistentPlayerState::collectedLetters() const
{
  return mCollectedLetters;
}


PersistentPlayerState::LetterCollectionState
  PersistentPlayerState::addLetter(const CollectableLetterType type)
{
  using L = CollectableLetterType;
  static std::vector<L> sExpectedOrder = {L::N, L::U, L::K, L::E, L::M};


  mCollectedLetters.push_back(type);

  if (mCollectedLetters.size() < 5)
  {
    return LetterCollectionState::Incomplete;
  }
  else
  {
    return mCollectedLetters == sExpectedOrder
      ? LetterCollectionState::InOrder
      : LetterCollectionState::WrongOrder;
  }
}


void PersistentPlayerState::resetForNewLevel()
{
  mHealth = MAX_HEALTH;
  mCollectedLetters.clear();
  mInventory.clear();
}


void PersistentPlayerState::resetHealthAndScore()
{
  mHealth = MAX_HEALTH;
  mScore = 0;
}


TutorialMessageState& PersistentPlayerState::tutorialMessages()
{
  return mTutorialMessages;
}


const TutorialMessageState& PersistentPlayerState::tutorialMessages() const
{
  return mTutorialMessages;
}

} // namespace rigel::data


namespace rigel::data
{

namespace
{

void putU32(std::vector<std::uint8_t>& out, const std::uint32_t value)
{
  for (auto i = 0; i < 4; ++i)
  {
    out.push_back(std::uint8_t((value >> (8 * i)) & 0xFF));
  }
}


bool getU32(
  const std::uint8_t*& pData,
  const std::uint8_t* pEnd,
  std::uint32_t& value)
{
  if (pEnd - pData < 4)
  {
    return false;
  }

  value = 0;
  for (auto i = 0; i < 4; ++i)
  {
    value |= std::uint32_t(pData[i]) << (8 * i);
  }
  pData += 4;
  return true;
}

} // namespace


void PersistentPlayerState::serialize(std::vector<std::uint8_t>& out) const
{
  putU32(out, std::uint32_t(mScore));
  putU32(out, std::uint32_t(mAmmo));
  putU32(out, std::uint32_t(mHealth));
  putU32(out, std::uint32_t(mWeapon));
  putU32(out, std::uint32_t(mTutorialMessages.toBits() & 0xFFFFFFFFu));
  putU32(out, std::uint32_t(mInventory.size()));
  for (const auto item : mInventory)
  {
    putU32(out, std::uint32_t(item));
  }
  putU32(out, std::uint32_t(mCollectedLetters.size()));
  for (const auto letter : mCollectedLetters)
  {
    putU32(out, std::uint32_t(letter));
  }
}


std::optional<PersistentPlayerState> PersistentPlayerState::deserialize(
  const std::uint8_t*& pData,
  const std::uint8_t* pEnd)
{
  PersistentPlayerState result;

  std::uint32_t score, ammo, health, weapon, tutorialBits, numItems, numLetters;
  if (
    !getU32(pData, pEnd, score) || !getU32(pData, pEnd, ammo) ||
    !getU32(pData, pEnd, health) || !getU32(pData, pEnd, weapon) ||
    !getU32(pData, pEnd, tutorialBits) || !getU32(pData, pEnd, numItems) ||
    numItems > 16)
  {
    return std::nullopt;
  }

  result.mScore = int(score);
  result.mAmmo = int(ammo);
  result.mHealth = int(health);
  result.mWeapon = static_cast<WeaponType>(weapon);
  result.mTutorialMessages = TutorialMessageState::fromBits(tutorialBits);

  result.mInventory.clear();
  for (auto i = 0u; i < numItems; ++i)
  {
    std::uint32_t item;
    if (!getU32(pData, pEnd, item))
    {
      return std::nullopt;
    }
    result.mInventory.push_back(static_cast<InventoryItemType>(item));
  }

  if (!getU32(pData, pEnd, numLetters) || numLetters > 5)
  {
    return std::nullopt;
  }

  result.mCollectedLetters.clear();
  for (auto i = 0u; i < numLetters; ++i)
  {
    std::uint32_t letter;
    if (!getU32(pData, pEnd, letter))
    {
      return std::nullopt;
    }
    result.mCollectedLetters.push_back(
      static_cast<CollectableLetterType>(letter));
  }

  return result;
}

} // namespace rigel::data
