#include "renderer.hpp"
#include "SDL3/SDL_vulkan.h"
#include "image.h"
#include "init.h"
#include "vertex.h"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

const std::vector<Vertex> vertices = {{{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
                                      {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
                                      {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}};

Renderer::Renderer(SDL_Window *window) {
    _window = window;
    instance = create_vulkan_instance(window);
    surface = create_surface(window, instance);
    VkPhysicalDevice physical_device = pick_physical_device(instance, surface);
    QueueFamilyIndices queue_family_indices =
        find_compatible_queue_family_indices(physical_device, surface);
    assert(queue_family_indices.is_complete());

    uint32_t graphics_index = queue_family_indices.graphics_family.value();
    uint32_t presentation_index = queue_family_indices.present_family.value();
    device = create_device(physical_device, graphics_index, presentation_index);
    _allocator = create_allocator(instance, physical_device, device);

    init_swap_chain(physical_device, graphics_index, presentation_index);

    // render_pass = create_render_pass(device, surface_format.format);
    PipelineContext pipeline_context =
        create_graphics_pipeline(device, _swapChainExtent);
    pipeline = pipeline_context.pipeline;
    pipeline_layout = pipeline_context.layout;
    // frame_buffers = create_frame_buffers(device, render_pass, image_views,
    //                                      swap_chain_extent);
    vertex_buffer = create_vertex_buffer(device);
    vertex_buffer_memory =
        allocate_vertex_buffer(physical_device, device, vertex_buffer);

    init_commands(graphics_index);
    init_sync_structures();
    init_descriptors();

    _graphicsQueue = get_device_queue(device, graphics_index, 0);
    _presentationQueue = get_device_queue(device, presentation_index, 0);

    _mainDeletionQueue.push_function(
        [&]() { vmaDestroyAllocator(_allocator); });

    // TODO: Cleanup
    void *data;
    vkMapMemory(device, vertex_buffer_memory, 0,
                vertices.size() * sizeof(Vertex), 0, &data);
    memcpy(data, vertices.data(), vertices.size() * sizeof(Vertex));
    vkUnmapMemory(device, vertex_buffer_memory);
}

void Renderer::init_commands(uint32_t queueFamilyIndex) {
    for (uint32_t i = 0; i < FRAME_OVERLAP; i++) {
        _frames[i]._commandPool = create_command_pool(device, queueFamilyIndex);
        _frames[i]._mainCommandBuffer =
            create_command_buffer(device, _frames[i]._commandPool);
        _mainDeletionQueue.push_function([&]() {
            vkDestroyCommandPool(device, _frames[i]._commandPool, nullptr);
        });
    }
}

void Renderer::init_swap_chain(VkPhysicalDevice physicalDevice,
                               uint32_t graphicsQueueFamilyIndex,
                               uint32_t presentationQueueFamilyIndex) {
    SwapChainSupportDetails swapChainSupportDetails =
        query_swap_chain_support(physicalDevice, surface);
    _swapChainExtent =
        choose_swap_extent(_window, swapChainSupportDetails.capabilities);
    VkSurfaceFormatKHR surfaceFormat =
        choose_swap_surface_format(swapChainSupportDetails.formats);
    swap_chain = create_swap_chain(
        _window, swapChainSupportDetails, device, surface, _swapChainExtent,
        surfaceFormat, graphicsQueueFamilyIndex, presentationQueueFamilyIndex);
    _swapChainImages = get_swap_chain_images(device, swap_chain);
    _swapChainImageViews =
        create_image_views(device, _swapChainImages, surfaceFormat.format);

    _drawImage = create_draw_image(device, _allocator, _swapChainExtent);
    _mainDeletionQueue.push_function([&]() {
        vkDestroyImageView(device, _drawImage.imageView, nullptr);
        vmaDestroyImage(_allocator, _drawImage.image, _drawImage.allocation);
    });
}

void Renderer::init_descriptors() {
    std::vector<DescriptorAllocator::PoolSizeRatio> sizes = {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}};

    _descriptorAllocator.init_pool(device, 10, sizes);

    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        _drawImageDescriptorLayout =
            builder.build(device, VK_SHADER_STAGE_COMPUTE_BIT);
    }

    _drawImageDescriptors =
        _descriptorAllocator.allocate(device, _drawImageDescriptorLayout);

    VkDescriptorImageInfo imgInfo = {};
    imgInfo.imageView = _drawImage.imageView;
    imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkWriteDescriptorSet drawImageWrite = {};
    drawImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    drawImageWrite.dstBinding = 0;
    drawImageWrite.dstSet = _drawImageDescriptors;
    drawImageWrite.descriptorCount = 1;
    drawImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    drawImageWrite.pImageInfo = &imgInfo;
    vkUpdateDescriptorSets(device, 1, &drawImageWrite, 0, nullptr);

    _mainDeletionQueue.push_function([&]() {
        _descriptorAllocator.destroy_pool(device);
        vkDestroyDescriptorSetLayout(device, _drawImageDescriptorLayout,
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
        VK_CHECK(vkCreateFence(device, &fence_create_info, nullptr,
                               &_frames[i]._renderFence));
        VK_CHECK(vkCreateSemaphore(device, &semaphore_create_info, nullptr,
                                   &_frames[i]._renderSemaphore));
        VK_CHECK(vkCreateSemaphore(device, &semaphore_create_info, nullptr,
                                   &_frames[i]._swapchainSemaphore));
        _mainDeletionQueue.push_function([&]() {
            vkDestroySemaphore(device, _frames[i]._renderSemaphore, nullptr);
            vkDestroySemaphore(device, _frames[i]._swapchainSemaphore, nullptr);
            vkDestroyFence(device, _frames[i]._renderFence, nullptr);
        });
    }
}

void Renderer::deinit() {
    _mainDeletionQueue.flush();
    for (auto frame_buffer : frame_buffers) {
        vkDestroyFramebuffer(device, frame_buffer, nullptr);
    }
    vkDestroyRenderPass(device, render_pass, nullptr);
    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
    for (auto image_view : _swapChainImageViews) {
        vkDestroyImageView(device, image_view, nullptr);
    }
    vkDestroySwapchainKHR(device, swap_chain, nullptr);
    vkDestroyBuffer(device, vertex_buffer, nullptr);
    vkFreeMemory(device, vertex_buffer_memory, nullptr);
    vkDestroyDevice(device, nullptr);
    SDL_Vulkan_DestroySurface(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);
    SDL_Vulkan_UnloadLibrary();
}

void Renderer::record_command_buffer(VkCommandBuffer buffer,
                                     uint32_t image_index) {
    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = 0;
    begin_info.pInheritanceInfo = nullptr;
    VK_CHECK(vkBeginCommandBuffer(buffer, &begin_info));

    // VkRenderPassBeginInfo render_pass_info = {};
    // render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    // render_pass_info.renderPass = render_pass;
    // render_pass_info.framebuffer = frame_buffers[image_index];
    // render_pass_info.renderArea.offset = {0, 0};
    // render_pass_info.renderArea.extent = swap_chain_extent;
    // VkClearValue clear_color = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    // render_pass_info.clearValueCount = 1;
    // render_pass_info.pClearValues = &clear_color;
    // vkCmdBeginRenderPass(buffer, &render_pass_info,
    // VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    vkutil::transition_image(buffer, _swapChainImages[image_index],
                             VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_GENERAL);

    VkClearColorValue clearValue = {{0.0f, 0.0f, 0.0f, 1.0f}};
    VkImageSubresourceRange clearRange =
        vkutil::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);
    vkCmdClearColorImage(buffer, _swapChainImages[image_index],
                         VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);

    vkutil::transition_image(buffer, _swapChainImages[image_index],
                             VK_IMAGE_LAYOUT_GENERAL,
                             VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    VkBuffer vertex_buffers[] = {vertex_buffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(buffer, 0, 1, vertex_buffers, offsets);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(_swapChainExtent.width);
    viewport.height = static_cast<float>(_swapChainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(buffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = _swapChainExtent;
    vkCmdSetScissor(buffer, 0, 1, &scissor);

    vkCmdDraw(buffer, static_cast<uint32_t>(vertices.size()), 1, 0, 0);
    vkCmdEndRenderPass(buffer);
    VK_CHECK(vkEndCommandBuffer(buffer));
}

void Renderer::draw_frame() {
    FrameData *currentFrame = &get_current_frame();
    vkWaitForFences(device, 1, &currentFrame->_renderFence, VK_TRUE,
                    UINT64_MAX);
    currentFrame->_deletionQueue.flush();
    vkResetFences(device, 1, &currentFrame->_renderFence);

    uint32_t image_index;
    vkAcquireNextImageKHR(device, swap_chain, UINT64_MAX,
                          currentFrame->_renderSemaphore, VK_NULL_HANDLE,
                          &image_index);

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

    // VkSubmitInfo submit_info = {};
    // submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    //
    // VkSemaphore wait_semaphores[] = {currentFrame->_renderSemaphore};
    // VkPipelineStageFlags wait_stages[] = {
    //     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    // submit_info.waitSemaphoreCount = 1;
    // submit_info.pWaitSemaphores = wait_semaphores;
    // submit_info.pWaitDstStageMask = wait_stages;
    // submit_info.commandBufferCount = 1;
    // submit_info.pCommandBuffers = &currentFrame->_mainCommandBuffer;
    // VkSemaphore signal_semaphores[] = {currentFrame->_swapchainSemaphore};
    // submit_info.signalSemaphoreCount = 1;
    // submit_info.pSignalSemaphores = signal_semaphores;
    //
    // VK_CHECK(vkQueueSubmit(_graphicsQueue, 1, &submit_info,
    //                        currentFrame->_renderFence));

    VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submitInfo,
                            currentFrame->_renderFence));

    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &currentFrame->_renderSemaphore;
    VkSwapchainKHR swap_chains[] = {swap_chain};
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swap_chains;
    present_info.pImageIndices = &image_index;
    present_info.pResults = nullptr;

    vkQueuePresentKHR(_presentationQueue, &present_info);
    _frameNumber++;
}
