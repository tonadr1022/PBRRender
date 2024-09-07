#include "CubeMapConverter.hpp"

#include "Path.hpp"
#include "Shape.hpp"
#include "gl/ShaderManager.hpp"
#include "pch.hpp"

namespace {
glm::ivec2 irradiance_dims = {32, 32};
}
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
      "env_map", {{GET_SHADER_PATH("env_map.vs.glsl"), gl::ShaderType::kVertex, {}},
                  {GET_SHADER_PATH("env_map.fs.glsl"), gl::ShaderType::kFragment, {}}});
  cube_pos_only_vao.Init();
  cube_pos_only_vao.EnableAttribute<float>(0, 3, 0);
  cube_pos_only_vbo.Init(sizeof(kCubePositionVertices) / (sizeof(float) * 3), 0,
                         &kCubePositionVertices);
  cube_pos_only_vao.AttachVertexBuffer(cube_pos_only_vbo.Id(), 0, 0, sizeof(VertexPosOnly));
  // TODO: RAII wrapper
  glCreateFramebuffers(1, &capture_fbo);
  glCreateRenderbuffers(1, &capture_rbo);

  glNamedRenderbufferStorage(capture_rbo, GL_DEPTH_COMPONENT24, dims.x, dims.y);
  glNamedFramebufferRenderbuffer(capture_fbo, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, capture_rbo);
  if (glCheckNamedFramebufferStatus(capture_fbo, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    spdlog::error("framebuffer incomplete");
  }

  env_cube_map.Load(gl::TexCubeCreateParamsEmpty{.dims = dims,
                                                 .internal_format = GL_RGB16F,
                                                 .wrap_s = GL_CLAMP_TO_EDGE,
                                                 .wrap_t = GL_CLAMP_TO_EDGE,
                                                 .wrap_r = GL_CLAMP_TO_EDGE,
                                                 .min_filter = GL_LINEAR,
                                                 .mag_filter = GL_LINEAR});
  irradiance_map.Load(gl::TexCubeCreateParamsEmpty{.dims = irradiance_dims,
                                                   .internal_format = GL_RGB16F,
                                                   .wrap_s = GL_CLAMP_TO_EDGE,
                                                   .wrap_t = GL_CLAMP_TO_EDGE,
                                                   .wrap_r = GL_CLAMP_TO_EDGE,
                                                   .min_filter = GL_LINEAR,
                                                   .mag_filter = GL_LINEAR});
}

void CubeMapConverter::RenderEquirectangularEnvMap(const gl::Texture& texture) const {
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
  gl::Shader shader = gl::ShaderManager::Get().GetShader("equirectangular_to_cube").value();
  glBindFramebuffer(GL_FRAMEBUFFER, capture_fbo);
  shader.Bind();
  shader.SetMat4("u_projection", capture_proj_matrix);
  texture.Bind(0);
  glViewport(0, 0, dims.x, dims.y);
  cube_pos_only_vao.Bind();
  for (int i = 0; i < 6; i++) {
    shader.SetMat4("u_view", capture_view_matrices[i]);
    glNamedFramebufferTexture2DEXT(capture_fbo, GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, env_cube_map.Id(), 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 36);
  }

  glNamedRenderbufferStorage(capture_rbo, GL_DEPTH_COMPONENT24, irradiance_dims.x,
                             irradiance_dims.y);
  gl::Shader irradiance_shader =
      gl::ShaderManager::Get().GetShader("irradiance_convolution").value();
  irradiance_shader.Bind();
  env_cube_map.Bind(0);
  irradiance_shader.SetMat4("u_projection", capture_proj_matrix);
  glViewport(0, 0, irradiance_dims.x, irradiance_dims.y);
  for (int i = 0; i < 6; i++) {
    irradiance_shader.SetMat4("u_view", capture_view_matrices[i]);
    glNamedFramebufferTexture2DEXT(capture_fbo, GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, irradiance_map.Id(), 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 36);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void CubeMapConverter::Draw(const gl::Texture& tex) const {
  glDepthFunc(GL_LEQUAL);
  gl::Shader shader = gl::ShaderManager::Get().GetShader("env_map").value();
  shader.Bind();
  tex.Bind(0);
  cube_pos_only_vao.Bind();
  glDrawArrays(GL_TRIANGLES, 0, 36);
  glDepthFunc(GL_LESS);
}

void CubeMapConverter::DrawIrradiance() const { Draw(irradiance_map); }

void CubeMapConverter::Draw() const { Draw(env_cube_map); }
