// Generated metadata and instruction-pattern fixtures, containing no game data.
#pragma once
#include "assets/duke1_assets.hpp"
#include <algorithm>
#include <string>
#include <vector>
namespace duke1::test
{
inline void
  put(std::vector<std::uint8_t>& image, std::size_t at, unsigned value)
{
  image.at(at) = value & 255;
  image.at(at + 1) = (value >> 8) & 255;
}
inline std::vector<std::uint8_t> fixture(int episode)
{
  constexpr std::size_t ds = 0x100;
  std::vector<std::uint8_t> image(ds + 0x5000);
  image[0] = 0xba;
  put(image, 1, ds / 16);
  image[3] = 0x2e;
  image[0x14] = 0x8e;
  image[0x15] = 0xda;
  std::size_t pool = ds + 0xc80;
  const auto pointer =
    [&](std::size_t table, int index, const std::string& name) {
      put(image, ds + table + std::size_t(index) * 4, pool - ds);
      put(image, ds + table + std::size_t(index) * 4 + 2, ds / 16);
      for (char c : name)
        image.at(pool++) = static_cast<std::uint8_t>(c);
      image.at(pool++) = 0;
    };
  const std::vector<std::string> sequence{
    "WORLDAL1", "WORLDAL2", "WORLDAL3", "WORLDAL2", "WORLDALC"};
  for (std::size_t i = 0; i < sequence.size(); ++i)
    pointer(
      0x3a4, static_cast<int>(i), duke1::asset_name(sequence[i], episode));
  for (int i = 0; i < 6; ++i)
    pointer(0xb47, i, duke1::asset_name("DROP" + std::to_string(i), episode));
  return image;
}
inline std::vector<std::uint8_t> textImage(bool shareware, int episode = 0)
{
  auto image = fixture(episode);
  image.resize(0x16000);
  constexpr std::size_t ui = 0x10000;
  const std::vector<std::uint8_t> signature{
    0xb8,
    20,
    0,
    0x50,
    0x0e,
    0xe8,
    0,
    0,
    0xb8,
    0x10,
    0x17,
    0x50,
    0x0e,
    0xe8,
    0,
    0};
  std::copy(
    signature.begin(),
    signature.end(),
    image.begin() + ui + (shareware ? 0x62e : 0x62c));
  const auto draw =
    [&](std::size_t at, int offset, int column, int row, bool near) {
      std::vector<std::uint8_t> bytes{
        0x1e, 0xb8, 0, 0, 0x50, 0xb8, 0, 0, 0x50, 0xb8, 0, 0, 0x50};
      put(bytes, 2, offset);
      put(bytes, 6, column);
      put(bytes, 10, row);
      if (near)
      {
        bytes.insert(bytes.end(), {0x0e, 0xe8, 0, 0});
        put(bytes, 15, 0x3800 - (at + bytes.size()));
      }
      else
      {
        bytes.insert(bytes.end(), {0x9a, 0, 0, 0, 0});
        put(bytes, 14, 0x3800);
        put(bytes, 16, ui / 16);
      }
      std::copy(bytes.begin(), bytes.end(), image.begin() + ui + at);
      return at + bytes.size();
    };
  std::size_t at = 0x640;
  at = draw(at, 0x3000, 0, 0, false);
  at = draw(at, 0x3029, 0, 2, true);
  at = draw(at, 0x3100, 0, 0, true);
  at = draw(at, 0x3129, 0, 2, false);
  draw(at, 0x3200, 0, 0, false); // instruction page is not an intro page
  at = draw(shareware ? 0x26bf : 0x26d5, 0x3300, 0, 11, false);
  draw(at, 0x3328, 0, 13, true); // intentionally not a fixed 41-byte stride
  at = draw(shareware ? 0x1456 : 0x1454, 0x3400, 3, 11, false);
  draw(at, 0x3419, 3, 12, true);
  at = draw(0x3bd7, 0x3500, 1, 19, true);
  at = draw(at, 0x3519, 1, 20, true);
  draw(at, 0x3532, 1, 21, true);
  at = draw(0x3c38, 0x3600, 11, 19, true);
  draw(at, 0x3619, 11, 20, true);
  for (int page = 0; page < 5; ++page)
  {
    const int column = page == 0 || page == 2 ? 1 : 11;
    at = draw(0x4300 + page * 0x60, 0x3700 + page * 0x80, column, 19, true);
    draw(at, 0x3720 + page * 0x80, column, 20, true);
  }
  return image;
}
} // namespace duke1::test
