#include "Shader.hpp"

#include <glm/gtc/type_ptr.hpp>

namespace gl {

void Shader::Bind() const {
  EASSERT_MSG(id_ != 0, "Shader is invalid");
  glUseProgram(id_);
}

void Shader::Unbind() { glUseProgram(0); }

void Shader::SetInt(const std::string& name, int value) {
  auto it = uniform_locations_.find(name);
  if (it != uniform_locations_.end()) {
    glUniform1i(it->second, value);
  } else {
    spdlog::error("uniform not found {}", name);
  }
}

void Shader::SetFloat(const std::string& name, float value) {
  auto it = uniform_locations_.find(name);
  if (it != uniform_locations_.end()) {
    glUniform1f(it->second, value);
  } else {
    spdlog::error("uniform not found {}", name);
  }
}

void Shader::SetMat4(const std::string& name, const glm::mat4& mat) {
  auto it = uniform_locations_.find(name);
  if (it != uniform_locations_.end()) {
    glUniformMatrix4fv(it->second, 1, GL_FALSE, glm::value_ptr(mat));
  } else {
    spdlog::error("uniform not found {}", name);
  }
}

void Shader::SetIVec2(const std::string& name, const glm::ivec2& vec) {
  auto it = uniform_locations_.find(name);
  if (it != uniform_locations_.end()) {
    glUniform2i(it->second, vec[0], vec[1]);
  } else {
    spdlog::error("uniform not found {}", name);
  }
}

void Shader::SetIVec3(const std::string& name, const glm::ivec3& vec) {
  auto it = uniform_locations_.find(name);
  if (it != uniform_locations_.end()) {
    glUniform3iv(it->second, 1, glm::value_ptr(vec));
  } else {
    spdlog::error("uniform not found {}", name);
  }
}

void Shader::SetVec3(const std::string& name, const glm::vec3& vec) {
  auto it = uniform_locations_.find(name);
  if (it != uniform_locations_.end()) {
    glUniform3fv(it->second, 1, glm::value_ptr(vec));
  } else {
    spdlog::error("uniform not found {}", name);
  }
}

void Shader::SetVec2(const std::string& name, const glm::vec2& vec) {
  auto it = uniform_locations_.find(name);
  if (it != uniform_locations_.end()) {
    glUniform2fv(it->second, 1, glm::value_ptr(vec));
  } else {
    spdlog::error("uniform not found {}", name);
  }
}

void Shader::SetVec4(const std::string& name, const Float4Arr& vec) {
  auto it = uniform_locations_.find(name);
  if (it != uniform_locations_.end()) {
    glUniform4fv(it->second, 1, vec);
  } else {
    spdlog::error("uniform not found {}", name);
  }
}

void Shader::SetVec4(const std::string& name, const glm::vec4& vec) {
  auto it = uniform_locations_.find(name);
  if (it != uniform_locations_.end()) {
    glUniform4fv(it->second, 1, glm::value_ptr(vec));
  } else {
    spdlog::error("uniform not found {}", name);
  }
}

void Shader::SetMat3(const std::string& name, const glm::mat3& mat, bool transpose) {
  auto it = uniform_locations_.find(name);
  if (it != uniform_locations_.end()) {
    glUniformMatrix3fv(it->second, 1, static_cast<GLboolean>(transpose), glm::value_ptr(mat));
  } else {
    spdlog::error("uniform not found {}", name);
  }
}

void Shader::SetFloatArr(const std::string& name, GLuint count, const GLfloat* value) {
  auto it = uniform_locations_.find(name);
  if (it != uniform_locations_.end()) {
    glUniform1fv(it->second, count, value);
  } else {
    spdlog::error("uniform not found {}", name);
  }
}

void Shader::SetBool(const std::string& name, bool value) {
  auto it = uniform_locations_.find(name);
  if (it != uniform_locations_.end()) {
    glUniform1i(it->second, static_cast<GLint>(value));
  } else {
    spdlog::error("uniform not found {}", name);
  }
}

Shader::Shader(uint32_t id, std::unordered_map<std::string, uint32_t>& uniform_locations)
    : id_(id), uniform_locations_(uniform_locations) {}

}  // namespace gl
