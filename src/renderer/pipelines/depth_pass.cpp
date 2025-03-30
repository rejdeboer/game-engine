#include "depth_pass.h"
#include "../frustum_culling.h"
#include "builder.h"

void DepthPassPipeline::init(VkDevice device, VkFormat imageFormat,
                             VkDescriptorSetLayout sceneLayout) {
    VkShaderModule shadowFragShader;
    if (!vkutil::load_shader_module("shaders/spv/depth.frag.spv", device,
                                    &shadowFragShader)) {
        fmt::println("Error when building the depth fragment shader module");
    }

    VkShaderModule shadowVertShader;
    if (!vkutil::load_shader_module("shaders/spv/depth.vert.spv", device,
                                    &shadowVertShader)) {
        fmt::println("Error when building the depth vertex shader module");
    }

    VkPushConstantRange matrixRange{};
    matrixRange.offset = 0;
    matrixRange.size = sizeof(GPUDrawPushConstants);
    matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayout layouts[] = {sceneLayout};

    VkPipelineLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = layouts;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &matrixRange;

    VK_CHECK(
        vkCreatePipelineLayout(device, &layoutInfo, nullptr, &_pipelineLayout));

    PipelineBuilder pipelineBuilder;
    pipelineBuilder.set_shaders(shadowVertShader, shadowFragShader);
    pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
    pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    pipelineBuilder.set_multisampling_none();
    pipelineBuilder.disable_blending();
    pipelineBuilder.disable_color_attachment_write();
    pipelineBuilder.enable_depthtest(true, VK_COMPARE_OP_LESS_OR_EQUAL);
    pipelineBuilder.set_depth_format(imageFormat);
    pipelineBuilder.enable_depth_bias(4.0f, 1.5f, 0.0f);
    pipelineBuilder._pipelineLayout = _pipelineLayout;
    _pipeline = pipelineBuilder.build_pipeline(device);

    vkDestroyShaderModule(device, shadowVertShader, nullptr);
    vkDestroyShaderModule(device, shadowFragShader, nullptr);
}

void DepthPassPipeline::draw(VkCommandBuffer cmd,
                             VkDescriptorSet sceneDescriptor,
                             glm::mat4 lightViewproj, uint32_t resolution,
                             const std::vector<MeshDrawCommand> &drawCommands) {
    VkViewport shadowViewport = {};
    shadowViewport.x = 0.0f;
    shadowViewport.y = 0.0f;
    shadowViewport.width = resolution;
    shadowViewport.height = resolution;
    shadowViewport.minDepth = 0.0f;
    shadowViewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &shadowViewport);

    VkRect2D shadowScissor = {};
    shadowScissor.offset = {0, 0};
    shadowScissor.extent = {resolution, resolution};
    vkCmdSetScissor(cmd, 0, 1, &shadowScissor);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            _pipelineLayout, 0, 1, &sceneDescriptor, 0,
                            nullptr);

    VkBuffer lastIndexBuffer = VK_NULL_HANDLE;
    for (const auto &drawCmd : drawCommands) {
        if (!vkutil::is_visible(drawCmd.transform, drawCmd.bounds,
                                lightViewproj)) {
            continue;
        }

        if (drawCmd.indexBuffer != lastIndexBuffer) {
            lastIndexBuffer = drawCmd.indexBuffer;
            vkCmdBindIndexBuffer(cmd, drawCmd.indexBuffer, 0,
                                 VK_INDEX_TYPE_UINT32);
        }

        GPUDrawPushConstants pushConstants;
        pushConstants.vertexBuffer = drawCmd.vertexBufferAddress;
        pushConstants.worldMatrix = drawCmd.transform;
        vkCmdPushConstants(cmd, _pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                           sizeof(GPUDrawPushConstants), &pushConstants);

        vkCmdDrawIndexed(cmd, drawCmd.indexCount, 1, drawCmd.firstIndex, 0, 0);
    }
}
