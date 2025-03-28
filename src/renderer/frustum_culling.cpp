#include "frustum_culling.h"
#include <array>

bool is_visible(glm::mat4 transform, Bounds bounds, const glm::mat4 &viewproj) {
    std::array<glm::vec3, 8> corners{
        glm::vec3{1, 1, 1},   glm::vec3{1, 1, -1},   glm::vec3{1, -1, 1},
        glm::vec3{1, -1, -1}, glm::vec3{-1, 1, 1},   glm::vec3{-1, 1, -1},
        glm::vec3{-1, -1, 1}, glm::vec3{-1, -1, -1},
    };

    glm::mat4 matrix = viewproj * transform;

    glm::vec3 min = {1.5, 1.5, 1.5};
    glm::vec3 max = {-1.5, -1.5, -1.5};

    for (int c = 0; c < 8; c++) {
        // project each corner into clip space
        glm::vec4 v =
            matrix *
            glm::vec4(bounds.origin + (corners[c] * bounds.extents), 1.f);

        // perspective correction
        v.x = v.x / v.w;
        v.y = v.y / v.w;
        v.z = v.z / v.w;

        min = glm::min(glm::vec3{v.x, v.y, v.z}, min);
        max = glm::max(glm::vec3{v.x, v.y, v.z}, max);
    }

    // check the clip space box is within the view
    if (min.z > 1.f || max.z < 0.f || min.x > 1.f || max.x < -1.f ||
        min.y > 1.f || max.y < -1.f) {
        return false;
    }
    return true;
}
