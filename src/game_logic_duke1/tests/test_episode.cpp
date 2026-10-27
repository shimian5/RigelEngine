#include "assets/duke1_assets.hpp"
#include "assets/duke1_exe_text.hpp"
#include "episode_fixture.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
using duke1::test::fixture;
using duke1::test::put;
void check(bool ok)
{
  if (!ok)
    throw std::runtime_error("episode metadata assertion failed");
}
void textFixture(bool shareware)
{
  auto image = duke1::test::textImage(shareware);
  const auto executable = duke1::ExeText::fromImage(std::move(image), 0);
  check(executable.has_value());
  const auto& layout = executable->layout();
  check(layout.introPages.size() == 2);
  check(layout.introPages[0].size() == 2);
  check(layout.introPages[1].size() == 4);
  check(layout.introPages[1][3].offset == 0x3328);
  check(layout.introPages[1][3].row == 13);
  check(layout.dialogue[0].size() == 3);
  check(layout.dialogue[2].size() == 2);
  check(layout.ending.size() == 5);
  check(!layout.ending[0].player);
  check(layout.ending.back().player);
  check(layout.ending.back().lines.back().offset == 0x3920);
}

} // namespace

int main()
{
  textFixture(false);
  textFixture(true);
  for (int episode = 0; episode < 3; ++episode)
  {
    const auto image = fixture(episode);
    const auto executable = duke1::ExeText::fromImage(image, episode);
    check(executable.has_value());
    check(executable->levels().size() == 2);
    check(executable->sequence().size() == 5);
    check(executable->levels()[1].intermissionMessage == 3);
    check(
      executable->levels()[1].filename ==
      duke1::asset_name("WORLDAL3", episode));
    check(
      executable->levels()[1].backdrops[0] ==
      duke1::asset_name("DROP2", episode));
    check(!executable->backdropPair("WORLDAL2"));
    check(
      executable->backdropPair("WORLDALC")->at(0) ==
      duke1::asset_name("DROP4", episode));
    check(!duke1::ExeText::fromImage(image, (episode + 1) % 3));
    auto broken = image;
    broken.at(1) = 0xff;
    broken.at(2) = 0xff;
    check(!duke1::ExeText::fromImage(std::move(broken), episode));
    broken = image;
    put(broken, 0x100 + 0xb47 + 2, 0xffff);
    check(!duke1::ExeText::fromImage(std::move(broken), episode));
    check(
      duke1::episode_from_asset("worldal1.dn" + std::to_string(episode + 1)) ==
      episode);
  }
  check(!duke1::ExeText::fromImage({}, 0));
  check(!duke1::ExeText::fromImage(fixture(0), 3));
  return 0;
}
