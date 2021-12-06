#include "../assetlib/mesh_asset.hpp"
#include "../assetlib/texture_asset.hpp"
#include "../stb_image_implementation.hpp"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <tiny_obj_loader.h>
#include <vector>

void pack_vertex(assets::Vertex_f32_PNCV &new_vert, tinyobj::real_t vx,
                 tinyobj::real_t vy, tinyobj::real_t vz, tinyobj::real_t nx,
                 tinyobj::real_t ny, tinyobj::real_t nz, tinyobj::real_t ux,
                 tinyobj::real_t uy) {
  new_vert.position[0] = vx;
  new_vert.position[1] = vy;
  new_vert.position[2] = vz;

  new_vert.normal[0] = nx;
  new_vert.normal[1] = ny;
  new_vert.normal[2] = nz;

  new_vert.uv[0] = ux;
  new_vert.uv[1] = 1 - uy;
}

void pack_vertex(assets::Vertex_P32N8C8V16 &new_vert, tinyobj::real_t vx,
                 tinyobj::real_t vy, tinyobj::real_t vz, tinyobj::real_t nx,
                 tinyobj::real_t ny, tinyobj::real_t nz, tinyobj::real_t ux,
                 tinyobj::real_t uy) {
  new_vert.position[0] = vx;
  new_vert.position[1] = vy;
  new_vert.position[2] = vz;

  new_vert.normal[0] = uint8_t(((nx + 1.0) / 2.0) * 255);
  new_vert.normal[1] = uint8_t(((ny + 1.0) / 2.0) * 255);
  new_vert.normal[2] = uint8_t(((nz + 1.0) / 2.0) * 255);

  new_vert.uv[0] = ux;
  new_vert.uv[1] = 1 - uy;
}

template <typename V>
void extract_mesh_from_obj(std::vector<tinyobj::shape_t> &shapes,
                           tinyobj::attrib_t &attrib,
                           std::vector<uint32_t> &_indices,
                           std::vector<V> &_vertices) {
  // Loop over shapes
  for (size_t s = 0; s != shapes.size(); ++s) {
    // Loop over faces (polygon)
    size_t index_offset = 0;
    for (size_t f = 0; f != shapes[s].mesh.num_face_vertices.size(); ++f) {
      // Harcode loading to triangles
      int fv = 3;

      // Loop over vertices in the face
      for (size_t v = 0; v != fv; ++v) {
        // Access to assets::Vertex_f32_PNCV
        tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];

        // Vertex position
        tinyobj::real_t vx = attrib.vertices[3 * idx.vertex_index + 0];
        tinyobj::real_t vy = attrib.vertices[3 * idx.vertex_index + 1];
        tinyobj::real_t vz = attrib.vertices[3 * idx.vertex_index + 2];

        // Vertex normal
        tinyobj::real_t nx = attrib.normals[3 * idx.normal_index + 0];
        tinyobj::real_t ny = attrib.normals[3 * idx.normal_index + 1];
        tinyobj::real_t nz = attrib.normals[3 * idx.normal_index + 2];

        // Vertex UV
        tinyobj::real_t ux = attrib.texcoords[2 * idx.texcoord_index + 0];
        tinyobj::real_t uy = attrib.texcoords[2 * idx.texcoord_index + 1];

        // Copy it into our vertex
        V new_vert;
        pack_vertex(new_vert, vx, vy, vz, nx, ny, nz, ux, uy);

        _indices.push_back(static_cast<std::uint32_t>(_vertices.size()));
        _vertices.push_back(new_vert);
      }
      index_offset += fv;
    }
  }
}

