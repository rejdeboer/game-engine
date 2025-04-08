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

    void init(VkDevice device, VkFormat drawImageFormat,
              VkFormat depthImageFormat,
              VkDescriptorSetLayout gpuSceneDataDescriptorLayout,
              VkDescriptorSetLayout shadowDescriptorSetLayout);
    void deinit();
    void draw(const RenderContext &ctx,
              const std::vector<MeshDrawCommand> &drawCommands);

    MaterialInstance
    write_material(VkDevice device, MaterialPass pass,
                   const MaterialResources &resources,
                   DescriptorAllocatorGrowable &descriptorAllocator);

  private:
    MaterialPipeline _opaquePipeline;
    MaterialPipeline _transparentPipeline;

    MaterialPipeline _stencilWritePipeline;
    // TODO: Should we use a separate pipeline layout for this
    MaterialPipeline _outlinePipeline;

    DescriptorWriter _writer;

    VkDescriptorSetLayout _materialLayout;
};
