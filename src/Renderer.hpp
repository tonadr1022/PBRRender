#pragma once

#include "gl/Buffer.hpp"
#include "gl/DynamicBuffer.hpp"
#include "gl/VertexArray.hpp"
#include "types.hpp"

struct RenderInfo {
  glm::mat4 view_matrix;
  glm::mat4 projection_matrix;
  glm::vec3 view_pos;
};

class Renderer {
 public:
  void Init();

  template <typename VertexType>
  [[nodiscard]] AssetHandle AllocateMesh(std::vector<VertexType>& vertices,
                                         std::vector<uint32_t>& indices,
                                         PrimitiveType primitive_type) {
    ZoneScoped;
    if (primitive_type != PrimitiveType::kTriangles) {
      spdlog::error("Primitive Type Not supported: {}", static_cast<int>(primitive_type));
      return 0;
    }
    uint32_t mesh_handle = next_mesh_handle_++;
    if constexpr (std::is_same_v<VertexType, Vertex>) {
      uint32_t vbo_offset;
      uint32_t vbo_handle = pos_tex_vbo_.Allocate(vertices.size(), vertices.data(), vbo_offset);
      if (vbo_handle == 0) {
        spdlog::error("Failed to allocate vertices");
        return 0;
      }
      uint32_t ebo_offset;
      uint32_t ebo_handle = index_buffer_.Allocate(indices.size(), indices.data(), ebo_offset);
      if (ebo_handle == 0) {
        spdlog::error("Failed to allocate indices");
        return 0;
      }
      mesh_allocs_map_.emplace(
          vbo_handle, VertexIndexAlloc{.vertex_handle = vbo_handle, .index_handle = ebo_handle});
      dei_cmds_map_.try_emplace(
          mesh_handle, DrawElementsIndirectCommand{
                           .count = static_cast<uint32_t>(indices.size()),
                           .instance_count = 0,
                           .first_index = static_cast<uint32_t>(ebo_offset / sizeof(uint32_t)),
                           .base_vertex = static_cast<uint32_t>(vbo_offset / sizeof(VertexType)),
                           .base_instance = 0,
                       });
    }
    return mesh_handle;
  }

  [[nodiscard]] AssetHandle AllocateMaterial(const Material& material, AlphaMode alpha_mode);
  void FreeMesh(AssetHandle& handle);
  void FreeMaterial(AssetHandle& handle);
  void SubmitStaticModel(Model& model, const glm::mat4& model_matrix);
  void SubmitStaticInstancedModel(const Mesh& mesh, const std::vector<glm::mat4>& model_matrices);
  void ResetStaticDrawCommands();
  void SubmitPointLights(const std::vector<PointLight>& lights);
  void EditPointLight(const PointLight& light, size_t idx);
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

  struct UBOUniforms {
    glm::mat4 vp_matrix;
    glm::vec3 view_pos;
  };

  gl::Buffer<UBOUniforms> uniform_ubo_;
  gl::DynamicBuffer<Vertex> pos_tex_vbo_;
  gl::VertexArray pos_tex_vao_;
  gl::DynamicBuffer<uint32_t> index_buffer_;
  gl::DynamicBuffer<Material> material_ssbo_;
  gl::Buffer<PointLight> point_lights_ssbo_;

  struct VertexIndexAlloc {
    uint32_t vertex_handle{};
    uint32_t index_handle{};
  };

  // NEED alignas 16 to match GPU padding... 30 minutes wasted, skill issue!
  struct alignas(16) DrawCmdUniforms {
    glm::mat4 model;
    glm::mat4 normal_matrix;
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
