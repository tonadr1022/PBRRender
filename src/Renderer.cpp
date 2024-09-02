#include "Renderer.hpp"

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
  pos_tex_vbo_.Init(1000000, sizeof(Vertex));

  pos_tex_32_vao_.Init();
  pos_tex_32_vao_.EnableAttribute<float>(0, 3, offsetof(Vertex, position));
  pos_tex_32_vao_.EnableAttribute<float>(1, 3, offsetof(Vertex, normal));
  pos_tex_32_vao_.EnableAttribute<float>(2, 3, offsetof(Vertex, tangent));
  pos_tex_32_vao_.EnableAttribute<float>(3, 2, offsetof(Vertex, uv));
  index_buffer_32_.Init(1000000, sizeof(uint32_t));
  pos_tex_32_vao_.AttachVertexBuffer(pos_tex_vbo_.Id(), 0, 0, sizeof(Vertex));
  pos_tex_32_vao_.AttachElementBuffer(index_buffer_32_.Id());

  pos_tex_16_vao_.Init();
  pos_tex_16_vao_.EnableAttribute<float>(0, 3, offsetof(Vertex, position));
  pos_tex_16_vao_.EnableAttribute<float>(1, 3, offsetof(Vertex, normal));
  pos_tex_16_vao_.EnableAttribute<float>(2, 3, offsetof(Vertex, tangent));
  pos_tex_16_vao_.EnableAttribute<float>(3, 2, offsetof(Vertex, uv));
  pos_tex_16_vao_.AttachVertexBuffer(pos_tex_vbo_.Id(), 0, 0, sizeof(Vertex));
  index_buffer_16_.Init(1000000, sizeof(uint16_t));
  pos_tex_16_vao_.AttachElementBuffer(index_buffer_16_.Id());
  index_buffer_32_.Init(1000000, sizeof(uint32_t));

  material_ssbo_.Init(3000, sizeof(Material));
  static_16_bit_idx_dei_cmds_buffer_.Init(2000, GL_DYNAMIC_STORAGE_BIT, nullptr);
  static_16_bit_idx_uniforms_ssbo_.Init(2000, GL_DYNAMIC_STORAGE_BIT, nullptr);
  static_32_bit_idx_dei_cmds_buffer_.Init(2000, GL_DYNAMIC_STORAGE_BIT, nullptr);
  static_32_bit_idx_uniforms_ssbo_.Init(2000, GL_DYNAMIC_STORAGE_BIT, nullptr);
}

AssetHandle Renderer::AllocateMaterial(const Material& material, AlphaMode) {
  // TODO: handle opaque vs blend
  // TODO: templatize for other material types?
  uint32_t offset;
  uint32_t mat_handle =
      material_ssbo_.Allocate(1, reinterpret_cast<const void*>(&material), offset);
  material_allocs_map_.emplace(mat_handle, offset / sizeof(Material));
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

  switch (alloc.index_buffer_idx) {
    case 0:
      index_buffer_16_.Free(alloc.index_handle);
      break;
    case 1:
      index_buffer_32_.Free(alloc.index_handle);
  }

  switch (alloc.vertex_buffer_idx) {
    case 0:
      pos_tex_vbo_.Free(handle);
  }

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
size_t static_16_base_instance = 0;
size_t static_32_base_instance = 0;
}  // namespace

void Renderer::SubmitStaticInstancedModel(const Model& model,
                                          const std::vector<glm::mat4>& model_matrices) {
  for (const Primitive& primitive : model.primitives) {
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

    if (mesh_it->second.index_buffer_idx == 0) {
      mesh_dei_cmd.base_instance = static_16_base_instance;
      static_16_base_instance += mesh_dei_cmd.instance_count;
      static_16_bit_idx_uniforms_ssbo_.SubData(uniforms.size(), uniforms.data());
      static_16_bit_idx_dei_cmds_buffer_.SubData(1, &mesh_dei_cmd);
    } else {
      mesh_dei_cmd.base_instance = static_32_base_instance;
      static_32_base_instance += mesh_dei_cmd.instance_count;
      static_32_bit_idx_uniforms_ssbo_.SubData(uniforms.size(), uniforms.data());
      static_32_bit_idx_dei_cmds_buffer_.SubData(1, &mesh_dei_cmd);
    }
    static_allocs_dirty_ = true;
  }
}

void Renderer::ResetStaticDrawCommands() {
  static_16_bit_idx_uniforms_ssbo_.ResetOffset();
  static_16_bit_idx_dei_cmds_buffer_.ResetOffset();
  static_16_base_instance = 0;
  static_32_base_instance = 0;
}

void Renderer::SubmitStaticModel(Model& model, const glm::mat4& model_matrix) {
  // TODO: one alloc after all primtitives
  for (const Primitive& primitive : model.primitives) {
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
    glm::mat4 normal_matrix = glm::transpose(glm::inverse(glm::mat3(model_matrix)));
    DrawCmdUniforms uniform{
        .model = model_matrix, .normal_matrix = normal_matrix, .material_index = mat_it->second};
    DrawElementsIndirectCommand mesh_dei_cmd = dei_cmds_map_.at(primitive.mesh_handle);
    mesh_dei_cmd.instance_count = 1;

    if (mesh_it->second.index_buffer_idx == 0) {
      mesh_dei_cmd.base_instance = static_16_base_instance;
      static_16_base_instance++;
      static_16_bit_idx_uniforms_ssbo_.SubData(1, &uniform);
      static_16_bit_idx_dei_cmds_buffer_.SubData(1, &mesh_dei_cmd);
    } else {
      mesh_dei_cmd.base_instance = static_32_base_instance;
      static_32_base_instance++;
      static_32_bit_idx_uniforms_ssbo_.SubData(1, &uniform);
      static_32_bit_idx_dei_cmds_buffer_.SubData(1, &mesh_dei_cmd);
    }
    static_allocs_dirty_ = true;
  }
}

void Renderer::DrawStaticOpaque(const RenderInfo& render_info) {
  UBOUniforms uniform_data{.vp_matrix = render_info.projection_matrix * render_info.view_matrix,
                           .view_pos = render_info.view_pos};
  uniform_ubo_.SubDataStart(1, &uniform_data);
  uniform_ubo_.BindBase(GL_UNIFORM_BUFFER, 0);

  static_16_bit_idx_uniforms_ssbo_.BindBase(GL_SHADER_STORAGE_BUFFER, 0);
  static_16_bit_idx_dei_cmds_buffer_.Bind(GL_DRAW_INDIRECT_BUFFER);
  material_ssbo_.BindBase(GL_SHADER_STORAGE_BUFFER, 1);
  pos_tex_16_vao_.Bind();
  glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_SHORT, nullptr,
                              static_16_bit_idx_dei_cmds_buffer_.NumAllocs(), 0);

  static_32_bit_idx_uniforms_ssbo_.BindBase(GL_SHADER_STORAGE_BUFFER, 0);
  static_32_bit_idx_dei_cmds_buffer_.Bind(GL_DRAW_INDIRECT_BUFFER);
  material_ssbo_.BindBase(GL_SHADER_STORAGE_BUFFER, 1);
  pos_tex_32_vao_.Bind();
  glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, nullptr,
                              static_32_bit_idx_dei_cmds_buffer_.NumAllocs(), 0);
}

uint32_t Renderer::NumMaterials() const { return material_allocs_map_.size(); }

uint32_t Renderer::NumMeshes() const { return mesh_allocs_map_.size(); }
