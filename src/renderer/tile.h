#pragma once
#include "types.h"
#include <array>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

struct TileInstance {
    /// Position within chunk
    glm::vec3 pos;
    glm::vec3 color;
};

struct TileVertex {
    /// Position within quad
    glm::vec3 pos;
    glm::vec3 normal;

    static std::array<VkVertexInputBindingDescription, 2>
    get_binding_descriptions() {
        std::array<VkVertexInputBindingDescription, 2> bindingDescriptions;
        bindingDescriptions[0].binding = 0;
        bindingDescriptions[0].stride = sizeof(TileVertex);
        bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        bindingDescriptions[1].binding = 1;
        bindingDescriptions[1].stride = sizeof(TileInstance);
        bindingDescriptions[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
        return bindingDescriptions;
    }

    static std::array<VkVertexInputAttributeDescription, 4>
    get_attribute_descriptions() {
        std::array<VkVertexInputAttributeDescription, 4>
            attributeDescriptions{};
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(TileVertex, pos);
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(TileVertex, normal);
        attributeDescriptions[2].binding = 1;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(TileInstance, pos);
        attributeDescriptions[3].binding = 1;
        attributeDescriptions[3].location = 3;
        attributeDescriptions[3].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[3].offset = offsetof(TileInstance, color);
        return attributeDescriptions;
    }
};

constexpr std::array<TileVertex, 4> kTileVertices = {{
    {{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
    {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
    {{1.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
    {{0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
}};

constexpr std::array<uint32_t, 6> kTileIndices = {0, 1, 2, 0, 2, 3};

struct TileDrawCommand {
    AllocatedBuffer instanceBuffer;
    uint32_t instanceCount;
    glm::mat4 transform;
    Bounds bounds;
};

struct TileRenderChunk {
    AllocatedBuffer instanceBuffer;
    uint32_t instanceCount;
    glm::vec3 position;
};

struct TileRenderingInput {
    std::vector<TileInstance> instances;
    glm::vec3 chunkPosition;
};

class Renderer;

class TileRenderer {
  public:
    void init(Renderer *renderer);
    void deinit();
    void draw(RenderContext ctx,
              const std::vector<TileDrawCommand> &drawCommands);

  private:
    Renderer *_renderer;
    MaterialPipeline _pipeline;
    AllocatedBuffer _vertexBuffer;
    AllocatedBuffer _indexBuffer;
    std::vector<TileRenderChunk> drawCommands;

    void init_pipeline();
    void init_buffers();
};
