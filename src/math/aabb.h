#pragma once
#include <glm/glm.hpp>

namespace math {
struct AABB {
    glm::vec3 min;
    glm::vec3 max;

    glm::vec3 get_size() const { return glm::abs(max - min); }

    void merge(const AABB &other) {
        min = glm::min(min, other.min);
        max = glm::max(max, other.max);
    }

    AABB transform(const glm::mat4 &matrix) const {
        glm::vec3 corners[8] = {
            glm::vec3(min.x, min.y, min.z), glm::vec3(max.x, min.y, min.z),
            glm::vec3(min.x, max.y, min.z), glm::vec3(min.x, min.y, max.z),
            glm::vec3(max.x, max.y, min.z), glm::vec3(min.x, max.y, max.z),
            glm::vec3(max.x, min.y, max.z), glm::vec3(max.x, max.y, max.z),
        };

        glm::vec4 transformedCorner4 = matrix * glm::vec4(corners[0], 1.0f);

        glm::vec3 firstTransformedCorner =
            (transformedCorner4.w == 0.0f) ? glm::vec3(transformedCorner4)
                                           : // Avoid division by zero
                glm::vec3(transformedCorner4) / transformedCorner4.w;

        AABB resultAABB;
        resultAABB.min = firstTransformedCorner;
        resultAABB.max = firstTransformedCorner;

        for (int i = 1; i < 8; ++i) {
            transformedCorner4 = matrix * glm::vec4(corners[i], 1.0f);
            glm::vec3 transformedCorner =
                (transformedCorner4.w == 0.0f)
                    ? glm::vec3(transformedCorner4)
                    : glm::vec3(transformedCorner4) / transformedCorner4.w;

            resultAABB.min = glm::min(resultAABB.min, transformedCorner);
            resultAABB.max = glm::max(resultAABB.max, transformedCorner);
        }

        return resultAABB;
    }
};
} // namespace math
