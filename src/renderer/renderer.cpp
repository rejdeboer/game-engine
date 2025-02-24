#include "renderer.hpp"
#include "SDL3/SDL_vulkan.h"
#include "image.h"
#include "pipeline.h"
#include <algorithm>
#include <glm/gtx/transform.hpp>
#include <vulkan/vulkan_core.h>

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

Renderer::Renderer(SDL_Window *window) {
    _window = window;
    _instance = create_vulkan_instance(window);
    _surface = create_surface(window, _instance);
    _physicalDevice = pick_physical_device(_instance, _surface);

    QueueFamilyIndices queueFamilyIndices =
        find_compatible_queue_family_indices(_physicalDevice, _surface);
    assert(queueFamilyIndices.is_complete());
    uint32_t graphicsIndex = queueFamilyIndices.graphics_family.value();
    uint32_t presentationIndex = queueFamilyIndices.present_family.value();

    _device = create_device(_physicalDevice, graphicsIndex, presentationIndex);
    _allocator = create_allocator(_instance, _physicalDevice, _device);

    init_swap_chain();
    _drawImage = create_draw_image(_device, _allocator, _swapChainExtent);
    _depthImage = create_depth_image(_device, _allocator, _swapChainExtent);

    _mainDeletionQueue.push_function([&]() {
        vkDestroyImageView(_device, _drawImage.imageView, nullptr);
        vmaDestroyImage(_allocator, _drawImage.image, _drawImage.allocation);
        vkDestroyImageView(_device, _depthImage.imageView, nullptr);
        vmaDestroyImage(_allocator, _depthImage.image, _depthImage.allocation);
    });

    init_pipelines();

    _graphicsQueue = get_device_queue(_device, graphicsIndex, 0);
    _presentationQueue = get_device_queue(_device, presentationIndex, 0);

    init_commands(graphicsIndex);
    init_sync_structures();
    init_descriptors();
    init_default_data();

    _mainDeletionQueue.push_function(
        [&]() { vmaDestroyAllocator(_allocator); });
}

void Renderer::init_default_data() {
    testMeshes = load_gltf_meshes(this, "assets/meshes/basicmesh.glb").value();
}

void Renderer::init_commands(uint32_t queueFamilyIndex) {
    for (uint32_t i = 0; i < FRAME_OVERLAP; i++) {
        _frames[i]._commandPool =
            create_command_pool(_device, queueFamilyIndex);
        _frames[i]._mainCommandBuffer =
            create_command_buffer(_device, _frames[i]._commandPool);
        _mainDeletionQueue.push_function([&]() {
            vkDestroyCommandPool(_device, _frames[i]._commandPool, nullptr);
        });
    }

    _immCommandPool = create_command_pool(_device, queueFamilyIndex);
    _immCommandBuffer = create_command_buffer(_device, _immCommandPool);

    _mainDeletionQueue.push_function([=, this]() {
        vkDestroyCommandPool(_device, _immCommandPool, nullptr);
    });
}

void Renderer::init_swap_chain() {
    SwapChainSupportDetails swapChainSupportDetails =
        query_swap_chain_support(_physicalDevice, _surface);
    _swapChainExtent =
        choose_swap_extent(_window, swapChainSupportDetails.capabilities);
    _swapChainImageFormat =
        choose_swap_surface_format(swapChainSupportDetails.formats);

    _swapChain = create_swap_chain(_window, swapChainSupportDetails, _device,
                                   _surface, _swapChainExtent,
                                   _swapChainImageFormat, _physicalDevice);
    _swapChainImages = get_swap_chain_images(_device, _swapChain);
    _swapChainImageViews = create_image_views(_device, _swapChainImages,
                                              _swapChainImageFormat.format);
}

void Renderer::init_pipelines() { init_mesh_pipeline(); }

