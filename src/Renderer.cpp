#include "Renderer.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include "gl/OpenGLDebug.hpp"
#include "types.hpp"

namespace {

const std::vector<float> kQuadVertices = {
    -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
    1.0f,  1.0f, 0.0f, 1.0f, 1.0f, 1.0f,  -1.0f, 0.0f, 1.0f, 0.0f,
};
}

void Renderer::Init() {
  glEnable(GL_DEBUG_OUTPUT);
  glDebugMessageCallback(gl::MessageCallback, nullptr);
  uniform_ubo_.Init(1, GL_DYNAMIC_STORAGE_BIT, nullptr);
  pos_tex_vbo_.Init(10000000, sizeof(Vertex));

  pos_tex_vao_.Init();
  pos_tex_vao_.EnableAttribute<float>(0, 3, offsetof(Vertex, position));
  pos_tex_vao_.EnableAttribute<float>(1, 3, offsetof(Vertex, normal));
  pos_tex_vao_.EnableAttribute<float>(2, 3, offsetof(Vertex, tangent));
  pos_tex_vao_.EnableAttribute<float>(3, 2, offsetof(Vertex, uv));
  pos_tex_vao_.AttachVertexBuffer(pos_tex_vbo_.Id(), 0, 0, sizeof(Vertex));
  index_buffer_.Init(10000000, sizeof(uint32_t));
  pos_tex_vao_.AttachElementBuffer(index_buffer_.Id());

  material_ssbo_.Init(3000, sizeof(Material));
  static_dei_cmds_buffer_.Init(2000, GL_DYNAMIC_STORAGE_BIT, nullptr);
  static_uniforms_ssbo_.Init(2000, GL_DYNAMIC_STORAGE_BIT, nullptr);
  point_lights_ssbo_.Init(20, sizeof(PointLight));
  std::vector<PointLight> point_lights = {PointLight{
      .position = glm::vec3{0, 0, 0}, ._pad1 = 0, .color = glm::vec3{1, 1, 1}, .intensity = 100}};
  uint32_t offset = 0;
  uint32_t _ = point_lights_ssbo_.Allocate(point_lights.size(), point_lights.data(), offset);
}

AssetHandle Renderer::AllocateMaterial(const Material& material, AlphaMode) {
  // TODO: handle opaque vs blend
  // TODO: templatize for other material types?
  uint32_t offset;
  uint32_t mat_handle =
      material_ssbo_.Allocate(1, reinterpret_cast<const void*>(&material), offset);
  if (mat_handle == 0) {
    spdlog::error("Failed to allocate material");
  } else {
    material_allocs_map_.emplace(mat_handle, offset / sizeof(Material));
  }
  return mat_handle;
}

void Renderer::FreeMesh(AssetHandle& handle) {
  if (handle == 0) return;
  auto it = mesh_allocs_map_.find(handle);
  if (it == mesh_allocs_map_.end()) {
    spdlog::error("Mesh handle not found");
    return;
  }

  auto& alloc = it->second;
  index_buffer_.Free(alloc.index_handle);

  // TODO: handle diff vertex types?
  pos_tex_vbo_.Free(handle);

  mesh_allocs_map_.erase(it);
  dei_cmds_map_.erase(handle);
  handle = 0;
}

void Renderer::FreeMaterial(AssetHandle& handle) {
  if (handle == 0) return;
  auto it = material_allocs_map_.find(handle);
  if (it == material_allocs_map_.end()) {
    spdlog::error("Material handle not found");
    EASSERT(0);
  }
  material_ssbo_.Free(handle);
  material_allocs_map_.erase(it);
  handle = 0;
}

namespace {
size_t static_base_instance = 0;
}  // namespace

