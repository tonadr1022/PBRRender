#pragma once

#include "gl/Buffer.hpp"
#include "gl/DynamicBuffer.hpp"
#include "gl/VertexArray.hpp"
#include "types.hpp"

struct RenderInfo {
  glm::mat4 view_matrix;
  glm::mat4 projection_matrix;
};

class Renderer {
 public:
  void Init();
  template <typename VertexType, typename AllocFunc>
  [[nodiscard]] AssetHandle AllocateMeshVertices(uint32_t count, PrimitiveType primitive_type,
                                                 AllocFunc func) {
    if (primitive_type != PrimitiveType::kTriangles) {
      spdlog::error("Primitive Type Not supported: {}", static_cast<int>(primitive_type));
      return 0;
    }
    uint32_t mesh_handle = next_mesh_handle_++;
    if constexpr (std::is_same_v<VertexType, Vertex>) {
      uint32_t offset;
      uint32_t handle = pos_tex_vbo_.Allocate(count, offset, func);
      mesh_allocs_map_.emplace(handle,
                               VertexIndexAlloc{.vertex_handle = handle, .index_handle = 0});
      dei_cmds_map_.try_emplace(
          mesh_handle, DrawElementsIndirectCommand{
                           .count = UINT32_MAX,
                           .instance_count = 0,
                           .first_index = UINT32_MAX,
                           .base_vertex = static_cast<uint32_t>(offset / sizeof(VertexType)),
                           .base_instance = 0,
                       });
    }
    return mesh_handle;
  }

  template <typename IndexType, typename AllocFunc>
  void AllocateMeshIndices(AssetHandle mesh_handle, uint32_t count, AllocFunc func) {
    auto alloc_it = mesh_allocs_map_.find(mesh_handle);
    if (alloc_it == mesh_allocs_map_.end()) {
      spdlog::error(
          "mesh alloc not found. Must allocate vertices first and use the returned handle");
      EASSERT(0);
    }
    if (alloc_it->second.index_handle != 0) {
      // TODO: decide whether it's better to free?
      spdlog::error("mesh alloc already has an index allocation");
      return;
    }
    uint32_t offset;
    if constexpr (std::is_same_v<IndexType, uint32_t>) {
      alloc_it->second.index_handle = index_buffer_32_.Allocate(count, offset, func);
      alloc_it->second.index_buffer_idx = 1;
    } else if constexpr (std::is_same_v<IndexType, uint16_t>) {
      alloc_it->second.index_handle = index_buffer_16_.Allocate(count, offset, func);
      alloc_it->second.index_buffer_idx = 0;
    } else {
      spdlog::error("Index type must be uint32_t");
      EASSERT(0);
    }

    auto dei_it = dei_cmds_map_.find(mesh_handle);
    EASSERT(dei_it != dei_cmds_map_.end());
    dei_it->second.first_index = offset / sizeof(IndexType);
    dei_it->second.count = count;
  }

  [[nodiscard]] AssetHandle AllocateMaterial(const Material& material, AlphaMode alpha_mode);
  void FreeMesh(AssetHandle& handle);
  void FreeMaterial(AssetHandle& handle);
  void SubmitStaticModel(Model& model, const glm::mat4& model_matrix);
  void SubmitStaticInstancedModel(const Model& model, const std::vector<glm::mat4>& model_matrices);
  void ResetStaticDrawCommands();
  void DrawStaticOpaque(const RenderInfo& render_info);
  uint32_t NumMaterials() const;
  uint32_t NumMeshes() const;

 private:
  struct DrawElementsIndirectCommand {
    uint32_t count;
    uint32_t instance_count;
    uint32_t first_index;
    uint32_t base_vertex;
    uint32_t base_instance;
  };

  gl::DynamicBuffer<Vertex> pos_tex_vbo_;
  gl::VertexArray pos_tex_16_vao_;
  gl::DynamicBuffer<uint32_t> index_buffer_32_;
  gl::DynamicBuffer<uint16_t> index_buffer_16_;
  gl::DynamicBuffer<Material> material_ssbo_;

  struct VertexIndexAlloc {
    uint32_t vertex_handle{};
    uint32_t index_handle{};
    // indices pointing the proper buffers to free from
    uint16_t vertex_buffer_idx{};
    // 0 for uint16_t, 1 for uint32_t
    uint16_t index_buffer_idx{};
  };

  // NEED alignas 16 to match GPU padding... 30 minutes wasted, skill issue!
  struct alignas(16) DrawCmdUniforms {
    glm::mat4 model;
    uint32_t material_index;
  };

  gl::Buffer<DrawCmdUniforms> static_uniforms_ssbo_;
  gl::Buffer<DrawElementsIndirectCommand> static_dei_cmds_buffer_;
  bool static_allocs_dirty_{true};

  std::unordered_map<AssetHandle, uint32_t> material_allocs_map_;
  std::unordered_map<AssetHandle, VertexIndexAlloc> mesh_allocs_map_;
  std::unordered_map<AssetHandle, DrawElementsIndirectCommand> dei_cmds_map_;
  uint32_t next_mesh_handle_{1};
};
