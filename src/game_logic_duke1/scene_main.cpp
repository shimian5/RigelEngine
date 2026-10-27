#include "assets/duke1_assets.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <iostream>
using namespace rigel::duke1;
static void
  writebytes(const std::filesystem::path& p, const std::vector<uint8_t>& b)
{
  std::ofstream f(p, std::ios::binary);
  f.write((const char*)b.data(), b.size());
}
static void inspect_assets(
  const std::filesystem::path& root,
  const std::filesystem::path& out)
{
  std::filesystem::create_directories(out);
  std::ofstream j(out / "manifest.json");
  j << "{\"graphics\":[";
  bool comma = false;
  auto item = [&](
                const std::string& kind,
                const std::string& source,
                const std::string& file,
                unsigned w,
                unsigned h) {
    if (comma)
      j << ",";
    comma = true;
    j << "{\"kind\":\"" << kind << "\",\"source\":\"" << source
      << "\",\"file\":\"" << file << "\",\"width\":" << w << ",\"height\":" << h
      << "}";
  };
  std::vector<std::string> sources;
  for (int n = 0; n < 4; n++)
  {
    sources.push_back("BACK" + std::to_string(n) + ".DN1");
    sources.push_back("SOLID" + std::to_string(n) + ".DN1");
  }
  for (int n = 0; n < 3; n++)
    sources.push_back("OBJECT" + std::to_string(n) + ".DN1");
  for (int n = 0; n < 6; n++)
    sources.push_back("ANIM" + std::to_string(n) + ".DN1");
  for (int n = 0; n < 5; n++)
    sources.push_back("MAN" + std::to_string(n) + ".DN1");
  for (auto n : {"BORDER.DN1", "NUMBERS.DN1", "FONT1.DN1", "FONT2.DN1"})
    sources.push_back(n);
  for (auto& name : sources)
  {
    auto set = parse_tiles(read_file(root, name));
    for (size_t k = 0; k < set.tiles.size(); k++)
    {
      auto file =
        name.substr(0, name.find('.')) + "_" + std::to_string(k) + ".rgba";
      auto px = set.tiles[k].rgba();
      writebytes(out / file, px);
      item(
        "tile", name, file, set.tiles[k].width_bytes * 8, set.tiles[k].height);
    }
  }
  for (int n = 0; n < 14; n++)
  {
    auto name = "DROP" + std::to_string(n) + ".DN1";
    try
    {
      auto bytes = read_file(root, name);
      auto p = parse_backdrop_compat(bytes, "DROP" + std::to_string(n));
      auto file = "DROP" + std::to_string(n) + ".rgba";
      writebytes(out / file, p.image.rgba);
      item("backdrop", name, file, 208, 160);
    }
    catch (const std::runtime_error& e)
    {
      if (std::string(e.what()) != "file not found")
        throw;
    }
  }
  j << "],\"screens\":[";
  comma = false;
  for (auto name :
       {"BADGUY.DN1", "CREDITS.DN1", "DN.DN1", "DUKE.DN1", "END.DN1"})
  {
    auto screen = parse_screen(read_file(root, name));
    auto file =
      std::string(name).substr(0, std::string(name).find('.')) + ".idx";
    writebytes(out / file, screen.indices);
    if (comma)
      j << ",";
    comma = true;
    j << "{\"source\":\"" << name << "\",\"file\":\"" << file
      << "\",\"width\":320,\"height\":200}";
  }
  j << "],\"sounds\":[";
  comma = false;
  for (auto name : {"DUKE1.DN1", "DUKE1-B.DN1"})
  {
    auto b = parse_sounds(read_file(root, name));
    if (comma)
      j << ",";
    comma = true;
    j << "{\"source\":\"" << name << "\",\"declared_size\":" << b.declared_size
      << ",\"count\":" << b.count << ",\"unknown\":" << b.unknown
      << ",\"sounds\":[";
    for (size_t i = 0; i < b.sounds.size(); i++)
    {
      if (i)
        j << ",";
      auto& x = b.sounds[i];
      j << "{\"offset\":" << x.offset
        << ",\"priority\":" << unsigned(x.priority)
        << ",\"rate\":" << unsigned(x.rate) << ",\"name\":\"" << x.name
        << "\",\"terminated\":" << (x.terminated ? "true" : "false")
        << ",\"divisors\":[";
      for (size_t k = 0; k < x.divisors.size(); k++)
      {
        if (k)
          j << ",";
        j << x.divisors[k];
      }
      j << "]}";
    }
    j << "]}";
  }
  j << "]}\n";
}
int main(int ac, char** av)
{
  try
  {
    if (ac == 4 && std::string(av[1]) == "--inspect")
    {
      inspect_assets(av[2], av[3]);
      return 0;
    }
    if (ac < 4)
    {
      std::cerr
        << "usage: dn1_scene DATA LEVEL OUTPUT.ppm [--flat] [--compat] [--context DROPx.DN1,DROPy.DN1] | --inspect DATA OUT_DIR\n";
      return 2;
    }
    std::filesystem::path root = av[1];
    std::string level = av[2], out = av[3];
    bool flat = false, compat = false;
    std::optional<std::array<std::string, 2>> context;
    for (int i = 4; i < ac; i++)
    {
      std::string a = av[i];
      if (a == "--flat")
        flat = true;
      else if (a == "--compat")
        compat = true;
      else if (a == "--context" && i + 1 < ac)
      {
        std::string v = av[++i];
        auto k = v.find(',');
        if (k == v.npos)
          throw std::runtime_error("context requires two backdrops");
        context = std::array<std::string, 2>{v.substr(0, k), v.substr(k + 1)};
      }
      else
        throw std::runtime_error("unknown option");
    }
    auto scene = render_scene(root, level, compat, flat, context);
    std::ofstream ppm(out, std::ios::binary);
    ppm << "P6\n"
        << scene.image.width << " " << scene.image.height << "\n255\n";
    for (size_t i = 0; i < scene.image.rgba.size(); i += 4)
      ppm.write((char*)&scene.image.rgba[i], 3);
    std::ofstream raw(out + ".rgba", std::ios::binary);
    raw.write((char*)scene.image.rgba.data(), scene.image.rgba.size());
    std::ofstream meta(out + ".meta");
    meta << "width=" << scene.image.width << "\nheight=" << scene.image.height
         << "\nlevel=" << level << "\n";
    for (auto& w : scene.warnings)
    {
      meta << "warning=" << w << "\n";
      std::cerr << w << "\n";
    }
    return 0;
  }
  catch (const std::exception& e)
  {
    std::cerr << "dn1_scene: " << e.what() << "\n";
    return 1;
  }
}
