#include "MeshLoader.hpp"

#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>

#include "Renderer.hpp"
#include "pch.hpp"
#include "stb_image_impl.hpp"
#include "types.hpp"

// bool LoadMesh(fastgltf::Mesh& mesh) {
//
// }

GLuint ToGLFilter(fastgltf::Filter filter) {
  switch (filter) {
    case fastgltf::Filter::Linear:
      return GL_LINEAR;
    case fastgltf::Filter::Nearest:
      return GL_NEAREST;
    case fastgltf::Filter::NearestMipMapLinear:
      return GL_NEAREST_MIPMAP_LINEAR;
    case fastgltf::Filter::LinearMipMapNearest:
      return GL_LINEAR_MIPMAP_NEAREST;
    case fastgltf::Filter::LinearMipMapLinear:
      return GL_LINEAR_MIPMAP_LINEAR;
    case fastgltf::Filter::NearestMipMapNearest:
      return GL_NEAREST_MIPMAP_NEAREST;
  }
}

namespace {

std::optional<fastgltf::Asset> LoadGLTFAsset(const std::filesystem::path& path) {
  if (!std::filesystem::exists(path)) {
    spdlog::error("Failed to find {}", path.string());
    return {};
  }

  fastgltf::Parser parser;
  constexpr auto kOptions =
      fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble |
      fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers |
      fastgltf::Options::LoadExternalImages | fastgltf::Options::GenerateMeshIndices;
  fastgltf::GltfDataBuffer data;
  data.loadFromFile(path);

  fastgltf::Asset gltf;

  auto asset = parser.loadGltf(&data, path.parent_path(), kOptions);
  auto type = fastgltf::determineGltfFileType(&data);
  if (type == fastgltf::GltfType::glTF) {
    auto result = parser.loadGltf(&data, path.parent_path(), kOptions);
    if (result) {
      return std::move(result.get());
    }
    spdlog::error("Failed to load glTF: {}", fastgltf::to_underlying(result.error()));
    return {};
  }
  if (type == fastgltf::GltfType::GLB) {
    auto result = parser.loadGltfBinary(&data, path.parent_path(), kOptions);
    if (result) {
      return std::move(result.get());
    }
  } else {
    spdlog::error("Failed to determine glTF container");
    return {};
  }
  return {};
}

struct Image {
  unsigned char* data{};
  int width, height, channels;
};

struct IndirectDrawCmd {
  uint32_t count;
  uint32_t instance_count;
  uint32_t first_index;
  uint32_t base_vertex;
  uint32_t base_instance;
};

enum class AlphaMode {
  kOpaque,
  kBlend,
};
struct Material {
  float alpha_cutoff;
  AlphaMode alpha_mode;
  glm::vec4 base_color;
};

struct Primitive {
  PrimitiveType type;
};

}  // namespace