void Renderer::SubmitStaticInstancedModel(const Mesh& mesh,
                                          const std::vector<glm::mat4>& model_matrices) {
  for (const Primitive& primitive : mesh.primitives) {
    auto mesh_it = mesh_allocs_map_.find(primitive.mesh_handle);
    if (mesh_it == mesh_allocs_map_.end()) {
      spdlog::error("mesh not found");
      continue;
    }
    auto mat_it = material_allocs_map_.find(primitive.material_handle);
    if (mat_it == material_allocs_map_.end()) {
      spdlog::error("material not found");
      continue;
    }
    std::vector<DrawCmdUniforms> uniforms;
    uniforms.reserve(model_matrices.size());
    for (const auto& model_matrix : model_matrices) {
      glm::mat4 normal_matrix = glm::transpose(glm::inverse(glm::mat3(model_matrix)));
      uniforms.emplace_back(DrawCmdUniforms{
          .model = model_matrix,
          .normal_matrix = normal_matrix,
          .material_index = mat_it->second,
      });
    }
    // static_16_bit_idx_uniforms_ssbo_.SubData(uniforms.size(), uniforms.data());
    DrawElementsIndirectCommand mesh_dei_cmd = dei_cmds_map_.at(primitive.mesh_handle);
    mesh_dei_cmd.instance_count = uniforms.size();
    // static_16_bit_idx_dei_cmds_buffer_.SubData(1, &mesh_dei_cmd);

    mesh_dei_cmd.base_instance = static_base_instance;
    static_base_instance += mesh_dei_cmd.instance_count;
    static_uniforms_ssbo_.SubData(uniforms.size(), uniforms.data());
    static_dei_cmds_buffer_.SubData(1, &mesh_dei_cmd);

    static_allocs_dirty_ = true;
  }
}

void Renderer::ResetStaticDrawCommands() {
  static_dei_cmds_buffer_.ResetOffset();
  static_uniforms_ssbo_.ResetOffset();
  static_base_instance = 0;
}

void Renderer::SubmitStaticModel(Model& model, const glm::mat4& model_matrix) {
  for (const SceneNode& node : model.nodes) {
    auto& mesh = model.meshes[node.mesh_idx];

    for (const Primitive& primitive : mesh.primitives) {
      auto mesh_it = mesh_allocs_map_.find(primitive.mesh_handle);
      if (mesh_it == mesh_allocs_map_.end()) {
        spdlog::error("mesh not found");
        continue;
      }
      auto mat_it = material_allocs_map_.find(primitive.material_handle);
      if (mat_it == material_allocs_map_.end()) {
        spdlog::error("material not found");
        continue;
      }
      glm::mat4 transformed_model_matrix =
          glm::translate(glm::toMat4(node.rotation) * glm::scale(glm::mat4(1), node.scale),
                         node.translation) *
          model_matrix;
      glm::mat4 normal_matrix = glm::transpose(glm::inverse(glm::mat3(transformed_model_matrix)));
      DrawCmdUniforms uniform{.model = transformed_model_matrix,
                              .normal_matrix = normal_matrix,
                              .material_index = mat_it->second};
      DrawElementsIndirectCommand mesh_dei_cmd = dei_cmds_map_.at(primitive.mesh_handle);
      mesh_dei_cmd.instance_count = 1;

      mesh_dei_cmd.base_instance = static_base_instance;
      static_base_instance++;
      static_uniforms_ssbo_.SubData(1, &uniform);
      static_dei_cmds_buffer_.SubData(1, &mesh_dei_cmd);
      static_allocs_dirty_ = true;
    }
  }
}

void Renderer::DrawStaticOpaque(const RenderInfo& render_info) {
  // TODO: separate back face cull vs no cull
  // glEnable(GL_CULL_FACE);
  // glCullFace(GL_BACK);
  UBOUniforms uniform_data{.vp_matrix = render_info.projection_matrix * render_info.view_matrix,
                           .view_pos = render_info.view_pos};
  uniform_ubo_.SubDataStart(1, &uniform_data);
  uniform_ubo_.BindBase(GL_UNIFORM_BUFFER, 0);
  material_ssbo_.BindBase(GL_SHADER_STORAGE_BUFFER, 1);
  point_lights_ssbo_.BindBase(GL_SHADER_STORAGE_BUFFER, 2);

  pos_tex_vao_.Bind();
  static_uniforms_ssbo_.BindBase(GL_SHADER_STORAGE_BUFFER, 0);
  static_dei_cmds_buffer_.Bind(GL_DRAW_INDIRECT_BUFFER);
  glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, nullptr,
                              static_dei_cmds_buffer_.NumAllocs(), 0);
}

uint32_t Renderer::NumMaterials() const { return material_allocs_map_.size(); }

uint32_t Renderer::NumMeshes() const { return mesh_allocs_map_.size(); }
