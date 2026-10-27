// Dev tool: render a frame of the playable world to a PPM after N updates with a repeated input.
// usage: dn1_play_shot DATA_DIR LEVEL OUT.ppm [updates] [left|right|jump|fire ...]
#include "play.hpp"

#include <cstdio>
#include <array>
#include <cstdlib>
#include <cstring>
#include <string>

int main(int argc, char** argv) {
  if (argc < 4) { std::fprintf(stderr, "usage: %s DATA_DIR LEVEL OUT.ppm [updates] [keys...]\n", argv[0]); return 2; }
  std::array<std::string, 2> ctx{};
  bool haveCtx = false;
  if (const char* c = std::getenv("DN1_CONTEXT")) {
    std::string v = c;
    const auto comma = v.find(',');
    ctx = {v.substr(0, comma), v.substr(comma + 1)};
    haveCtx = true;
  }
  duke1::PlayWorld world(argv[1], argv[2], haveCtx ? &ctx : nullptr);
  for (const auto& w : world.warnings()) std::fprintf(stderr, "warning: %s\n", w.c_str());
  // Arguments after the update count are held keys (left right jump fire), or "KEYS:N" phases such as
  // "left:20" which hold the key for N updates before the next phase.
  if (const char* tp = std::getenv("DN1_TELEPORT")) {
    int c = 0, r = 0;
    std::sscanf(tp, "%d,%d", &c, &r);
    world.teleportToCell(c, r);
  }
  const int n = argc > 4 ? std::atoi(argv[4]) : 0;
  duke1::Input in;
  bool phases = false;
  for (int i = 5; i < argc; ++i) {
    std::string a = argv[i];
    const auto colon = a.find(':');
    if (colon == std::string::npos) {
      if (a == "left") in.left = true;
      if (a == "right") in.right = true;
      if (a == "jump") in.jump = true;
      if (a == "fire") in.fire = true;
    } else {
      phases = true;
    }
  }
  for (int i = 0; i < n; ++i) world.update(in);
  if (phases) {
    for (int i = 5; i < argc; ++i) {
      std::string a = argv[i];
      const auto colon = a.find(':');
      if (colon == std::string::npos) continue;
      duke1::Input p;
      const auto key = a.substr(0, colon);
      p.left = key == "left"; p.right = key == "right"; p.jump = key == "jump"; p.fire = key == "fire"; p.up = key == "up"; p.down = key == "down";
      const int count = std::atoi(a.c_str() + colon + 1);
      for (int k = 0; k < count; ++k) {
        world.update(p);
        const auto& pl = world.player();
        std::printf("  %s step %d: x8=%d facing=%d walk=%d\n", key.c_str(), k, pl.x8 + pl.cameraX8, pl.facing, pl.walkFrame);
      }
    }
  }
  world.debugDump();
  std::vector<std::uint8_t> rgba;
  world.render(rgba);
  FILE* f = std::fopen(argv[3], "wb");
  std::fprintf(f, "P6\n%d %d\n255\n", duke1::SCREEN_W, duke1::SCREEN_H);
  for (std::size_t i = 0; i < rgba.size(); i += 4) std::fwrite(&rgba[i], 1, 3, f);
  std::fclose(f);
  const auto& p = world.player();
  std::printf("x8=%d y=%d camX8=%d camYW=%d facing=%d health=%d\n", p.x8, p.y, p.cameraX8, p.cameraYWords, p.facing, p.health);
}
