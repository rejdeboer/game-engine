#pragma once

#include <glm/vec3.hpp>

namespace math {
inline constexpr glm::vec3 GLOBAL_UP_AXIS{0.f, 1.f, 0.f};
inline constexpr glm::vec3 GLOBAL_DOWN_AXIS{0.f, -1.f, 0.f};

inline constexpr glm::vec3 GLOBAL_FRONT_AXIS{0.f, 0.f, 1.f};
inline constexpr glm::vec3 GLOBAL_BACK_AXIS{0.f, 0.f, -1.f};

inline constexpr glm::vec3 GLOBAL_LEFT_AXIS{1.f, 0.f, 0.f};
inline constexpr glm::vec3 GLOBAL_RIGHT_AXIS{-1.f, 0.f, 0.f};
} // namespace math
