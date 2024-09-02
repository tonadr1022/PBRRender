#include "ResourceManager.hpp"

#include "Renderer.hpp"

void ResourceManager::FreeModel(Model& model) {
  for (auto& mat : model.material_handles) {
    renderer_.FreeMaterial(mat);
  }
  for (auto& tex : model.texture_handles) {
    Free<gl::Texture>(tex);
  }
  for (auto& p : model.primitives) {
    renderer_.FreeMesh(p.mesh_handle);
  }
}
