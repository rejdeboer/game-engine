#pragma once
#include "../types.h"

class DepthPassPipeline {
  public:
    void init(VkDevice device, VkFormat imageFormat,
              VkDescriptorSetLayout sceneLayout);
    void deinit();
    void draw(VkCommandBuffer cmd, VkDescriptorSet sceneDescriptor,
              glm::mat4 lightViewproj, uint32_t resolution,
              const std::vector<MeshDrawCommand> &drawCommands);

  private:
    VkPipeline _pipeline;
    VkPipelineLayout _pipelineLayout;
};
