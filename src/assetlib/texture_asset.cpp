#include "texture_asset.hpp"
#include <iostream>
#include <lz4.h>
#include <nlohmann/json.hpp>

auto parse_format(const char *f) -> assets::TextureFormat {

  if (strcmp(f, "RGBA8") == 0) {
    return assets::TextureFormat::RGBA8;
  } else {
    return assets::TextureFormat::Unknown;
  }
}

auto assets::read_texture_info(AssetFile *file) -> TextureInfo {
  TextureInfo info;
  nlohmann::json texture_metadata = nlohmann::json::parse(file->json);

  std::string formatString = texture_metadata["format"];
  info.textureFormat = parse_format(formatString.c_str());

  std::string compressionString = texture_metadata["compression"];
  info.compressionMode = parse_compression(compressionString.c_str());

  info.pixelsize[0] = texture_metadata["width"];
  info.pixelsize[1] = texture_metadata["height"];

  info.textureSize = texture_metadata["buffer_size"];
  info.originalFile = texture_metadata["original_file"];

  return info;
}

void assets::unpack_texture(TextureInfo *info, const char *sourceBuffer,
                            size_t sourceSize, char *destination) {
  if (info->compressionMode == CompressionMode::LZ4) {
    LZ4_decompress_safe(sourceBuffer, destination, static_cast<int>(sourceSize),
                        static_cast<int>(info->textureSize));
  } else {
    memcpy(destination, sourceBuffer, sourceSize);
  }
}

auto assets::pack_texture(TextureInfo *info, void *pixelData) -> AssetFile {
  nlohmann::json texture_metadata;
  texture_metadata["format"] = "RGBA8";
  texture_metadata["width"] = info->pixelsize[0];
  texture_metadata["height"] = info->pixelsize[1];
  texture_metadata["buffer_size"] = info->textureSize;
  texture_metadata["original_file"] = info->originalFile;

  // Core file header
  AssetFile file;
  file.type[0] = 'T';
  file.type[1] = 'E';
  file.type[2] = 'X';
  file.type[3] = 'I';
  file.version = 1;

  // Compress buffer into blob
  texture_metadata["compression"] = "LZ4";
  int compressStaging = LZ4_compressBound(static_cast<int>(info->textureSize));

  file.binaryBlob.resize(compressStaging);
  int compressedSize = LZ4_compress_default(
      (const char *)pixelData, file.binaryBlob.data(),
      static_cast<int>(info->textureSize), compressStaging);
  file.binaryBlob.resize(compressedSize);

  // Store uncompressed
  // file.binaryBlob.resize(info->textureSize);
  // memcpy(file.binaryBlob.data(), pixelData, info->textureSize);
  // texture_metadata["compression"] = "None";

  file.json = texture_metadata.dump();

  return file;
}
