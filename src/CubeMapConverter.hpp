#pragma once
#include "gl/Buffer.hpp"
#include "gl/Texture.hpp"
#include "gl/VertexArray.hpp"
#include "types.hpp"
namespace gl {
class Texture;
}

struct CubeMapConverter {
  void Init();

  void RenderEquirectangularEnvMap(const gl::Texture& texture);
  void DrawBRDFTexture();
  void Draw() const;
  void DrawIrradiance() const;
  void DrawPrefilter() const;

  gl::VertexArray cube_pos_only_vao;
  gl::Buffer<VertexPosOnly> cube_pos_only_vbo;

  // TODO: be specific and move to impl file
  glm::ivec2 dims{512, 512};
  gl::Texture env_cube_map;
  gl::Texture irradiance_map;
  gl::Texture prefilter_map;
  gl::Texture brdf_lookup_tex;

  ~CubeMapConverter() {
    // TODO: RAII wrapper
    glDeleteFramebuffers(1, &capture_fbo_);
    glDeleteRenderbuffers(1, &capture_rbo_);
  }

 private:
  void RenderBRDFLookupTexture();
  gl::VertexArray quad_vao_;
  gl::Buffer<PosTexVertex> quad_vbo_;
  gl::Buffer<uint32_t> quad_ebo_;
  GLuint capture_fbo_, capture_rbo_;
  void Draw(const gl::Texture& tex) const;
};
