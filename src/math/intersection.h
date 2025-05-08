#pragma once
#include "aabb.h"
#include <glm/glm.hpp>

struct Ray {
    glm::vec3 origin;
    glm::vec3 direction;
};

namespace math {
bool intersect_ray_aabb(const Ray &ray, const math::AABB &aabb,
                        float &outIntersectionDistance);
bool intersect_ray_aabb(const Ray &ray, const math::AABB &aabb);
glm::vec3 intersect_ray_plane(const Ray &ray, const glm::vec3 &planeOrigin,
                              const glm::vec3 &planeNormal);
} // namespace math
