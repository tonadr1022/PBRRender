#include "MeshLoader.hpp"

#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>

#include "Renderer.hpp"
#include "ResourceManager.hpp"
#include "gl/Texture.hpp"
#include "pch.hpp"
#include "stb_image_impl.hpp"
#include "types.hpp"

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

  static constexpr auto kSupportedExtensions = fastgltf::Extensions::KHR_mesh_quantization |
                                               fastgltf::Extensions::KHR_texture_transform |
                                               fastgltf::Extensions::KHR_materials_variants;

  constexpr auto kOptions =
      fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble |
      fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers |
      fastgltf::Options::LoadExternalImages | fastgltf::Options::GenerateMeshIndices;
  fastgltf::GltfDataBuffer data;
  data.loadFromFile(path);

  fastgltf::Parser parser(kSupportedExtensions);
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
    spdlog::error("Failed to load glTF: {}", fastgltf::getErrorMessage(result.error()));
    return {};
  }
  spdlog::error("Failed to determine glTF container");
  return {};
}

struct Image {
  unsigned char* data{};
  int width{}, height{}, channels{};
};

}  // namespace

namespace loader {

Model LoadModel(ResourceManager& resource_manager, Renderer& renderer,
                const std::filesystem::path& path) {
  if (!std::filesystem::exists(path)) {
    spdlog::error("Failed to find {}", path.string());
    return {};
  }
  auto res = LoadGLTFAsset(path);
  if (!res) {
    spdlog::error("Failed to load model: {}", path.string());
    return {};
  }
  Model out_model;
  auto& asset = res.value();

  // Load images using stb_image
  std::vector<Image> images;
  images.reserve(asset.images.size());
  for (fastgltf::Image& image : asset.images) {
    std::visit(
        fastgltf::visitor{
            [](auto&) {},
            [&images](fastgltf::sources::URI& file_path) {
              int w, h, channels;
              const std::string path(file_path.uri.path().begin(), file_path.uri.path().end());
              unsigned char* data = stbi_load(path.c_str(), &w, &h, &channels, 4);
              images.emplace_back(
                  Image{.data = data, .width = w, .height = h, .channels = channels});
            },
            [&images](fastgltf::sources::Array& vector) {
              int w, h, channels;
              unsigned char* data = stbi_load_from_memory(
                  reinterpret_cast<const stbi_uc*>(vector.bytes.data()),
                  static_cast<int>(vector.bytes.size_bytes()), &w, &h, &channels, 4);
              images.emplace_back(
                  Image{.data = data, .width = w, .height = h, .channels = channels});
            },
            [&](fastgltf::sources::BufferView& view) {
              auto& buffer_view = asset.bufferViews[view.bufferViewIndex];
              auto& buffer = asset.buffers[buffer_view.bufferIndex];
              std::visit(fastgltf::visitor{
                             [](auto&) {},
                             [&](fastgltf::sources::Array& vector) {
                               int w, h, channels;
                               unsigned char* data = stbi_load_from_memory(
                                   reinterpret_cast<const stbi_uc*>(vector.bytes.data() +
                                                                    buffer_view.byteOffset),
                                   static_cast<int>(buffer_view.byteLength), &w, &h, &channels, 4);
                               images.emplace_back(Image{
                                   .data = data, .width = w, .height = h, .channels = channels});
                             }},
                         buffer.data);
            }},
        image.data);
  }

  // Load images into GL textures. Doing so separately since multithreading is possible above
  out_model.texture_handles.reserve(images.size());
  std::vector<std::string> names;
  int i = 0;
  for (Image& img : images) {
    names.emplace_back(path.string() + std::to_string(i++));
    out_model.texture_handles.emplace_back(
        resource_manager.Load(path.string() + std::to_string(i++),
                              gl::Tex2DCreateInfo{.dims = glm::ivec2{img.width, img.height},
                                                  .wrap_s = GL_REPEAT,
                                                  .wrap_t = GL_REPEAT,
                                                  .internal_format = GL_RGBA8,
                                                  .format = GL_RGBA,
                                                  .type = GL_UNSIGNED_BYTE,
                                                  .min_filter = GL_LINEAR,
                                                  .mag_filter = GL_LINEAR,
                                                  .data = img.data,
                                                  .bindless = true,
                                                  .gen_mipmaps = true}));
    stbi_image_free(img.data);
  }

  auto convert_alpha_mode = [](fastgltf::AlphaMode mode) -> AlphaMode {
    switch (mode) {
      case fastgltf::AlphaMode::Opaque:
        return AlphaMode::kOpaque;
      case fastgltf::AlphaMode::Blend:
      case fastgltf::AlphaMode::Mask:
        return AlphaMode::kBlend;
    }
  };

  // Load materials
  out_model.material_handles.reserve(asset.materials.size());

  std::vector<size_t> indices;
  for (fastgltf::Material& gltf_mat : asset.materials) {
    // TODO: handle base color texture
    uint64_t base_color_bindless_handle{};
    uint64_t metallic_roughness_bindless_handle{};
    uint64_t normal_bindless_handle{};
    if (gltf_mat.pbrData.baseColorTexture.has_value()) {
      auto* tex = resource_manager.Get<gl::Texture>(
          out_model.texture_handles[gltf_mat.pbrData.baseColorTexture->textureIndex]);
      if (tex) {
        base_color_bindless_handle = tex->BindlessHandle();
      }
      indices.emplace_back(gltf_mat.pbrData.baseColorTexture->textureIndex);
    }
    if (gltf_mat.pbrData.metallicRoughnessTexture.has_value()) {
      auto* tex = resource_manager.Get<gl::Texture>(
          out_model.texture_handles[gltf_mat.pbrData.metallicRoughnessTexture->textureIndex]);
      if (tex) {
        metallic_roughness_bindless_handle = tex->BindlessHandle();
      }
      indices.emplace_back(gltf_mat.pbrData.metallicRoughnessTexture->textureIndex);
    }
    if (gltf_mat.normalTexture.has_value()) {
      auto* tex = resource_manager.Get<gl::Texture>(
          out_model.texture_handles[gltf_mat.normalTexture->textureIndex]);
      if (tex) {
        normal_bindless_handle = tex->BindlessHandle();
      }
      indices.emplace_back(gltf_mat.normalTexture->textureIndex);
    }
    if (gltf_mat.occlusionTexture.has_value()) {
      spdlog::error("unimplemented occlusion texture");
    }

    auto& base_color = gltf_mat.pbrData.baseColorFactor;
    out_model.material_handles.emplace_back(renderer.AllocateMaterial(
        Material{
            .base_color = glm::vec4{base_color[0], base_color[1], base_color[2], base_color[3]},
            .base_color_bindless_handle = base_color_bindless_handle,
            .metallic_roughness_bindless_handle = metallic_roughness_bindless_handle,
            .normal_bindless_handle = normal_bindless_handle,
            .alpha_cutoff = gltf_mat.alphaCutoff,
        },
        convert_alpha_mode(gltf_mat.alphaMode)));
  }
  // Load primitives
  for (fastgltf::Mesh& mesh : asset.meshes) {
    for (auto& gltf_primitive : mesh.primitives) {
      Primitive out_primitive;
      auto* position_it = gltf_primitive.findAttribute("POSITION");
      if (position_it == gltf_primitive.attributes.end()) {
        spdlog::error("glTF Mesh does not contain POSITION attribute");
        continue;
      }
      EASSERT_MSG(gltf_primitive.indicesAccessor.has_value(), "Must specify to generate indices");

      auto primitive_type = static_cast<PrimitiveType>(gltf_primitive.type);
      // bool has_material = false;
      size_t base_color_tex_coord_idx = 0;
      if (gltf_primitive.materialIndex.has_value()) {
        // has_material = true;
        // TODO: add material uniforms idx to primitive

        out_primitive.material_handle =
            out_model.material_handles[gltf_primitive.materialIndex.value()];
        auto& material = asset.materials[gltf_primitive.materialIndex.value()];
        auto& base_color_tex = material.pbrData.baseColorTexture;
        if (base_color_tex.has_value()) {
          auto& tex = asset.textures[base_color_tex->textureIndex];
          if (!tex.imageIndex.has_value()) {
            // TODO: handle no base texture image
            EASSERT(0);
          }
          if (base_color_tex->transform && base_color_tex->transform->texCoordIndex.has_value()) {
            base_color_tex_coord_idx = base_color_tex->transform->texCoordIndex.value();
          } else {
            base_color_tex_coord_idx = material.pbrData.baseColorTexture->texCoordIndex;
          }
        } else {
          spdlog::info("no base color for material");
          // TODO: default texture
        }
      }

      // Position
      auto& position_accessor = asset.accessors[position_it->second];
      if (!position_accessor.bufferViewIndex.has_value()) {
        spdlog::error("no position accessor for primitive at model path {}", path.string());
        continue;
      }
      const auto* tex_coord_iter = gltf_primitive.findAttribute(
          std::string("TEXCOORD_") + std::to_string(base_color_tex_coord_idx));
      const auto* normal_iter = gltf_primitive.findAttribute("NORMAL");
      const auto* tangent_iter = gltf_primitive.findAttribute("TANGENT");

      // Allocate the vertices, callback function takes the pointer to the allocation of the
      // mapped buffer, and copies the data to it, or returns if the allocator couldn't allocate
      // the size mesh handle returned is then used to allocate indices associated with the mesh
      const bool has_tex_coords =
          tex_coord_iter != gltf_primitive.attributes.end() &&
          asset.accessors[tex_coord_iter->second].bufferViewIndex.has_value();
      const bool has_normals = normal_iter != gltf_primitive.attributes.end() &&
                               asset.accessors[normal_iter->second].bufferViewIndex.has_value();
      const bool has_tangents = tangent_iter != gltf_primitive.attributes.end() &&
                                asset.accessors[tangent_iter->second].bufferViewIndex.has_value();
      std::vector<Vertex> vertices(position_accessor.count);
      fastgltf::iterateAccessorWithIndex<glm::vec3>(
          asset, position_accessor,
          [&vertices](glm::vec3 pos, size_t idx) { vertices[idx].position = pos; });
      if (has_tex_coords) {
        auto& tex_coord_accessor = asset.accessors[tex_coord_iter->second];
        fastgltf::iterateAccessorWithIndex<glm::vec2>(
            asset, tex_coord_accessor,
            [&vertices](glm::vec2 uv, size_t idx) { vertices[idx].uv = uv; });
      }
      if (has_normals) {
        auto& normal_accessor = asset.accessors[normal_iter->second];
        fastgltf::iterateAccessorWithIndex<glm::vec3>(
            asset, normal_accessor,
            [&vertices](glm::vec3 normal, size_t idx) { vertices[idx].normal = normal; });
      }
      if (has_tangents) {
        auto& tangent_accessor = asset.accessors[tangent_iter->second];
        fastgltf::iterateAccessorWithIndex<glm::vec4>(
            asset, tangent_accessor, [&vertices](glm::vec4 tangent, size_t idx) {
              vertices[idx].tangent = glm::vec3{tangent.x, tangent.y, tangent.z};
            });
      }

      if (!has_tangents) {
        spdlog::error("model does not have tangents, not supported");
      }
      if (!has_normals) {
        spdlog::error("model does not have normals, not supported");
      }
      if (!has_tex_coords) {
        spdlog::error("model does not have tex coords, not supported");
      }
      uint32_t mesh_handle = renderer.AllocateMeshVertices<Vertex>(vertices, primitive_type);

      // Allocate indices, using mapped index buffer
      auto& index_accessor = asset.accessors[gltf_primitive.indicesAccessor.value()];
      if (!index_accessor.bufferViewIndex.has_value()) {
        spdlog::info("no index accessor buffer view index for primitive at path {}", path.string());
      }
      if (index_accessor.componentType == fastgltf::ComponentType::UnsignedByte ||
          index_accessor.componentType == fastgltf::ComponentType::UnsignedShort) {
        spdlog::info("16");
        renderer.AllocateMeshIndices<uint16_t>(
            mesh_handle, index_accessor.count,
            [&asset, &index_accessor](uint16_t* indices, bool error) {
              if (error) {
                spdlog::error("error loading indices");
                return;
              }
              fastgltf::copyFromAccessor<uint16_t>(asset, index_accessor, indices);
            });
      } else {
        spdlog::info("32");
        renderer.AllocateMeshIndices<uint32_t>(
            mesh_handle, index_accessor.count,
            [&asset, &index_accessor](uint32_t* indices, bool error) {
              if (error) {
                spdlog::error("error loading indices");
                return;
              }
              fastgltf::copyFromAccessor<uint32_t>(asset, index_accessor, indices);
            });
      }
      out_primitive.mesh_handle = mesh_handle;
      out_model.primitives.emplace_back(out_primitive);
    }
  }
  return out_model;
}

}  // namespace loader
