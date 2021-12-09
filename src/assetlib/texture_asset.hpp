#pragma once
#include "asset_loader.hpp"

namespace assets {
enum class TextureFormat : uint32_t { Unknown = 0, RGBA8 };

// struct PageInfo {
//   uint32_t width;
//   uint32_t height;
//   uint32_t compressedSize;
//   uint32_t originalSize;
// };

struct TextureInfo {
  std::uint64_t textureSize;
  TextureFormat textureFormat;
  CompressionMode compressionMode;
  uint32_t pixelsize[3];
  std::string originalFile;
};

// Parses the texture metadata from an asset file
auto read_texture_info(AssetFile *file) -> TextureInfo;

void unpack_texture(TextureInfo *info, const char *sourceBuffer,
                    size_t sourceSize, char *destination);

auto pack_texture(TextureInfo *info, void *pixelData) -> AssetFile;

} // namespace assets