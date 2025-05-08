#pragma once
#include <glm/glm.hpp>
#include <vulkan/vulkan_core.h>

struct Vertex {
    glm::vec3 pos;
    float uv_x;
    glm::vec3 normal;
    float uv_y;
    glm::vec4 color;

    // Skinning data
    glm::ivec4 jointIndices = glm::ivec4(0);
    glm::vec4 jointWeights = glm::vec4(0.0f);
};
