#include "CubeMapConverter.hpp"

#include <imgui.h>

#include "Path.hpp"
#include "Shape.hpp"
#include "gl/ShaderManager.hpp"
#include "gl/Texture.hpp"
#include "pch.hpp"
#include "types.hpp"

namespace {
constexpr glm::ivec2 kIrradianceDims = {128, 128};
constexpr glm::ivec2 kPrefilterDims = {256, 256};

}  // namespace
void CubeMapConverter::Init() {
  gl::ShaderManager::Get().AddShader(
      "equirectangular_to_cube",
      {{GET_SHADER_PATH("cubemap.vs.glsl"), gl::ShaderType::kVertex, {}},
       {GET_SHADER_PATH("equirectangular_to_cube.fs.glsl"), gl::ShaderType::kFragment, {}}});
  gl::ShaderManager::Get().AddShader(
      "irradiance_convolution",
      {{GET_SHADER_PATH("cubemap.vs.glsl"), gl::ShaderType::kVertex, {}},
       {GET_SHADER_PATH("ibl/irradiance_convolution.fs.glsl"), gl::ShaderType::kFragment, {}}});
  gl::ShaderManager::Get().AddShader(
      "prefilter_convolution",
      {{GET_SHADER_PATH("cubemap.vs.glsl"), gl::ShaderType::kVertex, {}},
       {GET_SHADER_PATH("ibl/prefilter_convolution.fs.glsl"), gl::ShaderType::kFragment, {}}});
  gl::ShaderManager::Get().AddShader(
      "env_map", {{GET_SHADER_PATH("env_map.vs.glsl"), gl::ShaderType::kVertex, {}},
                  {GET_SHADER_PATH("env_map.fs.glsl"), gl::ShaderType::kFragment, {}}});
  gl::ShaderManager::Get().AddShader(
      "brdf_lookup", {{GET_SHADER_PATH("ibl/brdf.vs.glsl"), gl::ShaderType::kVertex, {}},
                      {GET_SHADER_PATH("ibl/brdf.fs.glsl"), gl::ShaderType::kFragment, {}}});

  cube_pos_only_vao.Init();
  cube_pos_only_vao.EnableAttribute<float>(0, 3, 0);
  cube_pos_only_vbo.Init(sizeof(kCubePositionVertices) / (sizeof(float) * 3), 0,
                         &kCubePositionVertices);
  cube_pos_only_vao.AttachVertexBuffer(cube_pos_only_vbo.Id(), 0, 0, sizeof(VertexPosOnly));
  // TODO: RAII wrapper
  glCreateFramebuffers(1, &capture_fbo_);
  glCreateRenderbuffers(1, &capture_rbo_);

  glNamedRenderbufferStorage(capture_rbo_, GL_DEPTH_COMPONENT24, dims.x, dims.y);
  glNamedFramebufferRenderbuffer(capture_fbo_, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, capture_rbo_);
  if (glCheckNamedFramebufferStatus(capture_fbo_, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    spdlog::error("framebuffer incomplete");
  }

  env_cube_map.Load(gl::TexCubeCreateParamsEmpty{.dims = dims,
                                                 .internal_format = GL_RGB16F,
                                                 .wrap_s = GL_CLAMP_TO_EDGE,
                                                 .wrap_t = GL_CLAMP_TO_EDGE,
                                                 .wrap_r = GL_CLAMP_TO_EDGE,
                                                 .min_filter = GL_LINEAR,
                                                 .mag_filter = GL_LINEAR,
                                                 .gen_mipmaps = false});
  irradiance_map.Load(gl::TexCubeCreateParamsEmpty{.dims = kIrradianceDims,
                                                   .internal_format = GL_RGB16F,
                                                   .wrap_s = GL_CLAMP_TO_EDGE,
                                                   .wrap_t = GL_CLAMP_TO_EDGE,
                                                   .wrap_r = GL_CLAMP_TO_EDGE,
                                                   .min_filter = GL_LINEAR_MIPMAP_LINEAR,
                                                   .mag_filter = GL_LINEAR,
                                                   .gen_mipmaps = true});
  prefilter_map.Load(gl::TexCubeCreateParamsEmpty{.dims = kPrefilterDims,
                                                  .internal_format = GL_RGB16F,
                                                  .wrap_s = GL_CLAMP_TO_EDGE,
                                                  .wrap_t = GL_CLAMP_TO_EDGE,
                                                  .wrap_r = GL_CLAMP_TO_EDGE,
                                                  .min_filter = GL_LINEAR_MIPMAP_LINEAR,
                                                  .mag_filter = GL_LINEAR,
                                                  .gen_mipmaps = true});

  quad_vao_.Init();
  quad_vao_.EnableAttribute<float>(0, 3, offsetof(PosTexVertex, position));
  quad_vao_.EnableAttribute<float>(1, 2, offsetof(PosTexVertex, tex_coords));

  const std::array<PosTexVertex, 4> quad_vertices_full = {
      {// positions        // texture Coords
       {glm::vec3{-1, 1, 0.0f}, glm::vec2{0.0f, 1.0f}},
       {glm::vec3{-1, -1, 0.0f}, glm::vec2{0.0f, 0.0f}},
       {glm::vec3{1, 1, 0.0f}, glm::vec2{1.0f, 1.0f}},
       {glm::vec3{1, -1, 0.0f}, glm::vec2{1.0f, 0.0f}}}};
  std::vector<uint32_t> indices = {0, 1, 2, 2, 1, 3};
  std::vector<PosTexVertex> vertices = {quad_vertices_full.begin(), quad_vertices_full.end()};
  quad_vbo_.Init(sizeof(PosTexVertex) * 4, 0, vertices.data());
  quad_ebo_.Init(sizeof(uint32_t) * 6, 0, indices.data());
  quad_vao_.AttachVertexBuffer(quad_vbo_.Id(), 0, 0, sizeof(PosTexVertex));
  quad_vao_.AttachElementBuffer(quad_ebo_.Id());
}

void CubeMapConverter::RenderEquirectangularEnvMap(const gl::Texture& texture) {
  // put cube exactly in perspective
  const glm::mat4 capture_proj_matrix = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
  // view matrices look from the center at each side of the cube
  const std::array<glm::mat4, 6> capture_view_matrices = {
      glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f),
                  glm::vec3(0.0f, -1.0f, 0.0f)),
      glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f),
                  glm::vec3(0.0f, -1.0f, 0.0f)),
      glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f),
                  glm::vec3(0.0f, 0.0f, 1.0f)),
      glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f),
                  glm::vec3(0.0f, 0.0f, -1.0f)),
      glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f),
                  glm::vec3(0.0f, -1.0f, 0.0f)),
      glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f),
                  glm::vec3(0.0f, -1.0f, 0.0f))};

  glBindFramebuffer(GL_FRAMEBUFFER, capture_fbo_);
  cube_pos_only_vao.Bind();

  auto draw_cube_map = [&capture_view_matrices, this](gl::Shader& shader, const gl::Texture& tex) {
    for (int i = 0; i < 6; i++) {
      shader.SetMat4("u_view", capture_view_matrices[i]);
      glNamedFramebufferTexture2DEXT(capture_fbo_, GL_COLOR_ATTACHMENT0,
                                     GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, tex.Id(), 0);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      glDrawArrays(GL_TRIANGLES, 0, 36);
    }
  };

  // equirectangular to cube render
  gl::Shader equirect_to_cube_shader =
      gl::ShaderManager::Get().GetShader("equirectangular_to_cube").value();
  equirect_to_cube_shader.Bind();
  equirect_to_cube_shader.SetMat4("u_projection", capture_proj_matrix);
  texture.Bind(0);
  glViewport(0, 0, dims.x, dims.y);
  draw_cube_map(equirect_to_cube_shader, env_cube_map);

  // irradiance map render
  glNamedRenderbufferStorage(capture_rbo_, GL_DEPTH_COMPONENT24, kIrradianceDims.x,
                             kIrradianceDims.y);
  gl::Shader irradiance_shader =
      gl::ShaderManager::Get().GetShader("irradiance_convolution").value();
  irradiance_shader.Bind();
  irradiance_shader.SetMat4("u_projection", capture_proj_matrix);
  env_cube_map.Bind(0);
  glViewport(0, 0, kIrradianceDims.x, kIrradianceDims.y);
  draw_cube_map(irradiance_shader, irradiance_map);

  // prefilter maps render
  gl::Shader prefilter_shader = gl::ShaderManager::Get().GetShader("prefilter_convolution").value();
  prefilter_shader.Bind();
  prefilter_shader.SetMat4("u_projection", capture_proj_matrix);
  env_cube_map.Bind(0);
  constexpr const uint32_t kMaxMipLevels = 6;
  for (uint32_t mip = 0; mip < kMaxMipLevels; ++mip) {
    uint32_t mip_width = kPrefilterDims.x * std::pow(0.5, mip);
    uint32_t mip_height = kPrefilterDims.y * std::pow(0.5, mip);
    glNamedRenderbufferStorage(capture_rbo_, GL_DEPTH_COMPONENT24, mip_width, mip_height);
    glViewport(0, 0, mip_width, mip_height);
    float roughness = static_cast<float>(mip) / static_cast<float>(kMaxMipLevels - 1);
    prefilter_shader.SetFloat("roughness", roughness);

    for (int i = 0; i < 6; i++) {
      prefilter_shader.SetMat4("u_view", capture_view_matrices[i]);
      glNamedFramebufferTexture2DEXT(capture_fbo_, GL_COLOR_ATTACHMENT0,
                                     GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, prefilter_map.Id(), mip);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      glDrawArrays(GL_TRIANGLES, 0, 36);
    }
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  RenderBRDFLookupTexture();
}

