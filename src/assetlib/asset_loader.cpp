#include "asset_loader.hpp"
#include <fstream>
#include <cstring>

auto assets::save_binaryfile(const std::filesystem::path &path,
                             const AssetFile &file) -> bool {
  std::ofstream outfile;
  outfile.open(path, std::ios::binary | std::ios::out);
  outfile.write(file.type, 4);
  uint32_t version = file.version;
  // Version
  outfile.write((const char *)&version, sizeof(uint32_t));

  // Json length
  uint32_t length = static_cast<uint32_t>(file.json.size());
  outfile.write((const char *)&length, sizeof(uint32_t));

  // Blob length
  uint32_t bloblength = static_cast<uint32_t>(file.binaryBlob.size());
  outfile.write((const char *)&bloblength, sizeof(uint32_t));

  // Json stream
  outfile.write(file.json.data(), length);
  // Blob data
  outfile.write(file.binaryBlob.data(), file.binaryBlob.size());

  outfile.close();

  return true;
}

auto assets::load_binaryfile(const std::filesystem::path &path,
                             AssetFile &outputFile) -> bool {
  std::ifstream infile;
  infile.open(path, std::ios::binary);

  if (!infile.is_open()) {
    return false;
  }

  // Move file cursor to the beginning
  infile.seekg(0);

  infile.read(outputFile.type, 4);
  infile.read((char *)&outputFile.version, sizeof(uint32_t));

  uint32_t jsonlen = 0;
  infile.read((char *)&jsonlen, sizeof(uint32_t));

  uint32_t bloblen = 0;
  infile.read((char *)&bloblen, sizeof(uint32_t));

  outputFile.json.resize(jsonlen);
  infile.read(outputFile.json.data(), jsonlen);

  outputFile.binaryBlob.resize(bloblen);
  infile.read(outputFile.binaryBlob.data(), bloblen);

  return true;
}

auto assets::parse_compression(const char *f) -> assets::CompressionMode {
  if (strcmp(f, "LZ4") == 0) {
    return assets::CompressionMode::LZ4;
  } else {
    return assets::CompressionMode::None;
  }
}
