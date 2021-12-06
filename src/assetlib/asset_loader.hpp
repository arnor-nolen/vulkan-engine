#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace assets {
struct AssetFile {
  char type[4];
  int version;
  std::string json;
  std::vector<char> binaryBlob;
};
enum class CompressionMode : std::uint32_t { None, LZ4 };

auto save_binaryfile(const std::filesystem::path &path, const AssetFile &file)
    -> bool;
auto load_binaryfile(const std::filesystem::path &path, AssetFile &outputFile)
    -> bool;

auto parse_compression(const char *f) -> assets::CompressionMode;

} // namespace assets