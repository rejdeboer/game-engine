#ifndef RENDERER_VERTEX_H

#include <glm/glm.hpp>
#include <vulkan/vulkan_core.h>

struct Vertex {
    glm::vec3 pos;
    float uv_x;
    glm::vec3 normal;
    float uv_y;
    glm::vec4 color;
};

#define RENDERER_VERTEX_H
#endif
