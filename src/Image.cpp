#include "Image.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image/stb_image_write.h"

Image::Image(const std::string& path, int req_components, bool flip) {
  LoadFromPath(path, req_components, flip);
}
Image::Image(const std::span<uint8_t>& bytes, int req_components) {
  LoadFromMemory(bytes, req_components);
}

// Image::Image(Image&& other) noexcept
//     : data(std::exchange(other.data, nullptr)),
//       width(std::exchange(other.width, 0)),
//       height(std::exchange(other.height, 0)),
//       channels(std::exchange(other.channels, 0)) {}
//
// Image& Image::operator=(Image&& other) noexcept {
//   data = std::exchange(other.data, nullptr);
//   width = std::exchange(other.width, 0);
//   height = std::exchange(other.height, 0);
//   channels = std::exchange(other.channels, 0);
//   return *this;
// }

void Image::LoadFromPathFloat(const std::string& path, int req_components, bool flip) {
  if (flip) stbi_set_flip_vertically_on_load_thread(flip);
  data = stbi_loadf(path.data(), &width, &height, &channels, req_components);
}

void Image::LoadFromPath(const std::string& path, int req_components, bool flip) {
  if (flip) stbi_set_flip_vertically_on_load_thread(flip);
  data = stbi_load(path.data(), &width, &height, &channels, req_components);
}

void Image::LoadFromMemory(const std::span<uint8_t>& bytes, int req_components) {
  data = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(bytes.data()),
                               static_cast<int>(bytes.size_bytes()), &width, &height, &channels,
                               req_components);
}

void Image::LoadFromMemory(unsigned char* bytes, size_t size_bytes, int req_components) {
  data =
      stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(bytes), static_cast<int>(size_bytes),
                            &width, &height, &channels, req_components);
}

Image::Image(unsigned char* bytes, size_t size_bytes, int req_components) {
  data =
      stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(bytes), static_cast<int>(size_bytes),
                            &width, &height, &channels, req_components);
}

void Image::Free() {
  if (data) stbi_image_free(data);
  data = nullptr;
  width = 0;
  height = 0;
  channels = 0;
}