void LoadModel(Renderer& renderer, const std::filesystem::path& path) {
  if (!std::filesystem::exists(path)) {
    spdlog::error("Failed to find {}", path.string());
    return;
  }
  auto res = LoadGLTFAsset(path);
  if (!res) {
    spdlog::error("Failed to load model: {}", path.string());
    return;
  }
  auto& asset = res.value();
  std::vector<Image> image_data;
  for (fastgltf::Image& image : asset.images) {
    std::visit(
        fastgltf::visitor{
            [](auto&) {},
            [&image_data](fastgltf::sources::URI& file_path) {
              int w, h, channels;
              const std::string path(file_path.uri.path().begin(), file_path.uri.path().end());
              unsigned char* data = stbi_load(path.c_str(), &w, &h, &channels, 4);
              image_data.emplace_back(
                  Image{.data = data, .width = w, .height = h, .channels = channels});
            },
            [&image_data](fastgltf::sources::Array& vector) {
              int w, h, channels;
              unsigned char* data = stbi_load_from_memory(
                  reinterpret_cast<const stbi_uc*>(vector.bytes.data()),
                  static_cast<int>(vector.bytes.size_bytes()), &w, &h, &channels, 4);
              image_data.emplace_back(
                  Image{.data = data, .width = w, .height = h, .channels = channels});
            },
            [&](fastgltf::sources::BufferView& view) {
              auto& buffer_view = asset.bufferViews[view.bufferViewIndex];
              auto& buffer = asset.buffers[buffer_view.bufferIndex];
              std::visit(fastgltf::visitor{
                             // We only care about VectorWithMime here, because we
                             // specify LoadExternalBuffers, meaning
                             // all buffers are already loaded into a vector.
                             [](auto&) {},
                             [&](fastgltf::sources::Array& vector) {
                               int w, h, channels;
                               unsigned char* data = stbi_load_from_memory(
                                   reinterpret_cast<const stbi_uc*>(vector.bytes.data() +
                                                                    buffer_view.byteOffset),
                                   static_cast<int>(buffer_view.byteLength), &w, &h, &channels, 4);
                               image_data.emplace_back(Image{
                                   .data = data, .width = w, .height = h, .channels = channels});
                             }},
                         buffer.data);
            }},
        image.data);
  }

  // loading into GL textures after since multithreading is possible above
  std::vector<GLuint> textures;

  auto get_mip_level_count = [](int w, int h) -> GLsizei {
    return static_cast<GLsizei>(1 + std::floor(std::log2(std::max(w, h))));
  };

  for (Image& img : image_data) {
    GLuint tex;
    glCreateTextures(GL_TEXTURE_2D, 1, &tex);
    glTextureStorage2D(tex, get_mip_level_count(img.width, img.height), GL_RGBA8, img.width,
                       img.height);
    glTextureSubImage2D(tex, 0, 0, 0, img.width, img.height, GL_RGBA, GL_UNSIGNED_BYTE, img.data);
    glGenerateTextureMipmap(tex);
    stbi_image_free(img.data);
    textures.emplace_back(tex);
  }

  auto to_alpha_mode = [](fastgltf::AlphaMode mode) -> AlphaMode {
    switch (mode) {
      case fastgltf::AlphaMode::Opaque:
        return AlphaMode::kOpaque;
      case fastgltf::AlphaMode::Blend:
      case fastgltf::AlphaMode::Mask:
        return AlphaMode::kBlend;
    }
  };

  // materials
  for (fastgltf::Material& gltf_mat : asset.materials) {
    // TODO: handle base color texture
    // gltf_mat.pbrData.baseColorTexture;

    auto& base_color = gltf_mat.pbrData.baseColorFactor;
    // TODO: handle
    Material _{.alpha_cutoff = gltf_mat.alphaCutoff,
               .alpha_mode = to_alpha_mode(gltf_mat.alphaMode),
               .base_color = glm::vec4{base_color[0], base_color[1], base_color[2], base_color[3]}};
  }

  std::vector<Primitive> primitives;
  for (fastgltf::Mesh& mesh : asset.meshes) {
    for (auto& gltf_primitive : mesh.primitives) {
      auto* position_it = gltf_primitive.findAttribute("POSITION");
      if (position_it == gltf_primitive.attributes.end()) {
        spdlog::error("glTF Mesh does not contain POSITION attribute");
        continue;
      }
      EASSERT_MSG(gltf_primitive.indicesAccessor.has_value(), "Must specify to generate indices");

      Primitive prim{.type = static_cast<PrimitiveType>(gltf_primitive.type)};
      // bool has_material = false;
      size_t base_color_tex_coord_idx = 0;
      if (gltf_primitive.materialIndex.has_value()) {
        // has_material = true;
        // TODO: add material uniforms idx to primitive

        auto& material = asset.materials[gltf_primitive.materialIndex.value()];
        auto& base_color_tex = material.pbrData.baseColorTexture;
        if (base_color_tex.has_value()) {
          auto& tex = asset.textures[base_color_tex->textureIndex];
          if (!tex.imageIndex.has_value()) {
            // TODO: handle no base texture image
            EASSERT(0);
          }
          // TODO: handle material index for primitive
          // tex.imageIndex is used
          if (base_color_tex->transform && base_color_tex->transform->texCoordIndex.has_value()) {
            base_color_tex_coord_idx = base_color_tex->transform->texCoordIndex.value();
          } else {
            base_color_tex_coord_idx = material.pbrData.baseColorTexture->texCoordIndex;
          }
        }

        // get the material
      }

      // Position
      auto& position_accessor = asset.accessors[position_it->second];
      if (!position_accessor.bufferViewIndex.has_value()) {
        spdlog::info("no position accessor for primitive at path {}", path.string());
        continue;
      }
      bool has_tex_coords = base_color_tex_coord_idx != 0;
      const auto* tex_coord = gltf_primitive.findAttribute(
          std::string("TEXCOORD_") + std::to_string(base_color_tex_coord_idx));
      if (tex_coord == gltf_primitive.attributes.end() ||
          !asset.accessors[tex_coord->second].bufferViewIndex.has_value()) {
        has_tex_coords = false;
      }
      // Allocate the vertices, callback function takes the pointer to the allocation of the mapped
      // buffer, and copies the data to it, or returns if the allocator couldn't allocate the size
      // mesh handle returned is then used to allocate indices associated with the mesh
      uint32_t mesh_handle = renderer.AllocateVertices<Vertex>(
          position_accessor.count,
          [&asset, &position_accessor, &tex_coord, has_tex_coords](Vertex* vertices, bool error) {
            if (error) return;
            fastgltf::iterateAccessorWithIndex<glm::vec3>(
                asset, position_accessor,
                [vertices](glm::vec3 pos, size_t idx) { vertices[idx].position = pos; });

            if (!has_tex_coords) return;
            auto& tex_coord_accessor = asset.accessors[tex_coord->second];
            fastgltf::iterateAccessorWithIndex<glm::vec3>(
                asset, tex_coord_accessor,
                [vertices](glm::vec2 uv, size_t idx) { vertices[idx].uv = uv; });

            // TODO: normals
          });

      auto& index_accessor = asset.accessors[gltf_primitive.indicesAccessor.value()];
      if (!index_accessor.bufferViewIndex.has_value()) {
        spdlog::info("no index accessor buffer view index for primitive at path {}", path.string());
      }
      if (index_accessor.componentType == fastgltf::ComponentType::UnsignedByte ||
          index_accessor.componentType == fastgltf::ComponentType::UnsignedShort) {
        renderer.AllocateIndices<uint16_t>(
            mesh_handle, index_accessor.count,
            [&asset, &index_accessor](uint16_t* indices, bool error) {
              if (error) return;
              fastgltf::copyFromAccessor<uint16_t>(asset, index_accessor, indices);
            });
      } else {
        renderer.AllocateIndices<uint32_t>(
            mesh_handle, index_accessor.count,
            [&asset, &index_accessor](uint32_t* indices, bool error) {
              if (error) return;
              fastgltf::copyFromAccessor<uint32_t>(asset, index_accessor, indices);
            });
      }

      primitives.emplace_back(prim);
    }
  }
}
