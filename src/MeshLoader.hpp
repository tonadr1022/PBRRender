#pragma once

#include <filesystem>

class Renderer;
class ResourceManager;
struct Model;

namespace fastgltf {
class Mesh;
}

namespace loader {

[[nodiscard]] extern Model LoadModel(ResourceManager& resource_manager, Renderer& renderer,
                                     const std::filesystem::path& path);
}  // namespace loader
