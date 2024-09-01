#include "Texture.hpp"

#include "pch.hpp"

namespace gl {

Texture::~Texture() {
  if (id_) {
    glDeleteTextures(1, &id_);
  }
}

void Texture::Load(const Tex2DCreateInfoEmpty& params) {
  dims_ = params.dims;
  glCreateTextures(GL_TEXTURE_2D, 1, &id_);
  glTextureStorage2D(id_, 1, params.internal_format, dims_.x, dims_.y);
  glTextureParameteri(id_, GL_TEXTURE_WRAP_S, params.wrap_s);
  glTextureParameteri(id_, GL_TEXTURE_WRAP_T, params.wrap_t);
  glTextureParameteri(id_, GL_TEXTURE_MIN_FILTER, params.min_filter);
  glTextureParameteri(id_, GL_TEXTURE_MAG_FILTER, params.mag_filter);
}

Texture::Texture(const Tex2DCreateInfoEmpty& params) { Load(params); }

Texture::Texture(Texture&& other) noexcept { *this = std::move(other); }

Texture& Texture::operator=(Texture&& other) noexcept {
  this->id_ = std::exchange(other.id_, 0);
  this->dims_ = other.dims_;
  return *this;
}

void Texture::Bind(int unit) const { glBindTextureUnit(unit, id_); }

}  // namespace gl
