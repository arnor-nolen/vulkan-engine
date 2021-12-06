#pragma once
#include "asset_loader.hpp"

namespace assets {
enum class TextureFormat : std::uint32_t { Unknown = 0, RGBA8 };

// struct PageInfo {
//   std::uint32_t width;
//   std::uint32_t height;
//   std::uint32_t compressedSize;
//   std::uint32_t originalSize;
// };

struct TextureInfo {
  std::uint64_t textureSize;
  TextureFormat textureFormat;
  CompressionMode compressionMode;
  std::uint32_t pixelsize[3];
  std::string originalFile;
};

// Parses the texture metadata from an asset file
auto read_texture_info(AssetFile *file) -> TextureInfo;

void unpack_texture(TextureInfo *info, const char *sourceBuffer,
                    size_t sourceSize, char *destination);

auto pack_texture(TextureInfo *info, void *pixelData) -> AssetFile;

} // namespace assets