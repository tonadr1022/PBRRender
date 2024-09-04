#pragma once

struct AABB {
  glm::vec3 min, max;
  inline AABB& operator|=(const AABB& other) {
    min = glm::min(min, other.min);
    max = glm::max(max, other.max);
    return *this;
  }

  inline AABB operator|(const AABB& other) {
    AABB result = *this;
    result |= other;
    return result;
  }
};
