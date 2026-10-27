#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <cstddef>
#include <vector>
#include <array>
#include <optional>
namespace duke1 {
  struct Tile {
    unsigned width_bytes{
    }
    , height{
    }
    ;
    std::vector<uint8_t> packed;
    std::vector<uint8_t> rgba() const;
  }
  ;
  struct Image {
    unsigned width{
    }
    ,height{
    }
    ;
    std::vector<uint8_t> rgba;
  }
  ;
  struct Tileset {
    std::vector<uint8_t> header,trailing;
    std::vector<Tile> tiles;
  }
  ;
  struct Level {
    std::vector<uint16_t> cells;
  }
  ;
  struct Screen {
    std::vector<uint8_t> indices;
  }
  ;
  struct Sound {
    uint16_t offset{
    }
    ;
    uint8_t priority{
    }
    ,rate{
    }
    ;
    std::string name;
    std::vector<uint16_t> divisors;
    bool terminated{
    }
    ;
  }
  ;
  struct SoundBank {
    uint16_t declared_size{
    }
    ,count{
    }
    ,unknown{
    }
    ;
    std::vector<Sound> sounds;
    std::vector<std::string> warnings;
  }
  ;
  struct BackdropPreview {
    Image image;
    size_t complete_images{
    }
    ,ignored_partial_bytes{
    }
    ;
    std::string warning;
  }
  ;
  struct Scene {
    Image image;
    std::vector<std::string> warnings;
  }
  ;
  // Episode numbers are zero-based; DOS resource suffixes are .DN1/.DN2/.DN3.
  std::string asset_name(const std::string& stem, int episode);
  int episode_from_asset(const std::string& filename);
  std::vector<uint8_t> read_file(const std::filesystem::path&, const std::string&);
  Tileset parse_tiles(const std::vector<uint8_t>&);
  Tileset parse_backdrop(const std::vector<uint8_t>&);
  BackdropPreview parse_backdrop_compat(const std::vector<uint8_t>&, const std::string& label="DROP");
  Scene render_scene(const std::filesystem::path&, const std::string&, bool compatibility=false, bool flat=false, std::optional<std::array<std::string,2>> context=std::nullopt);
  Level parse_level(const std::vector<uint8_t>&);
  Screen parse_screen(const std::vector<uint8_t>&);
  SoundBank parse_sounds(const std::vector<uint8_t>&);
}
