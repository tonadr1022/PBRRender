#include "Renderer.hpp"

#include "Path.hpp"
#include "gl/OpenGLDebug.hpp"
#include "gl/ShaderManager.hpp"
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
  gl::ShaderManager::Get().AddShader(
      "textured", {{GET_SHADER_PATH("textured.vs.glsl"), gl::ShaderType::kVertex, {}},
                   {GET_SHADER_PATH("textured.fs.glsl"), gl::ShaderType::kFragment, {}}});

  pos_tex_16_vao_.Init();
  pos_tex_16_vao_.EnableAttribute<float>(0, 3, offsetof(Vertex, position));
  pos_tex_16_vao_.EnableAttribute<float>(1, 2, offsetof(Vertex, uv));
  pos_tex_vbo_.Init(100000, sizeof(Vertex));
  pos_tex_16_vao_.AttachVertexBuffer(pos_tex_vbo_.Id(), 0, 0, sizeof(Vertex));
  index_buffer_16_.Init(1000000, sizeof(uint16_t));
  pos_tex_16_vao_.AttachElementBuffer(index_buffer_16_.Id());

  index_buffer_32_.Init(100000, sizeof(uint32_t));
  material_ssbo_.Init(300, sizeof(Material));
  static_dei_cmds_buffer_.Init(sizeof(DrawElementsIndirectCommand) * 20000, GL_DYNAMIC_STORAGE_BIT,
                               nullptr);
  static_uniforms_ssbo_.Init(sizeof(DrawCmdUniforms) * 20000, GL_DYNAMIC_STORAGE_BIT, nullptr);
}

AssetHandle Renderer::AllocateMaterial(const Material& material, AlphaMode) {
  // TODO: handle opaque vs blend
  // TODO: templatize for other material types?
  uint32_t offset;
  uint32_t mat_handle =
      material_ssbo_.Allocate(1, reinterpret_cast<const void*>(&material), offset);
  spdlog::info("offset: {}", offset);
  static int i = 0;
  material_allocs_map_.emplace(mat_handle, i++);
  // material_allocs_map_.emplace(mat_handle, offset / sizeof(Material));
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
size_t static_base_instance = 0;
}

void Renderer::SubmitStaticInstancedModel(Model& model,
                                          const std::vector<glm::mat4>& model_matrices) {
  for (const Primitive& primitive : model.primitives) {
    auto mat_it = material_allocs_map_.find(primitive.material_handle);
    if (mat_it == material_allocs_map_.end()) {
      spdlog::error("material not found");
      continue;
    }
    std::vector<DrawCmdUniforms> uniforms;
    uniforms.reserve(model_matrices.size());
    for (const auto& model_matrix : model_matrices) {
      uniforms.emplace_back(
          DrawCmdUniforms{.model = model_matrix, .material_index = mat_it->second, .pad = {}});
    }
    static_uniforms_ssbo_.SubData(uniforms.size(), uniforms.data());
    DrawElementsIndirectCommand mesh_dei_cmd = dei_cmds_map_.at(primitive.mesh_handle);
    mesh_dei_cmd.instance_count = uniforms.size();
    mesh_dei_cmd.base_instance = static_base_instance;
    static_base_instance += mesh_dei_cmd.instance_count;
    static_dei_cmds_buffer_.SubData(1, &mesh_dei_cmd);
    static_allocs_dirty_ = true;
  }
}

void Renderer::ResetStaticDrawCommands() {
  static_uniforms_ssbo_.ResetOffset();
  static_dei_cmds_buffer_.ResetOffset();
  static_base_instance = 0;
}

void Renderer::SubmitStaticModel(Model& model, const glm::mat4& model_matrix) {
  for (const Primitive& primitive : model.primitives) {
    auto mat_it = material_allocs_map_.find(primitive.material_handle);
    if (mat_it == material_allocs_map_.end()) {
      spdlog::error("material not found");
      continue;
    }
    DrawCmdUniforms uniform{.model = model_matrix, .material_index = mat_it->second, .pad = {}};
    spdlog::info("{}", uniform.material_index);
    static_uniforms_ssbo_.SubData(1, &uniform);
    DrawElementsIndirectCommand mesh_dei_cmd = dei_cmds_map_.at(primitive.mesh_handle);
    mesh_dei_cmd.instance_count = 1;
    mesh_dei_cmd.base_instance = static_base_instance;
    static_base_instance += mesh_dei_cmd.instance_count;
    static_dei_cmds_buffer_.SubData(1, &mesh_dei_cmd);
    static_allocs_dirty_ = true;
  }
}

void Renderer::DrawStaticOpaque(const RenderInfo& render_info) {
  auto shader = gl::ShaderManager::Get().GetShader("textured").value();
  shader.Bind();
  shader.SetMat4("u_vp_matrix", render_info.projection_matrix * render_info.view_matrix);

  static_uniforms_ssbo_.BindBase(GL_SHADER_STORAGE_BUFFER, 0);
  static_dei_cmds_buffer_.Bind(GL_DRAW_INDIRECT_BUFFER);
  material_ssbo_.BindBase(GL_SHADER_STORAGE_BUFFER, 1);

  pos_tex_16_vao_.Bind();
  glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_SHORT, nullptr,
                              static_dei_cmds_buffer_.NumAllocs(), 0);
}

uint32_t Renderer::NumMaterials() const { return material_allocs_map_.size(); }

uint32_t Renderer::NumMeshes() const { return mesh_allocs_map_.size(); }
