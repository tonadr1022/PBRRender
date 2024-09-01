#pragma once

#include "gl/Texture.hpp"

using AssetHandle = uint32_t;

class ResourceManager {
 public:
  template <typename T>
  [[nodiscard]] AssetHandle LoadTexture(const std::string& name, T&& params) {
    std::hash<std::string> hash;
    AssetHandle handle = hash(name);
    auto it = texture_map_.find(handle);
    // TODO: remove
    if (it != texture_map_.end()) {
      spdlog::info("reloading texture {}", name);
    }
    texture_map_.try_emplace(handle, std::forward<T>(params));
    return handle;
  }

  gl::Texture* GetTexture(AssetHandle handle);
  void FreeTexture(AssetHandle handle);
  uint32_t NumTextures() const { return texture_map_.size(); }

 private:
  std::unordered_map<AssetHandle, gl::Texture> texture_map_;
};