auto convert_mesh(const std::filesystem::path &input,
                  const std::filesystem::path &output) {
  // Attrib will containt the assets::Vertex_f32_PNCV arrays of the file
  tinyobj::attrib_t attrib;
  // Shapes contains the info for each separate object in the file
  std::vector<tinyobj::shape_t> shapes;
  // Materials contains the information aobut the material of each shape, but we
  // won't use it
  std::vector<tinyobj::material_t> materials;

  // Error and warning output from the load function
  std::string err;
  auto pngstart = std::chrono::high_resolution_clock::now();

  auto material_path = std::filesystem::path(input).remove_filename();

  // Load the OBJ file
  tinyobj::LoadObj(&attrib, &shapes, &materials, &err, input.string().c_str(),
                   material_path.string().c_str());
  auto pngend = std::chrono::high_resolution_clock::now();
  auto diff = pngend - pngstart;

  std::cout
      << "obj took "
      << std::chrono::duration_cast<std::chrono::nanoseconds>(diff).count() /
             1000000.0
      << "ms" << '\n';

  // Make sure to output the errors to the console in case there are issues with
  // the file
  if (!err.empty()) {
    std::cerr << err << '\n';
    return false;
  }

  using VertexFormat = assets::Vertex_f32_PNCV;

  std::vector<VertexFormat> _vertices;
  std::vector<std::uint32_t> _indices;

  extract_mesh_from_obj(shapes, attrib, _indices, _vertices);

  assets::MeshInfo meshinfo;
  meshinfo.vertexFormat = assets::VertexFormat::PNCV_F32;
  meshinfo.vertexBufferSize = _vertices.size() * sizeof(VertexFormat);
  meshinfo.indexBufferSize = _indices.size() * sizeof(std::uint32_t);
  meshinfo.indexSize = sizeof(std::uint32_t);
  meshinfo.originalFile = input.string();

  meshinfo.bounds =
      assets::calcualate_bounds(_vertices.data(), _vertices.size());
  // Pack mesh file
  auto start = std::chrono::high_resolution_clock::now();

  assets::AssetFile newFile = assets::pack_mesh(
      &meshinfo, (char *)_vertices.data(), (char *)_indices.data());

  auto end = std::chrono::high_resolution_clock::now();

  diff = end - start;

  std::cout
      << "compression took "
      << std::chrono::duration_cast<std::chrono::nanoseconds>(diff).count() /
             1000000.0
      << "ms" << '\n';

  // Save to disk
  save_binaryfile(output.string().c_str(), newFile);

  return true;
}

auto convert_image(const std::filesystem::path &input,
                   const std::filesystem::path &output) {
  int texWidth, texHeight, texChannels;

  stbi_uc *pixels = stbi_load(input.string().c_str(), &texWidth, &texHeight,
                              &texChannels, STBI_rgb_alpha);

  if (!pixels) {
    std::cout << "Failed to load texture file " << input << '\n';
    return false;
  }

  int texture_size = texWidth * texHeight * 4;

  assets::TextureInfo texinfo;
  texinfo.textureSize = texture_size;
  texinfo.pixelsize[0] = texWidth;
  texinfo.pixelsize[1] = texHeight;
  texinfo.textureFormat = assets::TextureFormat::RGBA8;
  texinfo.originalFile = input.string();
  assets::AssetFile newImage = assets::pack_texture(&texinfo, pixels);

  stbi_image_free(pixels);
  save_binaryfile(output.string().c_str(), newImage);

  return true;
}

auto main([[maybe_unused]] int argc, [[maybe_unused]] char *argv[]) -> int {
  std::filesystem::path path{argv[1]};
  std::filesystem::path directory = path;

  std::cout << "Loading asset directory at " << directory << '\n';

  for (auto &&p : std::filesystem::directory_iterator(directory)) {
    std::cout << "File: " << p;

    if (p.path().extension() == ".png") {
      std::cout << "found a texture" << '\n';

      auto newpath = p.path();
      newpath.replace_extension(".tx");
      convert_image(p.path(), newpath);
    }
    if (p.path().extension() == ".obj") {
      std::cout << "found a mesh" << '\n';

      auto newpath = p.path();
      newpath.replace_extension(".mesh");
      convert_mesh(p.path(), newpath);
    }
  }

  return 0;
}
