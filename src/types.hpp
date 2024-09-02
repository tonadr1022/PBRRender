#pragma once

using AssetHandle = uint32_t;

enum class PrimitiveType : std::uint8_t {
  kPoints = 0,
  kLines = 1,
  kLineLoop = 2,
  kLineStrip = 3,
  kTriangles = 4,
  kTriangleStrip = 5,
  kTriangleFan = 6,
};
struct Vertex {
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec3 tangent;
  glm::vec2 uv;
};

struct alignas(16) Material {
  glm::vec4 base_color{1};
  uint64_t base_color_bindless_handle{};
  uint64_t metallic_roughness_bindless_handle{};
  uint64_t normal_bindless_handle{};
  float alpha_cutoff{};
};

enum class AlphaMode {
  kOpaque,
  kBlend,
};

struct Primitive {
  AssetHandle mesh_handle{};
  AssetHandle material_handle{};
};

struct Model {
  std::vector<AssetHandle> texture_handles;
  std::vector<AssetHandle> material_handles;
  std::vector<Primitive> primitives;
};

struct LightsInfo {
  glm::vec3 directional_dir;
  glm::vec3 directional_color;
};
