#pragma once
#include <glm/glm.hpp>

struct Ray {
    glm::vec3 origin;
    glm::vec3 direction;
};

namespace math {
bool intersect_ray_aabb(const Ray &ray, const glm::vec3 &boxMin,
                        const glm::vec3 &boxMax,
                        float &outIntersectionDistance);
bool intersect_ray_aabb(const Ray &ray, const glm::vec3 &boxMin,
                        const glm::vec3 &boxMax);
} // namespace math