void Renderer::init_mesh_pipeline() {
    VkShaderModule meshFragShader;
    if (!vkutil::load_shader_module("shaders/spv/shader.frag.spv", _device,
                                    &meshFragShader)) {
        fmt::print("Error when building the mesh fragment shader module");
    } else {
        fmt::print("Mesh fragment shader succesfully loaded");
    }

    VkShaderModule meshVertexShader;
    if (!vkutil::load_shader_module("shaders/spv/mesh.vert.spv", _device,
                                    &meshVertexShader)) {
        fmt::print("Error when building the mesh vertex shader module");
    } else {
        fmt::print("Mesh vertex shader succesfully loaded");
    }

    VkPushConstantRange bufferRange{};
    bufferRange.offset = 0;
    bufferRange.size = sizeof(GPUDrawPushConstants);
    bufferRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pSetLayouts = nullptr;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &bufferRange;
    VK_CHECK(vkCreatePipelineLayout(_device, &pipelineLayoutInfo, nullptr,
                                    &_meshPipelineLayout));

    PipelineBuilder pipelineBuilder;

    pipelineBuilder._pipelineLayout = _meshPipelineLayout;
    pipelineBuilder.set_shaders(meshVertexShader, meshFragShader);
    pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
    pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    pipelineBuilder.set_multisampling_none();
    pipelineBuilder.disable_depthtest();
    pipelineBuilder.set_color_attachment_format(_drawImage.format);
    pipelineBuilder.set_depth_format(_depthImage.format);
    // TODO: Vkguide uses 0 as far and 1 as near
    pipelineBuilder.enable_depthtest(true, VK_COMPARE_OP_LESS_OR_EQUAL);
    pipelineBuilder.enable_blending_additive();
    _meshPipeline = pipelineBuilder.build_pipeline(_device);

    vkDestroyShaderModule(_device, meshFragShader, nullptr);
    vkDestroyShaderModule(_device, meshVertexShader, nullptr);

    _mainDeletionQueue.push_function([&]() {
        vkDestroyPipelineLayout(_device, _meshPipelineLayout, nullptr);
        vkDestroyPipeline(_device, _meshPipeline, nullptr);
    });
}

void Renderer::init_descriptors() {
    std::vector<DescriptorAllocator::PoolSizeRatio> sizes = {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}};

    _globalDescriptorAllocator.init_pool(_device, 10, sizes);

    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        _drawImageDescriptorLayout =
            builder.build(_device, VK_SHADER_STAGE_COMPUTE_BIT);
    }

    _drawImageDescriptors = _globalDescriptorAllocator.allocate(
        _device, _drawImageDescriptorLayout);

    DescriptorWriter writer;
    writer.write_image(0, _drawImage.imageView, VK_NULL_HANDLE,
                       VK_IMAGE_LAYOUT_GENERAL,
                       VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    writer.update_set(_device, _drawImageDescriptors);

    _mainDeletionQueue.push_function([&]() {
        _globalDescriptorAllocator.destroy_pool(_device);
        vkDestroyDescriptorSetLayout(_device, _drawImageDescriptorLayout,
                                     nullptr);
    });
}

void Renderer::init_sync_structures() {
    VkSemaphoreCreateInfo semaphore_create_info = {};
    semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fence_create_info = {};
    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (uint32_t i = 0; i < FRAME_OVERLAP; i++) {
        VK_CHECK(vkCreateFence(_device, &fence_create_info, nullptr,
                               &_frames[i]._renderFence));
        VK_CHECK(vkCreateSemaphore(_device, &semaphore_create_info, nullptr,
                                   &_frames[i]._renderSemaphore));
        VK_CHECK(vkCreateSemaphore(_device, &semaphore_create_info, nullptr,
                                   &_frames[i]._swapchainSemaphore));
        _mainDeletionQueue.push_function([&]() {
            vkDestroySemaphore(_device, _frames[i]._renderSemaphore, nullptr);
            vkDestroySemaphore(_device, _frames[i]._swapchainSemaphore,
                               nullptr);
            vkDestroyFence(_device, _frames[i]._renderFence, nullptr);
        });
    }

    VK_CHECK(vkCreateFence(_device, &fence_create_info, nullptr, &_immFence));
    _mainDeletionQueue.push_function(
        [=, this]() { vkDestroyFence(_device, _immFence, nullptr); });
}

void Renderer::deinit() {
    for (auto &mesh : testMeshes) {
        destroy_buffer(mesh->meshBuffers.indexBuffer);
        destroy_buffer(mesh->meshBuffers.vertexBuffer);
    }
    _mainDeletionQueue.flush();
    destroy_swap_chain();
    vkDestroyDevice(_device, nullptr);
    SDL_Vulkan_DestroySurface(_instance, _surface, nullptr);
    vkDestroyInstance(_instance, nullptr);
    SDL_Vulkan_UnloadLibrary();
}

void Renderer::resize_swap_chain() {
    vkDeviceWaitIdle(_device);
    destroy_swap_chain();
    init_swap_chain();
    _resizeRequested = false;
}

