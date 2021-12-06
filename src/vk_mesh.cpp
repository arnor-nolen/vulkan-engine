#include "vk_mesh.hpp"
#include "assetlib/mesh_asset.hpp"
#include <array>
#include <filesystem>
#include <iostream>
#include <string_view>
#include <tiny_obj_loader.h>

auto Vertex::get_vertex_description() -> VertexInputDescription {
  VertexInputDescription description;

  // We will have just 1 vertex buffer binding, with a per-vertex rate
  VkVertexInputBindingDescription mainBinding = {};
  mainBinding.binding = 0;
  mainBinding.stride = sizeof(Vertex);
  mainBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  description.bindings.push_back(mainBinding);

  // Position will be stored at Location 0
  VkVertexInputAttributeDescription positionAttribute = {};
  positionAttribute.binding = 0;
  positionAttribute.location = 0;
  positionAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
  positionAttribute.offset = offsetof(Vertex, position);

  // Normal will be stored at Location 1
  VkVertexInputAttributeDescription normalAttribute = {};
  normalAttribute.binding = 0;
  normalAttribute.location = 1;
  normalAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
  normalAttribute.offset = offsetof(Vertex, normal);

  // Color will be stored at Location 2
  VkVertexInputAttributeDescription colorAttribute = {};
  colorAttribute.binding = 0;
  colorAttribute.location = 2;
  colorAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
  colorAttribute.offset = offsetof(Vertex, color);

  // UV will be stored at Location 3
  VkVertexInputAttributeDescription uvAttribute = {};
  uvAttribute.binding = 0;
  uvAttribute.location = 3;
  uvAttribute.format = VK_FORMAT_R32G32_SFLOAT;
  uvAttribute.offset = offsetof(Vertex, uv);

  description.attributes.push_back(positionAttribute);
  description.attributes.push_back(normalAttribute);
  description.attributes.push_back(colorAttribute);
  description.attributes.push_back(uvAttribute);

  return description;
}

auto Mesh::load_from_meshasset(const std::filesystem::path &filename) -> bool {
  assets::AssetFile file;
  bool loaded = assets::load_binaryfile(filename, file);

  if (!loaded) {
    std::cout << "Error when loading mesh " << filename << std::endl;
    ;
    return false;
  }

  assets::MeshInfo meshinfo = assets::read_mesh_info(&file);

  std::vector<char> vertexBuffer;
  std::vector<char> indexBuffer;

  vertexBuffer.resize(meshinfo.vertexBufferSize);
  indexBuffer.resize(meshinfo.indexBufferSize);

  assets::unpack_mesh(&meshinfo, file.binaryBlob.data(), file.binaryBlob.size(),
                      vertexBuffer.data(), indexBuffer.data());

  bounds.extents = {meshinfo.bounds.extents[0], meshinfo.bounds.extents[1],
                    meshinfo.bounds.extents[2]};
  bounds.origin = {meshinfo.bounds.origin[0], meshinfo.bounds.origin[1],
                   meshinfo.bounds.origin[2]};
  bounds.radius = meshinfo.bounds.radius;
  bounds.valid = true;

  _vertices.clear();
  _indices.clear();

  _indices.resize(indexBuffer.size() / sizeof(std::uint32_t));
  for (int i = 0; i != _indices.size(); ++i) {
    uint32_t *unpacked_indices = (uint32_t *)indexBuffer.data();
    _indices[i] = unpacked_indices[i];
  }

  if (meshinfo.vertexFormat == assets::VertexFormat::PNCV_F32) {
    assets::Vertex_f32_PNCV *unpackedVertices =
        (assets::Vertex_f32_PNCV *)vertexBuffer.data();

    _vertices.resize(vertexBuffer.size() / sizeof(assets::Vertex_f32_PNCV));

    for (int i = 0; i != _vertices.size(); ++i) {

      _vertices[i].position.x = unpackedVertices[i].position[0];
      _vertices[i].position.y = unpackedVertices[i].position[1];
      _vertices[i].position.z = unpackedVertices[i].position[2];

      _vertices[i].normal = glm::vec3(unpackedVertices[i].normal[0],
                                      unpackedVertices[i].normal[1],
                                      unpackedVertices[i].normal[2]);

      _vertices[i].color =
          glm::vec3{unpackedVertices[i].color[0], unpackedVertices[i].color[1],
                    unpackedVertices[i].color[2]};

      _vertices[i].uv.x = unpackedVertices[i].uv[0];
      _vertices[i].uv.y = unpackedVertices[i].uv[1];
    }
  } else if (meshinfo.vertexFormat == assets::VertexFormat::P32N8C8V16) {
    assets::Vertex_P32N8C8V16 *unpackedVertices =
        (assets::Vertex_P32N8C8V16 *)vertexBuffer.data();

    _vertices.resize(vertexBuffer.size() / sizeof(assets::Vertex_P32N8C8V16));

    for (int i = 0; i < _vertices.size(); i++) {
      _vertices[i].position = {unpackedVertices[i].position[0],
                               unpackedVertices[i].position[1],
                               unpackedVertices[i].position[2]};
      _vertices[i].normal = {unpackedVertices[i].normal[0],
                             unpackedVertices[i].normal[1],
                             unpackedVertices[i].normal[2]};
      _vertices[i].color = {unpackedVertices[i].color[0],
                            unpackedVertices[i].color[1],
                            unpackedVertices[i].color[2]};
      _vertices[i].uv = {unpackedVertices[i].uv[0], unpackedVertices[i].uv[1]};
    }
  }

  std::cout << "Loaded mesh " << filename << ": Verts=" << _vertices.size()
            << ", Tris=" << _indices.size() / 3 << '\n';

  return true;
}

