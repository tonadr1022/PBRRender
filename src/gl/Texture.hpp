#pragma once

namespace gl {

struct Tex2DCreateInfo {
  glm::ivec2 dims;
  GLuint wrap_s;
  GLuint wrap_t;
  GLuint internal_format;
  GLuint format;
  GLenum type;
  GLuint min_filter{GL_LINEAR};
  GLuint mag_filter{GL_LINEAR};
  unsigned char* data;
  bool bindless{true};
  bool gen_mipmaps{true};
};

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
  void Load(const Tex2DCreateInfo& params);
  explicit Texture(const Tex2DCreateInfoEmpty& params);
  explicit Texture(const Tex2DCreateInfo& params);
  Texture(const Texture& other) = delete;
  Texture operator=(const Texture& other) = delete;
  Texture(Texture&& other) noexcept;
  Texture& operator=(Texture&& other) noexcept;
  ~Texture();
  [[nodiscard]] uint32_t Id() const { return id_; }
  [[nodiscard]] uint64_t BindlessHandle() const { return bindless_handle_; }
  void MakeNonResident();
  void MakeResident();

  void Bind() const;
  void Bind(int unit) const;

 private:
  uint32_t id_{0};
  uint32_t bindless_handle_{0};
  bool resident_{false};
};

}  // namespace gl
