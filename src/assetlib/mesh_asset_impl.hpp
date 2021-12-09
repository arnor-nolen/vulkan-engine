#pragma once

#include "mesh_asset.hpp"

template <typename V>
auto assets::pack_mesh(MeshInfo *info, V *vertexData, uint32_t *indexData)
    -> AssetFile {
  AssetFile file;
  file.type[0] = 'M';
  file.type[1] = 'E';
  file.type[2] = 'S';
  file.type[3] = 'H';
  file.version = 1;

  nlohmann::json metadata;
  if (info->vertexFormat == VertexFormat::P32N8C8V16) {
    metadata["vertex_format"] = "P32N8C8V16";
  } else if (info->vertexFormat == VertexFormat::PNCV_F32) {
    metadata["vertex_format"] = "PNCV_F32";
  }

  metadata["vertex_buffer_size"] = info->vertexBufferSize;
  metadata["index_buffer_size"] = info->indexBufferSize;
  metadata["index_size"] = info->indexSize;
  metadata["original_file"] = info->originalFile;

  std::vector<float> boundsData;
  boundsData.resize(7);

  boundsData[0] = info->bounds.origin[0];
  boundsData[1] = info->bounds.origin[1];
  boundsData[2] = info->bounds.origin[2];

  boundsData[3] = info->bounds.radius;

  boundsData[4] = info->bounds.extents[0];
  boundsData[5] = info->bounds.extents[1];
  boundsData[6] = info->bounds.extents[2];

  metadata["bounds"] = boundsData;

  size_t fullsize = info->vertexBufferSize + info->indexBufferSize;

  std::vector<char> merged_buffer;
  merged_buffer.resize(fullsize);

  // Copy vertex buffer
  memcpy(merged_buffer.data(), vertexData, info->vertexBufferSize);

  // Copy index buffer
  memcpy(merged_buffer.data() + info->vertexBufferSize, indexData,
         info->indexBufferSize);

  // Compress buffer and copy it into the file struct
  size_t compressStaging = LZ4_compressBound(static_cast<int>(fullsize));
  file.binaryBlob.resize(compressStaging);

  int compressedSize =
      LZ4_compress_default(merged_buffer.data(), file.binaryBlob.data(),
                           static_cast<int>(merged_buffer.size()),
                           static_cast<int>(compressStaging));
  file.binaryBlob.resize(compressedSize);

  metadata["compression"] = "LZ4";
  file.json = metadata.dump();

  return file;
}