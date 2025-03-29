#pragma once
#include "../descriptor.h"
#include "../types.h"
#include <glm/glm.hpp>
#include <vector>
#include <vulkan/vulkan_core.h>

struct MeshVertex {
    glm::vec3 pos;
    float uv_x;
    glm::vec3 normal;
    float uv_y;
    glm::vec4 color;
};

struct MeshDrawCommand {
    uint32_t indexCount;
    uint32_t firstIndex;
    VkBuffer indexBuffer;

    MaterialInstance *material;
    Bounds bounds;

    glm::mat4 transform;
    VkDeviceAddress vertexBufferAddress;
};

class Renderer; // Forward declaration

class MeshPipeline {
  public:
    struct MaterialConstants {
        glm::vec4 colorFactors;
        glm::vec4 metalRoughFactors;
        // padding, we need it for uniform buffers
        glm::vec4 extra[14];
    };

    struct MaterialResources {
        AllocatedImage colorImage;
        VkSampler colorSampler;
        AllocatedImage metalRoughImage;
        VkSampler metalRoughSampler;
        VkBuffer dataBuffer;
        uint32_t dataBufferOffset;
    };

    void init(Renderer *renderer);
    void deinit();
    void draw(RenderContext &ctx,
              const std::vector<MeshDrawCommand> &drawCommands);

    MaterialInstance
    write_material(VkDevice device, MaterialPass pass,
                   const MaterialResources &resources,
                   DescriptorAllocatorGrowable &descriptorAllocator);

  private:
    Renderer *_renderer;
    MaterialPipeline _opaquePipeline;
    MaterialPipeline _transparentPipeline;
    DescriptorWriter _writer;

    VkDescriptorSetLayout _materialLayout;

    void init_pipeline();
    void init_buffers();
};
