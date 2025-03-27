#pragma once
#include "types.h"
#include <vector>

class PipelineBuilder {
  public:
    std::vector<VkPipelineShaderStageCreateInfo> _shaderStages;

    VkPipelineInputAssemblyStateCreateInfo _inputAssembly;
    VkPipelineRasterizationStateCreateInfo _rasterizer;
    VkPipelineColorBlendAttachmentState _colorBlendAttachment;
    VkPipelineMultisampleStateCreateInfo _multisampling;
    VkPipelineLayout _pipelineLayout;
    VkPipelineDepthStencilStateCreateInfo _depthStencil;
    VkPipelineRenderingCreateInfo _renderInfo;
    VkFormat _colorAttachmentformat;
    VkPipelineVertexInputStateCreateInfo _vertexInput;

    PipelineBuilder() { clear(); }

    void clear();

    VkPipeline build_pipeline(VkDevice device);
    //< pipeline
    void set_shaders(VkShaderModule vertexShader,
                     VkShaderModule fragmentShader);
    void set_input_topology(VkPrimitiveTopology topology);
    void set_polygon_mode(VkPolygonMode mode);
    void set_cull_mode(VkCullModeFlags cullMode, VkFrontFace frontFace);
    void set_multisampling_none();
    void disable_blending();
    void disable_color_attachment_write();
    void enable_blending_additive();
    void enable_blending_alphablend();

    void set_color_attachment_format(VkFormat format);
    void set_depth_format(VkFormat format);
    void disable_depthtest();
    void enable_depthtest(bool depthWriteEnable, VkCompareOp op);
    void enable_depth_bias(float constantFactor, float slopeFactor,
                           float clamp);
    void
    set_vertex_input(VkVertexInputBindingDescription *bindingDescriptions,
                     uint32_t bindingDescriptionsCount,
                     VkVertexInputAttributeDescription *attributeDescriptions,
                     uint32_t attributeDescriptionsCount);

  private:
    bool _hasColorAttachment;
};

namespace vkutil {
bool load_shader_module(const char *filePath, VkDevice device,
                        VkShaderModule *outShaderModule);
}
