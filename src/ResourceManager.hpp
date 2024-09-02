#pragma once

#include <concepts>

#include "MeshLoader.hpp"
#include "gl/Texture.hpp"
#include "types.hpp"

using AssetHandle = uint32_t;
class Renderer;

template <typename T>
concept SupportedResource = std::same_as<T, gl::Texture> || std::same_as<T, Model>;

class ResourceManager {
 public:
  explicit ResourceManager(Renderer& renderer) : renderer_(renderer){};

  template <SupportedResource T, typename ParamT>
  [[nodiscard]] AssetHandle Load(const std::string& path_or_name, ParamT&& params) {
    std::hash<std::string> hash;
    AssetHandle handle = hash(path_or_name);
    if constexpr (std::is_same_v<T, Model>) {
      auto it = model_map_.find(handle);
      if (it != model_map_.end()) {
        spdlog::info("reloading model {}", path_or_name);
      }
      model_map_.try_emplace(handle, loader::LoadModel(*this, renderer_, path_or_name, params));
    } else if constexpr (std::is_same_v<T, gl::Texture>) {
      auto it = texture_map_.find(handle);
      if (it != texture_map_.end()) {
        spdlog::info("reloading texture {}", path_or_name);
      }
      texture_map_.try_emplace(handle, std::forward<ParamT>(params));
    }
    return handle;
  }

  template <SupportedResource T>
  void Free(AssetHandle handle) {
    if (handle == 0) return;
    if constexpr (std::is_same_v<T, gl::Texture>) {
      texture_map_.erase(handle);
    } else if constexpr (std::is_same_v<T, Model>) {
      auto it = model_map_.find(handle);
      if (it != model_map_.end()) {
        FreeModel(it->second);
        model_map_.erase(handle);
      }
    }
  }

  template <SupportedResource T>
  T* Get(AssetHandle handle) {
    if constexpr (std::is_same_v<T, gl::Texture>) {
      auto it = texture_map_.find(handle);
      return it == texture_map_.end() ? nullptr : &it->second;
    } else if constexpr (std::is_same_v<T, Model>) {
      auto it = model_map_.find(handle);
      return it == model_map_.end() ? nullptr : &it->second;
    }
  }

  uint32_t NumTextures() const { return texture_map_.size(); }
  uint32_t NumModels() const { return model_map_.size(); }

 private:
  void FreeModel(Model& model);
  Renderer& renderer_;
  std::unordered_map<AssetHandle, gl::Texture> texture_map_;
  std::unordered_map<AssetHandle, Model> model_map_;
};
