#include "ResourceManager.hpp"

#include "Renderer.hpp"

void ResourceManager::FreeModel(Model& model) {
  for (auto& mat : model.material_handles) {
    renderer_.FreeMaterial(mat);
  }
  for (auto& tex : model.texture_handles) {
    Free<gl::Texture>(tex);
  }
  for (auto& mesh : model.meshes) {
    for (auto& primitive : mesh.primitives) {
      renderer_.FreeMesh(primitive.mesh_handle);
    }
  }
}

void ResourceManager::Shutdown() {
  for (auto& [handle, model] : model_map_) {
    FreeModel(model);
  }
  model_map_.clear();
  texture_map_.clear();
}
