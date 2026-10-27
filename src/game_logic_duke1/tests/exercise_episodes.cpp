// Headless stress/replay checks against the player's own registered episode
// data.
#include "assets/duke1_exe_text.hpp"
#include "play.hpp"

#include <iostream>
#include <map>
#include <stdexcept>

namespace
{
void check(bool ok)
{
  if (!ok)
    throw std::runtime_error("episode replay divergence");
}

void compare(
  duke1::PlayWorld& first,
  duke1::PlayWorld& second,
  int tick,
  bool wide)
{
  const auto a = first.carry(), b = second.carry();
  check(first.worldX() == second.worldX() && first.worldY() == second.worldY());
  check(
    a.score == b.score && a.health == b.health && a.boost == b.boost &&
    a.claw == b.claw && a.capacity == b.capacity);
  check(
    first.dead() == second.dead() &&
    first.levelComplete() == second.levelComplete());
  check(first.takeSounds() == second.takeSounds());
  if (tick % 12 == 0)
  {
    const double blend = (tick % 36) / 24.0;
    first.setRenderBlend(blend);
    second.setRenderBlend(blend);
    std::vector<std::uint8_t> aPixels, bPixels;
    if (wide)
    {
      const int x = first.worldX() - 240, y = first.worldY() - 110;
      first.renderWorld(
        aPixels, 480, 220, x, y, first.worldX(), first.worldY());
      second.renderWorld(
        bPixels, 480, 220, x, y, second.worldX(), second.worldY());
    }
    else
    {
      first.render(aPixels);
      second.render(bPixels);
    }
    check(aPixels == bPixels);
  }
}

void exercise(
  const std::filesystem::path& data,
  const duke1::ExeText& exe,
  const duke1::ExeText::LevelInfo& level,
  bool wide,
  int col = -1,
  int row = -1)
{
  duke1::PlayWorld first(data, level.filename, &level.backdrops, exe.episode());
  duke1::PlayWorld second(
    data, level.filename, &level.backdrops, exe.episode());
  first.setWide(wide);
  second.setWide(wide);
  if (col >= 0)
  {
    first.teleportToCell(col, row);
    second.teleportToCell(col, row);
  }
  const int ticks = col < 0 ? 300 : 96;
  for (int tick = 0; tick < ticks; ++tick)
  {
    duke1::Input input{};
    input.right = tick % 80 < 40;
    input.left = !input.right;
    input.jump = tick % 24 < 8;
    input.fire = tick % 5 < 3;
    input.up = tick % 32 < 2;
    input.down = tick % 32 == 16;
    first.update(input);
    second.update(input);
    compare(first, second, tick, wide);
  }
}
} // namespace

int main(int argc, char** argv)
{
  try
  {
    if (argc != 3)
      return 2;
    const auto exe = duke1::ExeText::load(argv[1], std::stoi(argv[2]) - 1);
    if (!exe)
      throw std::runtime_error("episode executable unavailable");
    struct Site
    {
      std::size_t level;
      int col, row;
    };
    std::map<std::uint16_t, Site> sites;
    for (std::size_t index = 0; index < exe->levels().size(); ++index)
    {
      const auto& level = exe->levels()[index];
      const auto map =
        duke1::parse_level(duke1::read_file(argv[1], level.filename));
      for (std::size_t cell = 0; cell < map.cells.size(); ++cell)
        if (map.cells[cell] >= 0x3000)
          sites.emplace(
            map.cells[cell], Site{index, int(cell % 128), int(cell / 128)});
      for (bool wide : {false, true})
        exercise(argv[1], *exe, level, wide);
    }
    for (const auto& entry : sites)
    {
      const auto& site = entry.second;
      std::cout << "actor=" << std::hex << entry.first << std::dec
                << " level=" << site.level + 1 << std::endl;
      exercise(
        argv[1],
        *exe,
        exe->levels()[site.level],
        entry.first % 2 != 0,
        site.col > 2 ? site.col - 2 : site.col,
        site.row);
    }
    const auto updates = 2 * (exe->levels().size() * 600 + sites.size() * 96);
    std::cout << "PASS episode=" << exe->episode() + 1
              << " levels=" << exe->levels().size()
              << " distinct_main_map_ids=" << sites.size()
              << " simulated_updates=" << updates << '\n';
  }
  catch (const std::exception& e)
  {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
