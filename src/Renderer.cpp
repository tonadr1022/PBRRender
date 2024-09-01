#include "Renderer.hpp"

#include "gl/InternalTypes.hpp"
#include "gl/OpenGLDebug.hpp"
#include "types.hpp"

void Renderer::Init() {
  glEnable(GL_DEBUG_OUTPUT);
  glDebugMessageCallback(gl::MessageCallback, nullptr);

  pos_tex_vbo_.Init(100000, sizeof(Vertex));
  index_buffer_32_.Init(1000000, sizeof(uint32_t));
  index_buffer_16_.Init(1000000, sizeof(uint16_t));
}
