#include "tile.h"
#include "../frustum_culling.h"
#include "../renderer.hpp"
#include "builder.h"
#include "vk_mem_alloc.h"
#include <glm/ext/matrix_transform.hpp>

void TilePipeline::init(Renderer *renderer, VkDescriptorSetLayout sceneLayout,
                        VkDescriptorSetLayout shadowMapLayout) {
    _renderer = renderer;
    init_buffers();
    init_pipeline(sceneLayout, shadowMapLayout);
}

void TilePipeline::deinit() {
    _renderer->destroy_buffer(_vertexBuffer);
    _renderer->destroy_buffer(_indexBuffer);
}

void TilePipeline::draw(const RenderContext &ctx,
                        const std::vector<TileDrawCommand> &drawCommands) {
    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = (float)ctx.drawExtent.width;
    viewport.height = (float)ctx.drawExtent.height;
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;
    vkCmdSetViewport(ctx.cmd, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = ctx.drawExtent.width;
    scissor.extent.height = ctx.drawExtent.height;
    vkCmdSetScissor(ctx.cmd, 0, 1, &scissor);

    vkCmdBindPipeline(ctx.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline);

    VkDescriptorSet descriptorSets[] = {ctx.globalDescriptorSet,
                                        ctx.shadowMapSet};
    vkCmdBindDescriptorSets(ctx.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            _pipelineLayout, 0, 2, &descriptorSets[0], 0,
                            nullptr);

    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(ctx.cmd, 0, 1, &_vertexBuffer.buffer, offsets);
    vkCmdBindIndexBuffer(ctx.cmd, _indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

    for (auto &drawCommand : drawCommands) {
        if (!vkutil::is_visible(drawCommand.transform, drawCommand.bounds,
                                ctx.viewproj)) {
            continue;
        }

        vkCmdBindVertexBuffers(ctx.cmd, 1, 1,
                               &drawCommand.instanceBuffer.buffer, offsets);

        glm::mat4 chunkModel = drawCommand.transform;

        vkCmdPushConstants(ctx.cmd, _pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
                           0, sizeof(glm::mat4), &chunkModel);

        vkCmdDrawIndexed(ctx.cmd, kTileIndices.size(),
                         drawCommand.instanceCount, 0, 0, 0);
    }
}

void TilePipeline::init_pipeline(VkDescriptorSetLayout sceneLayout,
                                 VkDescriptorSetLayout shadowMapLayout) {
    VkShaderModule tileFragShader;
    if (!vkutil::load_shader_module("shaders/spv/tile.frag.spv",
                                    _renderer->_device, &tileFragShader)) {
        fmt::println("Error when building the tile fragment shader module");
    }

    VkShaderModule tileVertShader;
    if (!vkutil::load_shader_module("shaders/spv/tile.vert.spv",
                                    _renderer->_device, &tileVertShader)) {
        fmt::println("Error when building the tile vertex shader module");
    }

    // TODO: If the instance buffer is not updated often, using descriptor sets
    // would be better
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(glm::mat4);
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayout layouts[] = {sceneLayout, shadowMapLayout};
    VkPipelineLayoutCreateInfo tileLayoutInfo = {};
    tileLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    tileLayoutInfo.setLayoutCount = 2;
    tileLayoutInfo.pSetLayouts = layouts;
    tileLayoutInfo.pushConstantRangeCount = 1;
    tileLayoutInfo.pPushConstantRanges = &pushConstantRange;

    VkPipelineLayout newLayout;
    VK_CHECK(vkCreatePipelineLayout(_renderer->_device, &tileLayoutInfo,
                                    nullptr, &newLayout));
    _pipelineLayout = newLayout;

    auto bindingDescriptions = TileVertex::get_binding_descriptions();
    auto attributeDescriptions = TileVertex::get_attribute_descriptions();

    PipelineBuilder pipelineBuilder;
    pipelineBuilder.set_shaders(tileVertShader, tileFragShader);
    pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
    pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    pipelineBuilder.set_multisampling_none();
    pipelineBuilder.disable_blending();
    pipelineBuilder.enable_depthtest(true, VK_COMPARE_OP_LESS_OR_EQUAL);
    pipelineBuilder.set_color_attachment_format(_renderer->_drawImage.format);
    pipelineBuilder.set_depth_format(_renderer->_depthImage.format);
    pipelineBuilder.set_stencil_format(_renderer->_depthImage.format);
    pipelineBuilder.set_vertex_input(
        &bindingDescriptions[0], bindingDescriptions.size(),
        &attributeDescriptions[0], attributeDescriptions.size());
    pipelineBuilder._pipelineLayout = newLayout;
    _pipeline = pipelineBuilder.build_pipeline(_renderer->_device);

    vkDestroyShaderModule(_renderer->_device, tileVertShader, nullptr);
    vkDestroyShaderModule(_renderer->_device, tileFragShader, nullptr);
}

void TilePipeline::init_buffers() {
    const size_t vertexBufferSize = kTileVertices.size() * sizeof(TileVertex);
    const size_t indexBufferSize = kTileIndices.size() * sizeof(uint32_t);

    _vertexBuffer = _renderer->create_buffer(
        vertexBufferSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);

    _indexBuffer = _renderer->create_buffer(
        indexBufferSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);

    AllocatedBuffer staging = _renderer->create_buffer(
        vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_CPU_ONLY);

    VmaAllocationInfo allocInfo;
    vmaGetAllocationInfo(_renderer->_allocator, staging.allocation, &allocInfo);
    void *data = allocInfo.pMappedData;

    memcpy(data, kTileVertices.data(), vertexBufferSize);
    memcpy((char *)data + vertexBufferSize, kTileIndices.data(),
           indexBufferSize);

    _renderer->immediate_submit([&](VkCommandBuffer cmd) {
        VkBufferCopy vertexCopy{0};
        vertexCopy.dstOffset = 0;
        vertexCopy.srcOffset = 0;
        vertexCopy.size = vertexBufferSize;

        vkCmdCopyBuffer(cmd, staging.buffer, _vertexBuffer.buffer, 1,
                        &vertexCopy);

        VkBufferCopy indexCopy{0};
        indexCopy.dstOffset = 0;
        indexCopy.srcOffset = vertexBufferSize;
        indexCopy.size = indexBufferSize;

        vkCmdCopyBuffer(cmd, staging.buffer, _indexBuffer.buffer, 1,
                        &indexCopy);
    });

    _renderer->destroy_buffer(staging);
}
