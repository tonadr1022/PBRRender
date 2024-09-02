#pragma once

#include <glm/ext/quaternion_float.hpp>

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

// need to handle:
// metallic_roughness
// occlusion_roughness_metallic
enum MaterialFlags : uint32_t {
  kNone = 0,
  kMetallicRoughness = 1 << 0,
  kOcclusionRoughnessMetallic = 1 << 2,
  kAlphaMaskOn = 1 << 3,
};

struct alignas(16) Material {
  glm::vec4 base_color{1};
  glm::vec4 emissive_factor{1};
  glm::vec2 uv_scale{1};
  glm::vec2 uv_offset{0};
  uint64_t base_color_bindless_handle{};
  uint64_t metallic_roughness_bindless_handle{};
  uint64_t occlusion_handle{};
  uint64_t normal_bindless_handle{};
  uint64_t emissive_handle{};
  float metallic_factor{1};
  float roughness_factor{1};
  float emissive_strength{0};
  float alpha_cutoff{0.5};
  float uv_rotation{0};
  uint32_t material_flags{};
};

enum class AlphaMode {
  kOpaque,
  kBlend,
};

struct Primitive {
  AssetHandle mesh_handle{};
  AssetHandle material_handle{};
};

struct Mesh {
  std::vector<Primitive> primitives;
};

struct SceneNode {
  std::string name;
  glm::vec3 translation;
  glm::quat rotation;
  glm::vec3 scale;
  std::vector<size_t> child_indices;
  size_t idx;
  size_t mesh_idx;
};

struct CameraData {
  glm::mat4 proj_matrix;
  glm::mat4 view_matrix;
  glm::vec3 view_pos;
};

struct Model {
  std::vector<CameraData> camera_data;
  std::vector<AssetHandle> texture_handles;
  std::vector<AssetHandle> material_handles;
  std::vector<SceneNode> nodes;
  std::vector<Mesh> meshes;
};

struct LightsInfo {
  glm::vec3 directional_dir;
  glm::vec3 directional_color;
};
