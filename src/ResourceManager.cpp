#include "ResourceManager.hpp"

gl::Texture* ResourceManager::GetTexture(AssetHandle handle) {
  auto it = texture_map_.find(handle);
  return it == texture_map_.end() ? nullptr : &it->second;
}

void ResourceManager::FreeTexture(AssetHandle handle) { texture_map_.erase(handle); }
