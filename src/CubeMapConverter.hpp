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

  void RenderEquirectangularEnvMap(const gl::Texture& texture) const;
  void Draw() const;
  void DrawIrradiance() const;

  gl::VertexArray cube_pos_only_vao;
  gl::Buffer<VertexPosOnly> cube_pos_only_vbo;

  glm::ivec2 dims{512, 512};
  gl::Texture env_cube_map;
  gl::Texture irradiance_map;
  GLuint capture_fbo, capture_rbo;

  ~CubeMapConverter() {
    // TODO: RAII wrapper
    glDeleteFramebuffers(1, &capture_fbo);
    glDeleteRenderbuffers(1, &capture_rbo);
  }

 private:
  void Draw(const gl::Texture& tex) const;
};
