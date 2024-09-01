#pragma once

#include <filesystem>

class Renderer;

namespace fastgltf {
class Mesh;
}

extern bool LoadMesh(fastgltf::Mesh& mesh);

extern void LoadModel(Renderer& renderer, const std::filesystem::path& path);
