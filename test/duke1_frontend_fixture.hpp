// All files here are generated test inputs, never copied game data.
#pragma once

#include "duke1/tests/episode_fixture.hpp"
#include "frontend/duke1_mode.hpp"
#include "frontend/replay_save.hpp"
#include "frontend/user_profile.hpp"
#include "utils.hpp"

#include <chrono>
#include <fstream>
#include <stdexcept>

namespace rigel::test
{
inline void writeBytes(
  const std::filesystem::path& path,
  const std::vector<std::uint8_t>& bytes)
{
  std::ofstream out(path, std::ios::binary);
  out.write(
    reinterpret_cast<const char*>(bytes.data()),
    static_cast<std::streamsize>(bytes.size()));
  if (!out)
    throw std::runtime_error("cannot write generated DN1 fixture");
}

// Literal-only LZ91 stream, sufficient to exercise the real EXE file loader.
inline std::vector<std::uint8_t>
  packImage(const std::vector<std::uint8_t>& image)
{
  std::vector<std::uint8_t> stream(2);
  std::size_t control = 0;
  int used = 0;
  auto bit = [&](bool value) {
    if (value)
      stream[control + static_cast<std::size_t>(used / 8)] |=
        static_cast<std::uint8_t>(1u << (used % 8));
    if (++used == 16)
    {
      used = 0;
      control = stream.size();
      stream.resize(stream.size() + 2);
    }
  };
  for (auto byte : image)
  {
    bit(true);
    stream.push_back(byte);
  }
  bit(false);
  bit(true);
  stream.insert(stream.end(), {0, 0, 0}); // long token, zero length: end
  constexpr std::size_t header = 64;
  const auto stub = (header + stream.size() + 15) / 16 * 16;
  std::vector<std::uint8_t> exe(stub + 16);
  exe[0] = 'M';
  exe[1] = 'Z';
  duke1::test::put(exe, 8, header / 16);
  duke1::test::put(exe, 0x16, (stub - header) / 16);
  duke1::test::put(exe, 0x18, 0x1c);
  std::copy_n("LZ91", 4, exe.begin() + 0x1c);
  duke1::test::put(exe, stub + 8, (stub - header) / 16);
  std::copy(stream.begin(), stream.end(), exe.begin() + header);
  return exe;
}

class Duke1Fixture
{
public:
  Duke1Fixture()
    : mOldDirectory(std::filesystem::current_path())
  {
    const auto nonce =
      std::chrono::steady_clock::now().time_since_epoch().count();
    mDirectory = std::filesystem::temp_directory_path() /
      ("rigel-dn1-flow-" + std::to_string(nonce));
    if (!std::filesystem::create_directory(mDirectory))
      throw std::runtime_error("cannot create isolated DN1 fixture directory");
    mDirectory = std::filesystem::canonical(mDirectory);
    writeBytes(mDirectory / "portable.txt", {});
    std::filesystem::current_path(mDirectory);
    const auto prefs = createOrGetPreferencesPath();
    if (!prefs || *prefs != mDirectory / "userdata")
      throw std::runtime_error("DN1 tests require isolated portable saves");
    mData = mDirectory / "data";
    std::filesystem::create_directory(mData);
    mProfile.mOptions.mWidescreenModeOn = false;
    for (int episode = 0; episode < 3; ++episode)
      makeEpisode(episode);
  }
  ~Duke1Fixture()
  {
    std::error_code error;
    std::filesystem::current_path(mOldDirectory, error);
    std::filesystem::remove_all(mDirectory, error);
  }
  Duke1Fixture(const Duke1Fixture&) = delete;
  Duke1Fixture& operator=(const Duke1Fixture&) = delete;

  GameMode::Context context()
  {
    GameMode::Context context{};
    context.mpServiceProvider = &mServices;
    context.mpUserProfile = &mProfile;
    return context;
  }
  void writeSave(const std::vector<std::uint8_t>& bytes)
  {
    const auto path = saveFilePath("flow_test");
    if (!path || path->parent_path() != mDirectory / "userdata" / "saves")
      throw std::runtime_error("save path escaped the fixture");
    writeBytes(*path, bytes);
  }
  std::filesystem::path mData;

private:
  void makeEpisode(int episode)
  {
    auto image = duke1::test::textImage(false, episode);
    // Move the small resource-name pool away from the generated tip table.
    // Its original names occupy less than 256 bytes.
    std::copy_n(image.begin() + 0xd80, 256, image.begin() + 0x1900);
    for (auto table : {0x3a4, 0xb47})
      for (int i = 0; i < (table == 0x3a4 ? 5 : 6); ++i)
      {
        const auto pointer = static_cast<std::size_t>(0x100 + table + i * 4);
        const auto old =
          unsigned(image[pointer]) | (unsigned(image[pointer + 1]) << 8);
        duke1::test::put(image, pointer, old + 0xb80);
      }
    for (int i = 0; i < 19 * 4; ++i)
    {
      duke1::test::put(image, 0x100 + 0xba1 + std::size_t(i) * 4, 0x1a00);
      duke1::test::put(image, 0x100 + 0xba3 + std::size_t(i) * 4, 0x10);
    }
    const std::string tip = "GENERATED TEST TIP";
    std::copy(tip.begin(), tip.end(), image.begin() + 0x1b00);
    writeBytes(
      mData / ("DN" + std::to_string(episode + 1) + ".EXE"), packImage(image));

    auto asset =
      [&](const std::string& stem, const std::vector<std::uint8_t>& bytes) {
        writeBytes(mData / duke1::asset_name(stem, episode), bytes);
      };
    std::vector<std::uint8_t> tiles(3 + 50 * 160);
    tiles[0] = 50;
    tiles[1] = 2;
    tiles[2] = 16;
    for (const auto* stem : {"BACK", "SOLID", "ANIM", "OBJECT", "MAN"})
    {
      const int count = std::string(stem) == "ANIM" ? 6
        : std::string(stem) == "MAN"                ? 5
        : std::string(stem) == "OBJECT"             ? 3
                                                    : 4;
      for (int i = 0; i < count; ++i)
        asset(std::string(stem) + std::to_string(i), tiles);
    }
    for (const auto* stem : {"NUMBERS", "BORDER", "FONT1", "FONT2"})
      asset(stem, tiles);
    for (int i = 0; i < 6; ++i)
      asset("DROP" + std::to_string(i), std::vector<std::uint8_t>(20803));
    for (const auto* stem : {"DN", "DUKE", "BADGUY", "END"})
      asset(stem, std::vector<std::uint8_t>(32000));
    for (int level = 0; level < 2; ++level)
    {
      std::vector<std::uint8_t> map(128 * 90 * 2);
      for (int col = 0; col < 128; ++col)
        duke1::test::put(map, std::size_t(6 * 128 + col) * 2, 0x1820);
      duke1::test::put(
        map, std::size_t(5 * 128 + 8 + level * 4 + episode) * 2, 0x3032);
      asset(level ? "WORLDAL3" : "WORLDAL1", map);
    }
  }
  std::filesystem::path mDirectory, mOldDirectory;
  UserProfile mProfile;
  MockServiceProvider mServices;
};
} // namespace rigel::test
