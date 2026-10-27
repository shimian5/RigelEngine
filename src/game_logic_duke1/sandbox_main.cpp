#include "core.hpp"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
using namespace rigel::duke1;
namespace
{
void snapshot(const Sandbox& s)
{
  const auto& p = s.player;
  std::cout << "{\"x8\":" << (p.x8 & 65535) << ",\"y_px\":" << (p.y & 65535)
            << ",\"camera_x8\":" << (p.cameraX8 & 65535)
            << ",\"camera_y_words\":" << (p.cameraYWords & 65535)
            << ",\"ground_flag\":" << p.grounded
            << ",\"fall_flag\":" << p.falling
            << ",\"jump_index\":" << p.jumpIndex << ",\"facing\":" << p.facing
            << ",\"jump_lock\":" << p.jumpLatch
            << ",\"cling_state\":" << p.cling
            << ",\"pull_up_state\":" << p.pullUp
            << ",\"enabled\":" << p.verticalScroll
            << ",\"hp\":" << (p.health & 65535)
            << ",\"pending\":" << p.pendingHit << ",\"timer\":" << p.immunity
            << ",\"latch\":" << p.fireLatch << ",\"active_bullets\":";
  int count = 0;
  for (const auto& b : s.bullets)
    count += b.kind != 0;
  std::cout << count << ",\"events\":[";
  for (std::size_t i = 0; i < s.events.size(); ++i)
  {
    if (i)
      std::cout << ',';
    const auto& e = s.events[i];
    std::cout << "{\"routine\":\""
              << (e.kind == Event::Sound          ? "0x9b2"
                    : e.kind == Event::MapContact ? "0x3332"
                                                  : "0x14f5")
              << "\",\"arg\":" << e.value << '}';
  }
  std::cout << "],\"slots\":[";
  for (std::size_t i = 0; i < s.bullets.size(); ++i)
  {
    if (i)
      std::cout << ',';
    const auto& b = s.bullets[i];
    std::cout << "{\"kind\":" << b.kind << ",\"x8\":" << b.x8
              << ",\"map_y_words\":" << b.yWords
              << ",\"new_flag\":" << b.newFlag << '}';
  }
  std::cout << "]}\n";
}
Sensor sensor(const std::string& v)
{
  if (v == "left")
    return Sensor::Left;
  if (v == "right")
    return Sensor::Right;
  if (v == "head")
    return Sensor::Head;
  if (v == "floor")
    return Sensor::Floor;
  throw std::invalid_argument("unknown sensor");
}
void set(PlayerState& p, const std::string& key, int v)
{
  if (key == "x8")
    p.x8 = v;
  else if (key == "y")
    p.y = v;
  else if (key == "cx")
    p.cameraX8 = v;
  else if (key == "cy")
    p.cameraYWords = v;
  else if (key == "ground")
    p.grounded = v;
  else if (key == "fall")
    p.falling = v;
  else if (key == "cling")
    p.cling = v;
  else if (key == "jump_index")
    p.jumpIndex = v;
  else if (key == "facing")
    p.facing = v;
  else if (key == "jump_lock")
    p.jumpLatch = v;
  else if (key == "boost")
    p.boost = v;
  else if (key == "claw")
    p.claw = v;
  else if (key == "pull")
    p.pullUp = v;
  else if (key == "enabled")
    p.verticalScroll = v;
  else if (key == "lock")
    p.cameraLock = v;
  else if (key == "exit")
    p.exitAnimation = v;
  else if (key == "death")
    p.death = v;
  else if (key == "hp")
    p.health = v;
  else if (key == "timer")
    p.immunity = v;
  else if (key == "pending")
    p.pendingHit = v;
  else if (key == "capacity")
    p.capacity = v;
  else if (key == "shot_block")
    p.shotBlock = v;
  else if (key == "fire_lock")
    p.fireLatch = v;
  else if (key == "phase")
    p.phase = v;
  else
    throw std::invalid_argument("unknown state field: " + key);
}
} // namespace
int main(int argc, char** argv)
{
  if (argc == 2 && std::string(argv[1]) == "--help")
  {
    std::cout
      << "DOS-only deterministic sandbox. Reads bounded text commands on stdin; no window/audio.\nreset; set FIELD VALUE; input LEFT RIGHT UP DOWN JUMP FIRE; queries LEFT RIGHT HEAD FLOOR (1=clear); world; fill TILE; cell INDEX TILE; move; late; tick; camera; damage; draw; spawn FACING YWORDS X8; bullets CHECK_WORLD CULL; cull X8 YWORDS; collision SENSOR; clock SPEED CALLBACKS; state\n";
    return 0;
  }
  if (argc != 1)
  {
    std::cerr << "Use --help or pipe a finite command script on stdin\n";
    return 2;
  }
  Sandbox s;
  TickGate gate;
  std::string line;
  std::size_t lineno = 0;
  try
  {
    while (std::getline(std::cin, line))
    {
      ++lineno;
      if (line.empty() || line[0] == '#')
        continue;
      std::istringstream in(line);
      std::string op;
      in >> op;
      auto number = [&]() {
        std::string t;
        if (!(in >> t))
          throw std::invalid_argument("missing integer");
        std::size_t end = 0;
        int v = std::stoi(t, &end, 0);
        if (end != t.size())
          throw std::invalid_argument("invalid integer");
        return v;
      };
      if (op == "reset")
      {
        s = Sandbox();
        gate = TickGate();
      }
      else if (op == "set")
      {
        std::string key;
        in >> key;
        set(s.player, key, number());
      }
      else if (op == "input")
      {
        s.input.left = number();
        s.input.right = number();
        s.input.up = number();
        s.input.down = number();
        s.input.jump = number();
        s.input.fire = number();
      }
      else if (op == "queries")
      {
        std::array<bool, 4> q{};
        for (auto& v : q)
          v = number();
        s.query = [q](Sensor which) {
          return q[static_cast<int>(which)];
        };
      }
      else if (op == "world")
        s.query = {};
      else if (op == "peek")
      {
        int i = number();
        std::cout << "{\"cell\":" << s.world.at(i) << "}\n";
      }
      else if (op == "fill")
      {
        int v = number();
        if (v < 0 || v > 65535)
          throw std::invalid_argument("tile must be uint16");
        s.world.cells.fill(static_cast<std::uint16_t>(v));
      }
      else if (op == "cell")
      {
        int i = number(), v = number();
        if (i < 0 || i >= 11520 || v < 0 || v > 65535)
          throw std::invalid_argument("invalid cell");
        s.world.cells.at(i) = static_cast<std::uint16_t>(v);
      }
      else if (op == "move")
      {
        s.movement();
        snapshot(s);
      }
      else if (op == "late")
      {
        s.lateUpdate();
        snapshot(s);
      }
      else if (op == "tick")
      {
        s.tick();
        snapshot(s);
      }
      else if (op == "camera")
      {
        s.cameraY();
        snapshot(s);
      }
      else if (op == "damage")
      {
        s.damage(false);
        snapshot(s);
      }
      else if (op == "draw")
      {
        int draws = s.drawPlayer();
        std::cout << "{\"after\":" << s.player.immunity
                  << ",\"sprite_quadrants_drawn\":" << draws << "}\n";
      }
      else if (op == "spawn")
      {
        int face = number(), y = number(), x = number();
        s.spawn(face, y, x);
        snapshot(s);
      }
      else if (op == "bullet")
      {
        int slot = number();
        if (slot < 0 || slot >= 4)
          throw std::invalid_argument("invalid bullet slot");
        auto& b = s.bullets.at(slot);
        b.kind = number();
        b.x8 = number();
        b.yWords = number();
        b.newFlag = number();
      }
      else if (op == "bullets")
      {
        int world = number(), cull = number();
        s.moveBullets(world, cull);
        snapshot(s);
      }
      else if (op == "cull")
      {
        int x = number(), y = number();
        std::cout << "{\"culled\":" << (s.culled(x, y) ? "true" : "false")
                  << "}\n";
      }
      else if (op == "collision")
      {
        std::string name;
        in >> name;
        s.events.clear();
        bool clear = s.world.clear(sensor(name), s.player, s.events);
        std::cout << "{\"returns_clear\":" << clear
                  << ",\"cling_state\":" << s.player.cling
                  << ",\"sound_requests\":[";
        for (std::size_t i = 0; i < s.events.size(); ++i)
        {
          if (i)
            std::cout << ',';
          std::cout << s.events[i].value;
        }
        std::cout << "]}\n";
      }
      else if (op == "clock")
      {
        int speed = number(), n = number();
        if (n < 0)
          throw std::invalid_argument("negative tick count");
        TickGate g(speed);
        bool ready = g.callbacks(n);
        std::cout << "{\"ready\":" << ready << ",\"pending\":" << g.pending()
                  << "}\n";
      }
      else if (op == "speed")
        gate = TickGate(number());
      else if (op == "advance")
      {
        int n = number();
        if (n < 0)
          throw std::invalid_argument("negative callbacks");
        if (gate.callbacks(n))
          s.tick();
        snapshot(s);
      }
      else if (op == "state")
        snapshot(s);
      else
        throw std::invalid_argument("unknown command: " + op);
      std::string trailing;
      if (in >> trailing)
        throw std::invalid_argument("unexpected trailing argument");
    }
  }
  catch (const std::exception& e)
  {
    std::cerr << "sandbox line " << lineno << ": " << e.what() << '\n';
    return 2;
  }
  return 0;
}