void CubeMapConverter::Draw(const gl::Texture& tex) const {
  glDepthFunc(GL_LEQUAL);
  gl::Shader shader = gl::ShaderManager::Get().GetShader("env_map").value();
  shader.Bind();

  static float lod = 1.2;
  ImGui::Begin("LOD");
  ImGui::SliderFloat("LOD", &lod, 0, 4.99);
  ImGui::End();

  shader.SetFloat("lod", lod);

  tex.Bind(0);
  cube_pos_only_vao.Bind();
  glDrawArrays(GL_TRIANGLES, 0, 36);
  glDepthFunc(GL_LESS);
}

void CubeMapConverter::DrawPrefilter() const { Draw(prefilter_map); }
void CubeMapConverter::DrawIrradiance() const { Draw(irradiance_map); }
void CubeMapConverter::Draw() const { Draw(env_cube_map); }
void CubeMapConverter::DrawBRDFTexture() {}

void CubeMapConverter::RenderBRDFLookupTexture() {
  constexpr const glm::ivec2 kBrdfLookupDims = {1024, 1024};
  brdf_lookup_tex.Load(gl::Tex2DCreateInfoEmpty{.dims = kBrdfLookupDims,
                                                .wrap_s = GL_CLAMP_TO_EDGE,
                                                .wrap_t = GL_CLAMP_TO_EDGE,
                                                .internal_format = GL_RG16,
                                                .min_filter = GL_LINEAR,
                                                .mag_filter = GL_LINEAR});

  gl::Shader brdf_shader = gl::ShaderManager::Get().GetShader("brdf_lookup").value();
  brdf_shader.Bind();
  quad_vao_.Bind();
  glBindFramebuffer(GL_FRAMEBUFFER, capture_fbo_);
  glNamedRenderbufferStorage(capture_rbo_, GL_DEPTH_COMPONENT24, kBrdfLookupDims.x,
                             kBrdfLookupDims.y);
  glNamedFramebufferTexture2DEXT(capture_fbo_, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                 brdf_lookup_tex.Id(), 0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glViewport(0, 0, kBrdfLookupDims.x, kBrdfLookupDims.y);
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