void Renderer::record_command_buffer(VkCommandBuffer buffer,
                                     uint32_t image_index) {
    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = 0;
    begin_info.pInheritanceInfo = nullptr;
    VK_CHECK(vkBeginCommandBuffer(buffer, &begin_info));

    vkutil::transition_image(buffer, _drawImage.image,
                             VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_GENERAL);
    vkutil::transition_image(buffer, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL,
                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    vkutil::transition_image(buffer, _depthImage.image,
                             VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    // NOTE: Begin a rendering pass connected to our draw image
    VkRenderingAttachmentInfo colorAttachment = {};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = _drawImage.imageView;
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingAttachmentInfo depthAttachment = {};
    depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.imageView = _depthImage.imageView;
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    // TODO: Vkguide uses 0.f as the far value here, do we need that?
    depthAttachment.clearValue.depthStencil.depth = 1.f;

    VkRenderingInfo renderInfo = {};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachments = &colorAttachment;
    renderInfo.renderArea = VkRect2D{VkOffset2D{0, 0}, _drawExtent};
    renderInfo.layerCount = 1;

    vkCmdBeginRendering(buffer, &renderInfo);
    vkCmdBindPipeline(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _meshPipeline);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(_drawExtent.width);
    viewport.height = static_cast<float>(_drawExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(buffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = _drawExtent;
    vkCmdSetScissor(buffer, 0, 1, &scissor);

    glm::mat4 view = glm::translate(glm::vec3{0, 0, -5});
    // camera projection
    glm::mat4 projection = glm::perspective(
        glm::radians(35.0f),
        (float)_drawExtent.width / (float)_drawExtent.height, 0.0f, 1.0f);

    // invert Y direction
    projection[1][1] *= -1;

    GPUDrawPushConstants pushConstants;
    pushConstants.worldMatrix = projection * view;
    pushConstants.vertexBuffer = testMeshes[2]->meshBuffers.vertexBufferAddress;

    vkCmdPushConstants(buffer, _meshPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
                       0, sizeof(GPUDrawPushConstants), &pushConstants);
    vkCmdBindIndexBuffer(buffer, testMeshes[2]->meshBuffers.indexBuffer.buffer,
                         0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(buffer, testMeshes[2]->surfaces[0].count, 1,
                     testMeshes[2]->surfaces[0].startIndex, 0, 0);

    vkCmdEndRendering(buffer);

    vkutil::transition_image(buffer, _drawImage.image,
                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                             VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vkutil::transition_image(buffer, _swapChainImages[image_index],
                             VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    vkutil::copy_image_to_image(buffer, _drawImage.image,
                                _swapChainImages[image_index], _drawExtent,
                                _swapChainExtent);

    vkutil::transition_image(buffer, _swapChainImages[image_index],
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    vkutil::transition_image(buffer, _swapChainImages[image_index],
                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                             VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    VK_CHECK(vkEndCommandBuffer(buffer));
}

void Renderer::draw_frame() {
    FrameData *currentFrame = &get_current_frame();
    vkWaitForFences(_device, 1, &currentFrame->_renderFence, VK_TRUE,
                    UINT64_MAX);
    currentFrame->_deletionQueue.flush();
    vkResetFences(_device, 1, &currentFrame->_renderFence);

    uint32_t image_index;
    VkResult acquireResult = vkAcquireNextImageKHR(
        _device, _swapChain, UINT64_MAX, currentFrame->_swapchainSemaphore,
        VK_NULL_HANDLE, &image_index);
    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
        _resizeRequested = true;
        return;
    }

    _drawExtent.width =
        std::min(_swapChainExtent.width, _drawImage.extent.width);
    _drawExtent.height =
        std::min(_swapChainExtent.height, _drawImage.extent.height);

    vkResetCommandBuffer(currentFrame->_mainCommandBuffer, 0);
    record_command_buffer(currentFrame->_mainCommandBuffer, image_index);

    VkCommandBufferSubmitInfo cmdSubmitInfo = {};
    cmdSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    cmdSubmitInfo.commandBuffer = currentFrame->_mainCommandBuffer;
    cmdSubmitInfo.deviceMask = 0;

    VkSemaphoreSubmitInfo waitInfo = create_semaphore_submit_info(
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
        currentFrame->_swapchainSemaphore);
    VkSemaphoreSubmitInfo signalInfo = create_semaphore_submit_info(
        VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, currentFrame->_renderSemaphore);

    VkSubmitInfo2 submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo.waitSemaphoreInfoCount = 1;
    submitInfo.pWaitSemaphoreInfos = &waitInfo;
    submitInfo.signalSemaphoreInfoCount = 1;
    submitInfo.pSignalSemaphoreInfos = &signalInfo;
    submitInfo.commandBufferInfoCount = 1;
    submitInfo.pCommandBufferInfos = &cmdSubmitInfo;

    VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submitInfo,
                            currentFrame->_renderFence));

    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &currentFrame->_renderSemaphore;
    VkSwapchainKHR swap_chains[] = {_swapChain};
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swap_chains;
    present_info.pImageIndices = &image_index;
    present_info.pResults = nullptr;

    VkResult presentResult =
        vkQueuePresentKHR(_presentationQueue, &present_info);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR) {
        _resizeRequested = true;
        return;
    }
    _frameNumber++;
}

void Renderer::immediate_submit(
    std::function<void(VkCommandBuffer cmd)> &&function) {
    VK_CHECK(vkResetFences(_device, 1, &_immFence));
    VK_CHECK(vkResetCommandBuffer(_immCommandBuffer, 0));

    VkCommandBuffer cmd = _immCommandBuffer;

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0;
    beginInfo.pInheritanceInfo = nullptr;

    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    function(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmdSubmitInfo = {};
    cmdSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    cmdSubmitInfo.commandBuffer = cmd;
    cmdSubmitInfo.deviceMask = 0;

    VkSubmitInfo2 submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo.commandBufferInfoCount = 1;
    submitInfo.pCommandBufferInfos = &cmdSubmitInfo;
    submitInfo.signalSemaphoreInfoCount = 0;
    submitInfo.waitSemaphoreInfoCount = 0;

    VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submitInfo, _immFence));

    VK_CHECK(vkWaitForFences(_device, 1, &_immFence, true, 9999999999));
}

GPUMeshBuffers Renderer::uploadMesh(std::span<uint32_t> indices,
                                    std::span<Vertex> vertices) {
    const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
    const size_t indexBufferSize = indices.size() * sizeof(uint32_t);

    GPUMeshBuffers newSurface;

    newSurface.vertexBuffer = create_buffer(
        vertexBufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);

    VkBufferDeviceAddressInfo deviceAdressInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = newSurface.vertexBuffer.buffer};
    newSurface.vertexBufferAddress =
        vkGetBufferDeviceAddress(_device, &deviceAdressInfo);

    newSurface.indexBuffer = create_buffer(indexBufferSize,
                                           VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                                               VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                           VMA_MEMORY_USAGE_GPU_ONLY);

    AllocatedBuffer staging = create_buffer(vertexBufferSize + indexBufferSize,
                                            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                            VMA_MEMORY_USAGE_CPU_ONLY);

    void *data = staging.allocation->GetMappedData();

    memcpy(data, vertices.data(), vertexBufferSize);
    memcpy((char *)data + vertexBufferSize, indices.data(), indexBufferSize);

    // TODO: Here we are waiting for the GPU command to fully execute before
    // continuing with CPU logic. It is probably better to use a separate thread
    // for uploading / creating / deleting the staging buffer
    immediate_submit([&](VkCommandBuffer cmd) {
        VkBufferCopy vertexCopy{0};
        vertexCopy.dstOffset = 0;
        vertexCopy.srcOffset = 0;
        vertexCopy.size = vertexBufferSize;

        vkCmdCopyBuffer(cmd, staging.buffer, newSurface.vertexBuffer.buffer, 1,
                        &vertexCopy);

        VkBufferCopy indexCopy{0};
        indexCopy.dstOffset = 0;
        indexCopy.srcOffset = vertexBufferSize;
        indexCopy.size = indexBufferSize;

        vkCmdCopyBuffer(cmd, staging.buffer, newSurface.indexBuffer.buffer, 1,
                        &indexCopy);
    });

    destroy_buffer(staging);

    return newSurface;
}

AllocatedBuffer Renderer::create_buffer(size_t allocSize,
                                        VkBufferUsageFlags usage,
                                        VmaMemoryUsage memoryUsage) {
    VkBufferCreateInfo bufferInfo = {.sType =
                                         VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.pNext = nullptr;
    bufferInfo.size = allocSize;

    bufferInfo.usage = usage;

    VmaAllocationCreateInfo vmaallocInfo = {};
    vmaallocInfo.usage = memoryUsage;
    vmaallocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    AllocatedBuffer newBuffer;

    VK_CHECK(vmaCreateBuffer(_allocator, &bufferInfo, &vmaallocInfo,
                             &newBuffer.buffer, &newBuffer.allocation,
                             &newBuffer.info));

    return newBuffer;
}

void Renderer::destroy_buffer(const AllocatedBuffer &buffer) {
    vmaDestroyBuffer(_allocator, buffer.buffer, buffer.allocation);
}

void Renderer::destroy_swap_chain() {
    vkDestroySwapchainKHR(_device, _swapChain, nullptr);
    for (int i = 0; i < _swapChainImageViews.size(); i++) {
        vkDestroyImageView(_device, _swapChainImageViews[i], nullptr);
    }
}
