#pragma once

namespace gl {

struct Tex2DCreateInfoEmpty {
  glm::ivec2 dims;
  GLuint wrap_s;
  GLuint wrap_t;
  GLuint internal_format;
  GLuint min_filter{GL_LINEAR};
  GLuint mag_filter{GL_LINEAR};
};

class Texture {
 public:
  Texture() = default;
  void Load(const Tex2DCreateInfoEmpty& params);
  explicit Texture(const Tex2DCreateInfoEmpty& params);
  Texture(const Texture& other) = delete;
  Texture operator=(const Texture& other) = delete;
  Texture(Texture&& other) noexcept;
  Texture& operator=(Texture&& other) noexcept;
  ~Texture();
  [[nodiscard]] uint32_t Id() const { return id_; }
  [[nodiscard]] glm::ivec2 Dims() const { return dims_; }

  void Bind() const;
  void Bind(int unit) const;

 private:
  uint32_t id_{0};
  glm::ivec2 dims_{};
};

}  // namespace gl
