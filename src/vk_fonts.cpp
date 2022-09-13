#include "vk_fonts.hpp"
#include "utils/logger.hpp"
#include <fstream>
#include <nlohmann/json.hpp>

auto load_file(const std::filesystem::path &path) -> std::vector<char> {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  const std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);
  std::vector<char> buffer(size + 1);
  if (!file.read(buffer.data(), size)) {
    utils::logger.dump(fmt::format("Error when loading font {}", path.string()),
                       spdlog::level::err);
  }
  buffer.back() = '\0';
  return buffer;
}

auto FontInfo::load_from_json(const std::filesystem::path &filename) -> bool {
  auto fileStr = load_file(filename);
  auto parsed = nlohmann::json::parse(fileStr);

  _glyphs.reserve(parsed["glyphs"].size());

  for (auto &&glyph : parsed["glyphs"]) {
    std::optional<Bounds> atlas{};
    std::optional<Bounds> plane{};

    // Assume that if atlasBounds present, planeBounds are also present
    if (glyph.contains("atlasBounds")) {
      auto atlasBounds = glyph["atlasBounds"];
      auto planeBounds = glyph["planeBounds"];
      atlas = {
          atlasBounds["bottom"].get<float>(),
          atlasBounds["left"].get<float>(),
          atlasBounds["right"].get<float>(),
          atlasBounds["top"].get<float>(),
      };
      plane = {
          .bottom = planeBounds["bottom"].get<float>(),
          .left = planeBounds["left"].get<float>(),
          .right = planeBounds["right"].get<float>(),
          .top = planeBounds["top"].get<float>(),
      };
    }

    _glyphs[glyph["unicode"].get<unsigned int>()] = {
        glyph["advance"].get<float>(), std::move(atlas), std::move(plane)};
  }

  return true;
}
