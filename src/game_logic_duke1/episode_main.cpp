#include "assets/duke1_assets.hpp"
#include "assets/duke1_exe_text.hpp"
#include "play.hpp"

#include <iostream>
#include <set>
#include <stdexcept>

int main(int argc, char** argv)
{
  try
  {
    if (argc < 3 || argc > 4)
    {
      std::cerr << "usage: dn1_episode_info DATA EPISODE [--worlds]\n";
      return 2;
    }
    const int episode = std::stoi(argv[2]) - 1;
    const auto executable = duke1::ExeText::load(argv[1], episode);
    if (!executable)
      throw std::runtime_error("episode executable metadata is not supported");
    std::set<std::uint16_t> ids;
    std::cout << "intro_rows=";
    for (const auto& page : executable->layout().introPages)
      std::cout << page.size() << ",";
    std::cout << " dialogue_rows=";
    for (const auto& block : executable->layout().dialogue)
      std::cout << block.size() << ",";
    std::cout << " ending_rows=";
    for (const auto& page : executable->layout().ending)
      std::cout << page.lines.size() << ",";
    std::cout << " tip_groups=" << executable->layout().tipCount << "\n";
    std::cout << "episode=" << episode + 1
              << " levels=" << executable->levels().size()
              << " sequence_entries=" << executable->sequence().size() << '\n';
    for (const auto& stage : executable->sequence())
    {
      const auto level =
        duke1::parse_level(duke1::read_file(argv[1], stage.filename));
      for (auto id : level.cells)
        if (id >= 0x3000)
          ids.insert(id);
      for (const auto& backdrop : stage.backdrops)
      {
        const auto parsed = duke1::parse_backdrop_compat(
          duke1::read_file(argv[1], backdrop), backdrop);
        if (!parsed.warning.empty())
          std::cerr << parsed.warning << '\n';
      }
    }
    for (const auto& stage : executable->levels())
    {
      std::cout << stage.filename << " " << stage.backdrops[0] << " "
                << stage.backdrops[1] << " sequence=" << stage.sequenceIndex
                << '\n';
      if (argc == 4)
      {
        if (std::string(argv[3]) != "--worlds")
          throw std::runtime_error("unknown option");
        duke1::PlayWorld world(
          argv[1], stage.filename, &stage.backdrops, episode);
        for (int i = 0; i < 4; ++i)
          world.update({});
        std::vector<std::uint8_t> rgba;
        world.render(rgba);
        if (rgba.size() != duke1::SCREEN_W * duke1::SCREEN_H * 4)
          throw std::runtime_error("incorrect world frame size");
      }
    }
    std::cout << "active_ids=" << ids.size() << '\n';
    return 0;
  }
  catch (const std::exception& e)
  {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
