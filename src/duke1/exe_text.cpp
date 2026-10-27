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

#include "exe_text.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>

namespace duke1 {

namespace {

constexpr int DATA_SEGMENT_BASE = 0x17e10;  // load image offset of the data segment in DN1 1.0
constexpr int HALLWAY_TABLE = 0xba1;        // data segment offset of the hallway message table

std::uint16_t word(const std::vector<std::uint8_t>& d, const std::size_t at) {
  return static_cast<std::uint16_t>(d[at] | (d[at + 1] << 8));
}

// LZEXE's bit reader: bits come least significant first from 16 bit words
struct Bits {
  const std::vector<std::uint8_t>& data;
  std::size_t pos;
  std::uint16_t buffer = 0;
  int count = 16;

  Bits(const std::vector<std::uint8_t>& d, const std::size_t start) : data(d), pos(start) { refill(); }
  void refill() {
    if (pos + 1 < data.size()) buffer = word(data, pos);
    pos += 2;
  }
  int bit() {
    const int b = buffer & 1;
    buffer >>= 1;
    if (--count == 0) {
      refill();
      count = 16;
    }
    return b;
  }
  int byte() { return pos < data.size() ? data[pos++] : 0; }
};

}  // namespace

std::optional<std::vector<std::uint8_t>> unpackLzexe91(const std::vector<std::uint8_t>& exe) {
  if (exe.size() < 0x40 || exe[0] != 'M' || exe[1] != 'Z') return std::nullopt;
  const std::size_t headerParas = word(exe, 8);
  const std::size_t initialCs = word(exe, 0x16);
  if (word(exe, 0x18) != 0x1c || std::memcmp(&exe[0x1c], "LZ91", 4) != 0) return std::nullopt;

  // The packer's stub sits at the entry point; its first words describe the compressed block
  const std::size_t stub = (headerParas + initialCs) << 4;
  if (stub + 16 > exe.size()) return std::nullopt;
  const std::size_t compressedParas = word(exe, stub + 8);
  const std::size_t start = (headerParas + initialCs - compressedParas) << 4;
  if (start >= exe.size()) return std::nullopt;

  std::vector<std::uint8_t> out;
  out.reserve(0x30000);
  Bits bits{exe, start};
  for (;;) {
    if (bits.pos > exe.size() + 4 || out.size() > 0x100000) return std::nullopt;
    if (bits.bit()) {
      out.push_back(static_cast<std::uint8_t>(bits.byte()));
      continue;
    }
    int length;
    int span;
    if (!bits.bit()) {
      length = bits.bit() << 1;
      length |= bits.bit();
      length += 2;
      span = bits.byte() | 0xff00;
    } else {
      span = bits.byte();
      length = bits.byte();
      span |= ((length & ~0x07) << 5) | 0xe000;
      length = (length & 0x07) + 2;
      if (length == 2) {
        length = bits.byte();
        if (length == 0) break;      // end of the packed module
        if (length == 1) continue;   // segment change: nothing to do for a flat image
        ++length;
      }
    }
    const auto distance = static_cast<int>(static_cast<std::int16_t>(span));
    for (; length > 0; --length) {
      if (static_cast<int>(out.size()) + distance < 0) return std::nullopt;
      out.push_back(out[static_cast<std::size_t>(static_cast<int>(out.size()) + distance)]);
    }
  }
  return out;
}

std::optional<ExeText> ExeText::load(const std::filesystem::path& dataDir) {
  std::ifstream file(dataDir / "DN1.EXE", std::ios::binary);
  if (!file) return std::nullopt;
  const std::vector<std::uint8_t> exe{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
  auto image = unpackLzexe91(exe);
  if (!image) return std::nullopt;
  ExeText text;
  text.image_ = std::move(*image);
  if (!text.plausible()) return std::nullopt;
  return text;
}

bool ExeText::plausible() const {
  return image_.size() > static_cast<std::size_t>(DATA_SEGMENT_BASE + 0x4c00);
}

std::string ExeText::at(const int dsOffset) const {
  const auto start = static_cast<std::size_t>(DATA_SEGMENT_BASE + dsOffset);
  if (dsOffset < 0 || start >= image_.size()) return {};
  std::string s;
  for (std::size_t i = start; i < image_.size() && image_[i] != 0 && s.size() < 200; ++i)
    s.push_back(static_cast<char>(image_[i]));
  return s;
}

std::vector<std::string> ExeText::rows(const int dsOffset, const int count, const int stride) const {
  std::vector<std::string> out;
  for (int i = 0; i < count; ++i) {
    auto row = at(dsOffset + i * stride);
    while (!row.empty() && row.back() == ' ') row.pop_back();
    out.push_back(std::move(row));
  }
  return out;
}

std::vector<std::string> ExeText::hallwayMessage(const int index) const {
  std::vector<std::string> lines;
  const auto entry = static_cast<std::size_t>(DATA_SEGMENT_BASE + HALLWAY_TABLE + index * 16);
  if (index < 0 || entry + 16 > image_.size()) return lines;
  for (int i = 0; i < 4; ++i) {
    const int offset = word(image_, entry + static_cast<std::size_t>(i) * 4);
    auto line = at(offset);
    while (!line.empty() && line.back() == ' ') line.pop_back();
    lines.push_back(std::move(line));
  }
  // Trailing empty rows are not part of the message
  while (!lines.empty() && lines.back().empty()) lines.pop_back();
  return lines;
}

}  // namespace duke1
