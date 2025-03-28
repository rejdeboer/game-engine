#pragma once
#include "types.h"

namespace vkutil {
bool is_visible(glm::mat4 transform, Bounds bounds, const glm::mat4 &viewproj);
};
