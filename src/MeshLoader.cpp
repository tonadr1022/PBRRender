#include "MeshLoader.hpp"

#include <mikktspace.h>

#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <iostream>

#include "pch.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

#include "Renderer.hpp"
#include "ResourceManager.hpp"
#include "gl/Texture.hpp"
#include "stb_image_impl.hpp"
#include "types.hpp"

namespace {

template <typename IndexType>
void CalcTangents(std::vector<Vertex>& vertices, std::vector<IndexType>& indices) {
  SMikkTSpaceContext ctx{};
  SMikkTSpaceInterface interface{};
  ctx.m_pInterface = &interface;

  struct MyCtx {
    MyCtx(std::vector<Vertex>& vertices, std::vector<IndexType>& indices)
        : vertices(vertices), indices(indices), num_faces(indices.size() / 3) {}
    std::vector<Vertex>& vertices;
    std::vector<IndexType>& indices;
    size_t num_faces{};
    int face_size = 3;
    Vertex& GetVertex(int face_idx, int vert_idx) {
      return vertices[indices[(face_idx * face_size) + vert_idx]];
    }
  };

  MyCtx my_ctx{vertices, indices};
  ctx.m_pUserData = &my_ctx;

  interface.m_getNumFaces = [](const SMikkTSpaceContext* ctx) -> int {
    return reinterpret_cast<MyCtx*>(ctx->m_pUserData)->num_faces;
  };
  // assuming GL_TRIANGLES until it becomes an issue
  interface.m_getNumVerticesOfFace = [](const SMikkTSpaceContext* ctx, const int) {
    return reinterpret_cast<MyCtx*>(ctx->m_pUserData)->face_size;
  };

  interface.m_getPosition = [](const SMikkTSpaceContext* ctx, float fvPosOut[], const int iFace,
                               const int iVert) {
    MyCtx& my_ctx = *reinterpret_cast<MyCtx*>(ctx->m_pUserData);
    Vertex& vertex = my_ctx.GetVertex(iFace, iVert);
    fvPosOut[0] = vertex.position.x;
    fvPosOut[1] = vertex.position.y;
    fvPosOut[2] = vertex.position.z;
  };
  interface.m_getNormal = [](const SMikkTSpaceContext* ctx, float fvNormOut[], const int iFace,
                             const int iVert) {
    MyCtx& my_ctx = *reinterpret_cast<MyCtx*>(ctx->m_pUserData);
    Vertex& vertex = my_ctx.GetVertex(iFace, iVert);
    fvNormOut[0] = vertex.normal.x;
    fvNormOut[1] = vertex.normal.y;
    fvNormOut[2] = vertex.normal.z;
  };
  interface.m_getTexCoord = [](const SMikkTSpaceContext* ctx, float fvTexcOut[], const int iFace,
                               const int iVert) {
    MyCtx& my_ctx = *reinterpret_cast<MyCtx*>(ctx->m_pUserData);
    Vertex& vertex = my_ctx.GetVertex(iFace, iVert);
    fvTexcOut[0] = vertex.uv.x;
    fvTexcOut[1] = vertex.uv.y;
  };
  interface.m_setTSpaceBasic = [](const SMikkTSpaceContext* ctx, const float fvTangent[],
                                  const float, const int iFace, const int iVert) {
    MyCtx& my_ctx = *reinterpret_cast<MyCtx*>(ctx->m_pUserData);
    Vertex& vertex = my_ctx.GetVertex(iFace, iVert);
    vertex.tangent.x = fvTangent[0];
    vertex.tangent.y = fvTangent[1];
    vertex.tangent.z = fvTangent[2];
    std::cout << vertex.tangent.x << '\n';
    // vertex.tangent.w = fSign;
  };
  genTangSpaceDefault(&ctx);
}

void DecomposeMatrix(const glm::mat4& m, glm::vec3& pos, glm::quat& rot, glm::vec3& scale) {
  pos = m[3];
  for (int i = 0; i < 3; i++) scale[i] = glm::length(glm::vec3(m[i]));
  const glm::mat3 rot_mtx(glm::vec3(m[0]) / scale[0], glm::vec3(m[1]) / scale[1],
                          glm::vec3(m[2]) / scale[2]);
  rot = glm::quat_cast(rot_mtx);
}

// GLuint ToGLFilter(fastgltf::Filter filter) {
//   switch (filter) {
//     case fastgltf::Filter::Linear:
//       return GL_LINEAR;
//     case fastgltf::Filter::Nearest:
//       return GL_NEAREST;
//     case fastgltf::Filter::NearestMipMapLinear:
//       return GL_NEAREST_MIPMAP_LINEAR;
//     case fastgltf::Filter::LinearMipMapNearest:
//       return GL_LINEAR_MIPMAP_NEAREST;
//     case fastgltf::Filter::LinearMipMapLinear:
//       return GL_LINEAR_MIPMAP_LINEAR;
//     case fastgltf::Filter::NearestMipMapNearest:
//       return GL_NEAREST_MIPMAP_NEAREST;
//   }
// }

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
      fastgltf::Options::LoadExternalImages | fastgltf::Options::GenerateMeshIndices |
      fastgltf::Options::DecomposeNodeMatrices;
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
                const std::filesystem::path& path, float camera_aspect_ratio) {
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
  // TODO: multithread
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

  // Load materials
  out_model.material_handles.reserve(asset.materials.size());

  out_model.texture_handles.resize(images.size());
  int num_textures = 0;
  auto load_texture_into_material = [&path, &num_textures, &out_model, &images, &resource_manager](
                                        uint64_t& out_handle, size_t tex_idx, size_t image_index,
                                        GLuint internal_format) -> bool {
    // Load the texture with unique name and creation info
    auto& img = images[image_index];
    out_model.texture_handles[tex_idx] = resource_manager.Load<gl::Texture>(
        path.string() + std::to_string(num_textures++),
        gl::Tex2DCreateInfo{.dims = glm::ivec2{img.width, img.height},
                            .wrap_s = GL_REPEAT,
                            .wrap_t = GL_REPEAT,
                            .internal_format = internal_format,
                            .format = GL_RGBA,
                            .type = GL_UNSIGNED_BYTE,
                            .min_filter = GL_LINEAR,
                            .mag_filter = GL_LINEAR,
                            .data = img.data,
                            .bindless = true,
                            .gen_mipmaps = true});

    // get the texture and set the handle if it was made
    auto* tex = resource_manager.Get<gl::Texture>(out_model.texture_handles[tex_idx]);
    if (tex) {
      out_handle = tex->BindlessHandle();
      return true;
    }
    return false;
  };

  for (fastgltf::Material& gltf_mat : asset.materials) {
    Material out_mat{};
    if (gltf_mat.pbrData.baseColorTexture.has_value()) {
      auto& texture = gltf_mat.pbrData.baseColorTexture.value();
      auto& tex = asset.textures[gltf_mat.pbrData.baseColorTexture.value().textureIndex];
      // TODO: see if it's possible to have different textures with diff uv scales?
      if (gltf_mat.pbrData.baseColorTexture->transform) {
        auto& transform = texture.transform;
        out_mat.uv_scale = glm::make_vec2(transform->uvScale.data());
        out_mat.uv_offset = glm::make_vec2(transform->uvOffset.data());
        out_mat.uv_rotation = transform->rotation;
      }
      load_texture_into_material(out_mat.base_color_bindless_handle, texture.textureIndex,
                                 tex.imageIndex.value(), GL_SRGB8_ALPHA8);
    }

    // has metallic roughness and occlusion and indices are the same -> occlusionRoughnessMetallic
    if (gltf_mat.pbrData.metallicRoughnessTexture.has_value() &&
        gltf_mat.occlusionTexture.has_value() &&
        gltf_mat.pbrData.metallicRoughnessTexture->textureIndex ==
            gltf_mat.occlusionTexture->textureIndex) {
      auto& tex = asset.textures[gltf_mat.pbrData.metallicRoughnessTexture.value().textureIndex];
      if (load_texture_into_material(
              out_mat.metallic_roughness_bindless_handle, tex.imageIndex.value(),
              gltf_mat.pbrData.metallicRoughnessTexture->textureIndex, GL_RGBA8)) {
        out_mat.material_flags |= MaterialFlags::kOcclusionRoughnessMetallic;
      }
    } else {
      // metallic roughness
      if (gltf_mat.pbrData.metallicRoughnessTexture.has_value()) {
        auto& tex = asset.textures[gltf_mat.pbrData.metallicRoughnessTexture.value().textureIndex];
        if (load_texture_into_material(
                out_mat.metallic_roughness_bindless_handle, tex.imageIndex.value(),
                gltf_mat.pbrData.metallicRoughnessTexture->textureIndex, GL_RGBA8)) {
          out_mat.material_flags |= MaterialFlags::kMetallicRoughness;
        }
      }
      // occlusion
      if (gltf_mat.occlusionTexture.has_value()) {
        auto& tex = asset.textures[gltf_mat.occlusionTexture.value().textureIndex];
        load_texture_into_material(out_mat.occlusion_handle, tex.imageIndex.value(),
                                   gltf_mat.occlusionTexture->textureIndex, GL_RGBA8);
      }
    }

    if (gltf_mat.emissiveTexture.has_value()) {
      auto& tex = asset.textures[gltf_mat.emissiveTexture.value().textureIndex];
      load_texture_into_material(out_mat.emissive_handle, gltf_mat.emissiveTexture->textureIndex,
                                 tex.imageIndex.value(), GL_SRGB8_ALPHA8);
    }
    if (gltf_mat.normalTexture.has_value()) {
      auto& tex = asset.textures[gltf_mat.normalTexture.value().textureIndex];
      load_texture_into_material(out_mat.normal_bindless_handle, tex.imageIndex.value(),
                                 gltf_mat.normalTexture->textureIndex, GL_RGBA8);
    }

    auto& base_color = gltf_mat.pbrData.baseColorFactor;
    out_mat.base_color = glm::vec4{base_color[0], base_color[1], base_color[2], base_color[3]};
    if (gltf_mat.alphaMode == fastgltf::AlphaMode::Mask) {
      out_mat.material_flags |= MaterialFlags::kAlphaMaskOn;
      out_mat.alpha_cutoff = gltf_mat.alphaCutoff;
    }
    out_mat.metallic_factor = gltf_mat.pbrData.metallicFactor;
    out_mat.roughness_factor = gltf_mat.pbrData.roughnessFactor;
    out_mat.emissive_strength = gltf_mat.emissiveStrength;
    out_mat.emissive_factor = glm::vec4{gltf_mat.emissiveFactor[0], gltf_mat.emissiveFactor[1],
                                        gltf_mat.emissiveFactor[2], 1};

    auto convert_alpha_mode = [](fastgltf::AlphaMode mode) -> AlphaMode {
      switch (mode) {
        case fastgltf::AlphaMode::Opaque:
          return AlphaMode::kOpaque;
        case fastgltf::AlphaMode::Blend:
        case fastgltf::AlphaMode::Mask:
          return AlphaMode::kBlend;
      }
    };
    out_model.material_handles.emplace_back(
        renderer.AllocateMaterial(out_mat, convert_alpha_mode(gltf_mat.alphaMode)));
  }
  // Load primitives
  for (fastgltf::Mesh& mesh : asset.meshes) {
    Mesh out_mesh;
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

      if (!has_normals) {
        // TODO: use diff vertex layout
        spdlog::error("model does not have normals, not supported");
      }
      if (!has_tex_coords) {
        // TODO: use diff vertex layout
        spdlog::error("model does not have tex coords, not supported");
      }

      // Allocate indices, using mapped index buffer
      auto& index_accessor = asset.accessors[gltf_primitive.indicesAccessor.value()];
      if (!index_accessor.bufferViewIndex.has_value()) {
        spdlog::info("no index accessor buffer view index for primitive at path {}", path.string());
        continue;
      }
      std::vector<uint32_t> indices(index_accessor.count);
      if (index_accessor.componentType == fastgltf::ComponentType::UnsignedByte ||
          index_accessor.componentType == fastgltf::ComponentType::UnsignedShort) {
        std::vector<uint16_t> short_indices(index_accessor.count);
        fastgltf::copyFromAccessor<uint16_t>(asset, index_accessor, short_indices.data());
        std::transform(short_indices.begin(), short_indices.end(), indices.begin(),
                       [](uint16_t val) { return static_cast<uint32_t>(val); });
      } else {
        fastgltf::copyFromAccessor<uint32_t>(asset, index_accessor, indices.data());
      }

      // Calc tangents using Mikktspace
      if (!has_tangents) {
        CalcTangents(vertices, indices);
      }

      out_primitive.mesh_handle = renderer.AllocateMesh<Vertex>(vertices, indices, primitive_type);
      out_mesh.primitives.emplace_back(out_primitive);
    }
    out_model.meshes.emplace_back(out_mesh);
  }
  if (asset.scenes.size() != 1) {
    spdlog::error("model loader: multiple scenes not supported");
  }

  // TODO: child indices are wrong since skipping some nodes
  for (size_t node_idx : asset.scenes[0].nodeIndices) {
    auto& gltf_node = asset.nodes[node_idx];
    glm::quat rotation{};
    glm::vec3 translation{}, scale{1};
    if (auto* trs = std::get_if<fastgltf::TRS>(&asset.nodes[node_idx].transform)) {
      rotation = glm::make_quat(trs->rotation.data());
      translation = glm::make_vec3(trs->translation.data());
      scale = glm::make_vec3(trs->scale.data());
    } else if (std::array<float, 16>* arr =
                   std::get_if<std::array<float, 16>>(&asset.nodes[node_idx].transform)) {
      DecomposeMatrix(glm::make_mat4(arr->data()), translation, rotation, scale);
    }

    if (gltf_node.cameraIndex.has_value()) {
      // https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#projection-matrices
      glm::mat4 proj_mat;
      std::visit(fastgltf::visitor{
                     [&](fastgltf::Camera::Perspective& perspective) {
                       float aspect_ratio = perspective.aspectRatio.value_or(camera_aspect_ratio);
                       proj_mat = glm::mat4{0};
                       proj_mat[0][0] = 1.f / (aspect_ratio * tan(0.5f * perspective.yfov));
                       proj_mat[1][1] = 1.f / (tan(0.5f * perspective.yfov));
                       proj_mat[2][3] = -1;

                       if (perspective.zfar.has_value()) {
                         // Finite projection proj_matrix
                         proj_mat[2][2] = (*perspective.zfar + perspective.znear) /
                                          (perspective.znear - *perspective.zfar);
                         proj_mat[3][2] = (2 * *perspective.zfar * perspective.znear) /
                                          (perspective.znear - *perspective.zfar);
                       } else {
                         // Infinite projection proj_matrix
                         proj_mat[2][2] = -1;
                         proj_mat[3][2] = -2 * perspective.znear;
                       }
                     },
                     [&](fastgltf::Camera::Orthographic& orthographic) {
                       proj_mat = glm::mat4{1};
                       proj_mat[0][0] = 1.f / orthographic.xmag;
                       proj_mat[1][1] = 1.f / orthographic.ymag;
                       proj_mat[2][2] = 2.f / (orthographic.znear - orthographic.zfar);
                       proj_mat[3][2] = (orthographic.zfar + orthographic.znear) /
                                        (orthographic.znear - orthographic.zfar);
                     },
                 },
                 asset.cameras[gltf_node.cameraIndex.value()].camera);
      glm::mat4 view_matrix =
          glm::inverse(glm::translate(glm::mat4(1.0f), translation) * glm::toMat4(rotation));
      out_model.camera_data.emplace_back(proj_mat, view_matrix, translation);
      continue;
    }
    if (!gltf_node.meshIndex.has_value()) {
      spdlog::info("Non-mesh nodes not supported");
      continue;
    }
    out_model.nodes.emplace_back(SceneNode{
        .name = std::string(gltf_node.name.begin(), gltf_node.name.end()),
        .translation = translation,
        .rotation = rotation,
        .scale = scale,
        .child_indices = std::vector<size_t>(gltf_node.children.begin(), gltf_node.children.end()),
        .idx = node_idx,
        .mesh_idx = gltf_node.meshIndex.value_or(0)});

    if (!gltf_node.children.empty()) {
      spdlog::error("unimplemented: nodes have children");
    }
  }
  return out_model;
}

}  // namespace loader
