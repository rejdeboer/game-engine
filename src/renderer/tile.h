#pragma once
#include "types.h"
#include <array>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

struct TileVertex {
    glm::vec3 pos;
    glm::vec3 normal;

    static VkVertexInputBindingDescription get_binding_description() {
        VkVertexInputBindingDescription desc = {};
        desc.binding = 0;
        desc.stride = sizeof(TileVertex);
        desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return desc;
    }

    static std::array<VkVertexInputAttributeDescription, 2>
    get_attribute_descriptions() {
        std::array<VkVertexInputAttributeDescription, 2>
            attributeDescriptions{};
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(TileVertex, pos);
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(TileVertex, normal);
        return attributeDescriptions;
    }
};

struct TileInstance {
    glm::vec3 pos;
    glm::vec3 color;
};

constexpr std::array<TileVertex, 4> kTileVertices = {{
    {{-0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
    {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
    {{0.5f, 0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
    {{-0.5f, 0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
}};

const std::array<uint32_t, 6> kTileIndices = {0, 1, 2, 1, 2, 3};

struct RenderTileChunk {
    AllocatedBuffer instanceBuffer;
    uint32_t instanceCount;
};

struct TileRenderingInput {
    std::vector<TileInstance> instances;
    glm::vec3 chunkPosition;
};