auto Mesh::load_from_obj(const std::filesystem::path &filename) -> bool {

  auto material_path = std::filesystem::path(filename).remove_filename();

  // Attrib will contain the vertex arrays of the file
  tinyobj::attrib_t attrib;
  // Shapes contains the info for each separate object in the file
  std::vector<tinyobj::shape_t> shapes;
  // Materials contains the information about the material of each shape
  std::vector<tinyobj::material_t> materials;

  // Error and warning output from the load function
  std::string err;

  // Load the OBJ file
  tinyobj::LoadObj(&attrib, &shapes, &materials, &err,
                   filename.string().c_str(), material_path.string().c_str());

  // If we have any error, print it to the console, and break the mesh loading.
  // This happens if the file can't be found or is malformed
  if (!err.empty()) {
    std::cerr << err << '\n';
    return false;
  }

  // Loop over shapes
  for (auto &&shape : shapes) {
    // Loop over faces(polygon)
    size_t index_offset = 0;
    for (size_t f = 0; f != shape.mesh.num_face_vertices.size(); ++f) {
      // Hardcode loading to triangles
      size_t fv = 3;
      // Loop over vertices in the face
      for (size_t v = 0; v != fv; ++v) {
        // Access to vertex
        tinyobj::index_t idx = shape.mesh.indices[index_offset + v];

        // Vertex position
        auto vc = glm::vec3();
        for (int i = 0; i != 3; ++i) {
          vc[i] = attrib.vertices[3 * idx.vertex_index + i];
        }

        // Vertex normal
        auto nc = glm::vec3();
        for (int i = 0; i != 3; ++i) {
          nc[i] = attrib.normals[3 * idx.normal_index + i];
        }

        // Vertex uv
        tinyobj::real_t ux = attrib.texcoords[2 * idx.texcoord_index + 0];
        tinyobj::real_t uy = attrib.texcoords[2 * idx.texcoord_index + 1];

        // We are setting the vertex color as the vertex normal. This is just
        // for display purposes
        Vertex new_vert = {.position = vc,
                           .normal = nc,
                           .color = nc,
                           .uv = glm::vec2{ux, 1 - uy}};

        _vertices.push_back(new_vert);
      }
      index_offset += fv;
    }
  }
  return true;
}
