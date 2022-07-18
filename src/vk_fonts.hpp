#pragma once
#include <filesystem>
#include <optional>
#include <unordered_map>

struct Bounds {
  float bottom;
  float left;
  float right;
  float top;
};

struct Glyph {
  float advance;
  std::optional<Bounds> atlasBounds;
  std::optional<Bounds> planeBounds;
};

struct FontInfo {
  std::unordered_map<unsigned int, Glyph> _glyphs;

  auto load_from_json(const std::filesystem::path &filename) -> bool;
};