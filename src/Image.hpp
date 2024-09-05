#pragma once

#include <span>

struct Image {
  explicit Image(const std::string& path, int req_components, bool flip = true);
  explicit Image(const std::span<uint8_t>& bytes, int req_components);
  explicit Image(unsigned char* bytes, size_t size_bytes, int req_components);
  Image(void* data, int width, int height, int channels)
      : data(data), width(width), height(height), channels(channels) {}
  Image() = default;
  // Image(const Image& other) = delete;
  // Image& operator=(const Image& other) = delete;
  // Image(Image&& other) noexcept;
  // Image& operator=(Image&& other) noexcept;

  void Free();
  void* data{};
  int width{}, height{}, channels{};
  void LoadFromPath(const std::string& path, int req_components, bool flip = true);
  void LoadFromPathFloat(const std::string& path, int req_components, bool flip = true);
  void LoadFromMemory(const std::span<uint8_t>& bytes, int req_components);
  void LoadFromMemory(unsigned char* bytes, size_t size_bytes, int req_components);

 private:
};
