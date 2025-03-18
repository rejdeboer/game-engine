#include "tile.h"
#include "pipeline.h"
#include "renderer.hpp"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

void TileRenderer::init(Renderer *renderer) {
    _renderer = renderer;
    init_buffers();
    init_pipeline();
}

void TileRenderer::deinit() {
    _renderer->destroy_buffer(_vertexBuffer);
    _renderer->destroy_buffer(_indexBuffer);
}

void TileRenderer::render(VkCommandBuffer cmd) {
    // VkRenderingAttachmentInfo colorAttachment = {};
    // colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    // colorAttachment.imageView = _renderer->_drawImage.imageView;
    // colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    // colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    // colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    //
    // VkRenderingAttachmentInfo depthAttachment = {};
    // depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    // depthAttachment.imageView = _renderer->_depthImage.imageView;
    // depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    // depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    // depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    // // TODO: Vkguide uses 0.f as the far value here, do we need that?
    // depthAttachment.clearValue.depthStencil.depth = 1.f;
    //
    // VkRenderingInfo renderInfo = {};
    // renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    // renderInfo.colorAttachmentCount = 1;
    // renderInfo.pColorAttachments = &colorAttachment;
    // renderInfo.pDepthAttachment = &depthAttachment;
    // renderInfo.renderArea = VkRect2D{VkOffset2D{0, 0}, _drawExtent};
    // renderInfo.layerCount = 1;
    //
    // vkCmdBeginRendering(cmd, &renderInfo);
    //
    // VkViewport viewport = {};
    // viewport.x = 0;
    // viewport.y = 0;
    // viewport.width = (float)_swapchainExtent.width;
    // viewport.height = (float)_swapchainExtent.height;
    // viewport.minDepth = 0.f;
    // viewport.maxDepth = 1.f;
    // vkCmdSetViewport(cmd, 0, 1, &viewport);
    //
    // VkRect2D scissor = {};
    // scissor.offset.x = 0;
    // scissor.offset.y = 0;
    // scissor.extent.width = _swapchainExtent.width;
    // scissor.extent.height = _swapchainExtent.height;
    // vkCmdSetScissor(cmd, 0, 1, &scissor);
    //
    // AllocatedBuffer gpuSceneDataBuffer =
    //     create_buffer(sizeof(GPUSceneData),
    //     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
    //                   VMA_MEMORY_USAGE_CPU_TO_GPU);
    //
    // get_current_frame()._deletionQueue.push_function(
    //     [=, this]() { destroy_buffer(gpuSceneDataBuffer); });
    //
    // GPUSceneData *sceneUniformData =
    //     (GPUSceneData *)gpuSceneDataBuffer.allocation->GetMappedData();
    // *sceneUniformData = sceneData;
    //
    // VkDescriptorSet globalDescriptor =
    //     get_current_frame()._frameDescriptors.allocate(
    //         _device, _gpuSceneDataDescriptorLayout);
    // DescriptorWriter writer;
    // writer.write_buffer(0, gpuSceneDataBuffer.buffer, sizeof(GPUSceneData),
    // 0,
    //                     VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    // writer.update_set(_device, globalDescriptor);
    //
    // vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
    //                   _tilePipeline.pipeline);
    //
    // VkDeviceSize offsets[] = {0};
    // vkCmdBindVertexBuffers(cmd, 0, 1, &_tileVertices.buffer, offsets);
    // vkCmdBindIndexBuffer(cmd, _tileIndices.buffer, 0, VK_INDEX_TYPE_UINT32);
    //
    // vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
    //                         _tilePipeline.layout, 0, 1, &globalDescriptor, 0,
    //                         nullptr);
    //
    // for (auto chunk : _tileRenderChunks) {
    //     vkCmdBindVertexBuffers(cmd, 1, 1, &chunk.instanceBuffer.buffer,
    //                            offsets);
    //
    //     glm::mat4 chunkModel = glm::translate(glm::mat4(1.0f),
    //     chunk.position);
    //
    //     vkCmdPushConstants(cmd, _tilePipeline.layout,
    //                        VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4),
    //                        &chunkModel);
    //
    //     vkCmdDrawIndexed(cmd, kTileIndices.size(), chunk.instanceCount, 0, 0,
    //                      0);
    // }
    //
    // vkCmdEndRendering(cmd);
}

void TileRenderer::init_pipeline() {
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

    VkDescriptorSetLayout layouts[] = {
        _renderer->_gpuSceneDataDescriptorLayout};
    VkPipelineLayoutCreateInfo tileLayoutInfo = {};
    tileLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    tileLayoutInfo.setLayoutCount = 1;
    tileLayoutInfo.pSetLayouts = layouts;
    tileLayoutInfo.pushConstantRangeCount = 1;
    tileLayoutInfo.pPushConstantRanges = &pushConstantRange;

    VkPipelineLayout newLayout;
    VK_CHECK(vkCreatePipelineLayout(_renderer->_device, &tileLayoutInfo,
                                    nullptr, &newLayout));
    _pipeline.layout = newLayout;

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
    pipelineBuilder.set_vertex_input(
        &bindingDescriptions[0], bindingDescriptions.size(),
        &attributeDescriptions[0], attributeDescriptions.size());
    pipelineBuilder._pipelineLayout = newLayout;
    _pipeline.pipeline = pipelineBuilder.build_pipeline(_renderer->_device);

    vkDestroyShaderModule(_renderer->_device, tileVertShader, nullptr);
    vkDestroyShaderModule(_renderer->_device, tileFragShader, nullptr);
}

void TileRenderer::init_buffers() {
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

    void *data = staging.allocation->GetMappedData();

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

void TileRenderer::update_chunks(std::vector<TileRenderingInput> inputs) {
    for (auto chunk : _renderChunks) {
        _renderer->destroy_buffer(chunk.instanceBuffer);
    }

    _renderChunks.clear();
    _renderChunks.resize(inputs.size());

    for (int i = 0; i < inputs.size(); i++) {
        TileRenderChunk chunk;
        chunk.position = inputs[i].chunkPosition;
        chunk.instanceCount = inputs[i].instances.size();

        size_t bufferSize = sizeof(TileInstance) * chunk.instanceCount;
        // TODO: This should probably be a GPU buffer
        chunk.instanceBuffer = _renderer->create_buffer(
            bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VMA_MEMORY_USAGE_CPU_TO_GPU);

        void *data = chunk.instanceBuffer.allocation->GetMappedData();
        memcpy(data, inputs[i].instances.data(), bufferSize);

        _renderChunks[i] = chunk;
    }
}
