#pragma once
#include "asset_loader.hpp"

namespace assets {
struct Vertex_f32_PNCV {
  float position[3];
  float normal[3];
  float color[3];
  float uv[2];
};

struct Vertex_P32N8C8V16 {
  float position[3];
  std::uint8_t normal[3];
  std::uint8_t color[3];
  float uv[2];
};

enum class VertexFormat : std::uint32_t {
  Unknown = 0,
  PNCV_F32,  // Everything at 32 bits
  P32N8C8V16 // Position at 32 bits, normal at 8 bits, color at 8 bits, uvs at
             // 16 bits float
};

struct MeshBounds {
  float origin[3];
  float radius;
  float extents[3];
};

struct MeshInfo {
  std::uint64_t vertexBufferSize;
  std::uint64_t indexBufferSize;
  MeshBounds bounds;
  VertexFormat vertexFormat;
  char indexSize;
  CompressionMode compressionMode;
  std::string originalFile;
};

auto parse_format(const char *f) -> VertexFormat;

auto read_mesh_info(AssetFile *file) -> MeshInfo;

void unpack_mesh(MeshInfo *info, const char *sourceBuffer, size_t sourceSize,
                 char *vertexBuffer, char *indexBuffer);

auto pack_mesh(MeshInfo *info, char *vertexData, char *indexData) -> AssetFile;

auto calcualate_bounds(Vertex_f32_PNCV *vertices, size_t count) -> MeshBounds;

} // namespace assets
