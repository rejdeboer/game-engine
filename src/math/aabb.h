#pragma once
#include <glm/common.hpp>
#include <glm/vec3.hpp>

namespace math {
struct AABB {
    glm::vec3 min;
    glm::vec3 max;

    glm::vec3 get_size() const { return glm::abs(max - min); }

    void merge(const AABB &other) {
        min = glm::min(min, other.min);
        max = glm::max(max, other.max);
    }
};
} // namespace math
