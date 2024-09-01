#pragma once

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
  glm::vec2 uv;
};
