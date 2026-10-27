#include "frontend/duke1_story.hpp"

#include <stdexcept>

int main(int argc, char** argv) {
  if (argc != 3) return 2;
  const int episode = std::stoi(argv[2]) - 1;
  const auto text = duke1::ExeText::load(argv[1], episode);
  if (!text) throw std::runtime_error("episode metadata missing");
  rigel::Duke1Screens screens(argv[1], *text);
  if (!screens.valid()) throw std::runtime_error("episode screen assets missing");
  const auto intro = screens.introPages();
  const auto ending = screens.endingPages();
  if (intro.size() != 5 || ending.size() != (episode == 2 ? 1u : 5u))
    throw std::runtime_error("incorrect episode scene count");
  for (const auto* pages : {&intro, &ending})
    for (const auto& frame : *pages)
      if (frame.size() != 320 * 200 * 4)
        throw std::runtime_error("incorrect scene buffer size");
  if (text->layout().tipCount != 19 || screens.hallwayMessage(-1).size() != 0 ||
      screens.hallwayMessage(19).size() != 0)
    throw std::runtime_error("incorrect tip bounds");
}
