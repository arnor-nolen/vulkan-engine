#include "mesh_asset.hpp"

auto assets::parse_format(const char *f) -> VertexFormat {
  if (strcmp(f, "PNCV_F32") == 0) {
    return assets::VertexFormat::PNCV_F32;
  }
  if (strcmp(f, "P32N8C8V16") == 0) {
    return assets::VertexFormat::P32N8C8V16;
  }
  return assets::VertexFormat::Unknown;
}

auto assets::read_mesh_info(AssetFile *file) -> MeshInfo {
  nlohmann::json metadata = nlohmann::json::parse(file->json);

  std::string compressionString = metadata["compression"];
  std::string vertexFormat = metadata["vertex_format"];

  MeshInfo info = {.vertexBufferSize = metadata["vertex_buffer_size"],
                   .indexBufferSize = metadata["index_buffer_size"],
                   .vertexFormat = parse_format(vertexFormat.c_str()),
                   .indexSize = static_cast<int8_t>(metadata["index_size"]),
                   .compressionMode =
                       parse_compression(compressionString.c_str()),
                   .originalFile = metadata["original_file"]};

  auto boundsData = metadata["bounds"].get<std::vector<float>>();

  for (size_t i = 0; i != 3; ++i) {
    info.bounds.origin[i] = boundsData.at(i);
    info.bounds.extents[i] = boundsData.at(4 + i);
  }

  info.bounds.radius = boundsData.at(3);

  return info;
}

void assets::unpack_mesh(MeshInfo *info, const char *sourceBuffer,
                         size_t sourceSize, char *vertexBuffer,
                         char *indexBuffer) {
  // Decompression into temporal vector. TODO: streaming decompress directly
  // on the buffers
  std::vector<char> decompressedBuffer;
  decompressedBuffer.resize(info->vertexBufferSize + info->indexBufferSize);

  LZ4_decompress_safe(sourceBuffer, decompressedBuffer.data(),
                      static_cast<int>(sourceSize),
                      static_cast<int>(decompressedBuffer.size()));

  // Copy vertex buffer
  memcpy(vertexBuffer, decompressedBuffer.data(), info->vertexBufferSize);

  // Copy index buffer
  memcpy(indexBuffer, &decompressedBuffer[info->vertexBufferSize],
         info->indexBufferSize);
}

auto assets::calcualate_bounds(Vertex_f32_PNCV *vertices, size_t count)
    -> assets::MeshBounds {
  auto max_float = std::numeric_limits<float>::max();
  auto min_float = std::numeric_limits<float>::min();

  auto min = std::array<float, 3>{max_float, max_float, max_float};
  auto max = std::array<float, 3>{min_float, min_float, min_float};

  for (size_t i = 0; i != count; ++i) {
    for (size_t j = 0; j != 3; ++j) {
      min.at(j) = std::min(min.at(j), vertices[i].position[j]);
      max.at(j) = std::max(max.at(j), vertices[i].position[j]);
    }
  }

  MeshBounds bounds;
  for (size_t i = 0; i != 3; ++i) {
    bounds.extents[i] = (max.at(i) - min.at(i)) / 2.0F;
    bounds.origin[i] = bounds.extents[i] + min.at(i);
  }

  // Go through the vertices again to calculate the exact bounding sphere
  // radius
  float r2 = 0;
  for (int i = 0; i != count; ++i) {
    auto offset =
        std::array<float, 3>{vertices[i].position[0] - bounds.origin[0],
                             vertices[i].position[1] - bounds.origin[1],
                             vertices[i].position[2] - bounds.origin[2]};

    float distance =
        offset[0] * offset[0] + offset[1] * offset[1] + offset[2] * offset[2];
    r2 = std::max(r2, distance);
  }
  bounds.radius = std::sqrt(r2);

  return bounds;
}