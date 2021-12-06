#pragma once

#include "vk_types.hpp"
#include <filesystem>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <string_view>
#include <vector>

struct VertexInputDescription {
  std::vector<VkVertexInputBindingDescription> bindings;
  std::vector<VkVertexInputAttributeDescription> attributes;

  VkPipelineVertexInputStateCreateFlags flags = 0;
};

struct Vertex {
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec3 color;
  glm::vec2 uv;

  static auto get_vertex_description() -> VertexInputDescription;
};

struct RenderBounds {
  glm::vec3 origin;
  float radius;
  glm::vec3 extents;
  bool valid;
};

struct Mesh {
  // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
  std::vector<Vertex> _vertices;
  std::vector<std::uint32_t> _indices;
  // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
  AllocatedBuffer _vertexBuffer;
  AllocatedBuffer _indexBuffer;

  RenderBounds bounds;

  auto load_from_obj(const std::filesystem::path &filename) -> bool;

  auto load_from_meshasset(const std::filesystem::path &filename) -> bool;
};
