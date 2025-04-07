#pragma once
#include <glm/glm.hpp>

namespace math {
bool intersect_ray_aabb(const glm::vec3 &rayOrigin,
                        const glm::vec3 &rayDirection, const glm::vec3 &boxMin,
                        const glm::vec3 &boxMax,
                        float &outIntersectionDistance);
bool intersect_ray_aabb(const glm::vec3 &rayOrigin,
                        const glm::vec3 &rayDirection, const glm::vec3 &boxMin,
                        const glm::vec3 &boxMax);
} // namespace math
