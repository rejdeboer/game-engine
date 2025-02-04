#include "renderer.hpp"
#include "SDL3/SDL_vulkan.h"
#include "bootstrap.h"
#include "types.h"
#include "vertex.h"

const std::vector<Vertex> vertices = {{{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
                                      {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
                                      {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}};

Renderer::Renderer(SDL_Window *window) {
    instance = create_vulkan_instance(window);
    surface = create_surface(window, instance);
    VkPhysicalDevice physical_device = pick_physical_device(instance, surface);
    QueueFamilyIndices queue_family_indices =
        find_compatible_queue_family_indices(physical_device, surface);
    assert(queue_family_indices.is_complete());

    uint32_t graphics_index = queue_family_indices.graphics_family.value();
    uint32_t presentation_index = queue_family_indices.present_family.value();
    device = create_device(physical_device, graphics_index, presentation_index);

    SwapChainSupportDetails swap_chain_support_details =
        query_swap_chain_support(physical_device, surface);
    swap_chain_extent =
        choose_swap_extent(window, swap_chain_support_details.capabilities);
    VkSurfaceFormatKHR surface_format =
        choose_swap_surface_format(swap_chain_support_details.formats);
    swap_chain = create_swap_chain(window, swap_chain_support_details, device,
                                   surface, swap_chain_extent, surface_format,
                                   graphics_index, presentation_index);
    std::vector<VkImage> swap_chain_images =
        get_swap_chain_images(device, swap_chain);
    image_views =
        create_image_views(device, swap_chain_images, surface_format.format);

    render_pass = create_render_pass(device, surface_format.format);
    PipelineContext pipeline_context =
        create_graphics_pipeline(device, render_pass, swap_chain_extent);
    pipeline = pipeline_context.pipeline;
    pipeline_layout = pipeline_context.layout;
    frame_buffers = create_frame_buffers(device, render_pass, image_views,
                                         swap_chain_extent);
    vertex_buffer = create_vertex_buffer(device);
    vertex_buffer_memory =
        allocate_vertex_buffer(physical_device, device, vertex_buffer);

    for (uint32_t i = 0; i < FRAME_OVERLAP; i++) {
        _frames[i]._commandPool = create_command_pool(device, graphics_index);
        _frames[i]._mainCommandBuffer =
            create_command_buffer(device, _frames[i]._commandPool);
    }

    _graphicsQueue = get_device_queue(device, graphics_index, 0);
    _presentationQueue = get_device_queue(device, presentation_index, 0);

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
    }

    // TODO: Cleanup
    void *data;
    vkMapMemory(device, vertex_buffer_memory, 0,
                vertices.size() * sizeof(Vertex), 0, &data);
    memcpy(data, vertices.data(), vertices.size() * sizeof(Vertex));
    vkUnmapMemory(device, vertex_buffer_memory);
}

void Renderer::deinit() {
    for (uint32_t i = 0; i < FRAME_OVERLAP; i++) {
        vkDestroyCommandPool(device, _frames[i]._commandPool, nullptr);
        vkDestroySemaphore(device, _frames[i]._renderSemaphore, nullptr);
        vkDestroySemaphore(device, _frames[i]._swapchainSemaphore, nullptr);
        vkDestroyFence(device, _frames[i]._renderFence, nullptr);
    }
    for (auto frame_buffer : frame_buffers) {
        vkDestroyFramebuffer(device, frame_buffer, nullptr);
    }
    vkDestroyRenderPass(device, render_pass, nullptr);
    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
    for (auto image_view : image_views) {
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

    VkRenderPassBeginInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = render_pass;
    render_pass_info.framebuffer = frame_buffers[image_index];
    render_pass_info.renderArea.offset = {0, 0};
    render_pass_info.renderArea.extent = swap_chain_extent;
    VkClearValue clear_color = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    render_pass_info.clearValueCount = 1;
    render_pass_info.pClearValues = &clear_color;
    vkCmdBeginRenderPass(buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    VkBuffer vertex_buffers[] = {vertex_buffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(buffer, 0, 1, vertex_buffers, offsets);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swap_chain_extent.width);
    viewport.height = static_cast<float>(swap_chain_extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(buffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swap_chain_extent;
    vkCmdSetScissor(buffer, 0, 1, &scissor);

    vkCmdDraw(buffer, static_cast<uint32_t>(vertices.size()), 1, 0, 0);
    vkCmdEndRenderPass(buffer);
    VK_CHECK(vkEndCommandBuffer(buffer));
}

void Renderer::draw_frame() {
    FrameData *currentFrame = &get_current_frame();
    vkWaitForFences(device, 1, &currentFrame->_renderFence, VK_TRUE,
                    UINT64_MAX);
    vkResetFences(device, 1, &currentFrame->_renderFence);

    uint32_t image_index;
    vkAcquireNextImageKHR(device, swap_chain, UINT64_MAX,
                          currentFrame->_renderSemaphore, VK_NULL_HANDLE,
                          &image_index);

    vkResetCommandBuffer(currentFrame->_mainCommandBuffer, 0);
    record_command_buffer(currentFrame->_mainCommandBuffer, image_index);

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore wait_semaphores[] = {currentFrame->_renderSemaphore};
    VkPipelineStageFlags wait_stages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &currentFrame->_mainCommandBuffer;
    VkSemaphore signal_semaphores[] = {currentFrame->_swapchainSemaphore};
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;

    VK_CHECK(vkQueueSubmit(_graphicsQueue, 1, &submit_info,
                           currentFrame->_renderFence));

    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = signal_semaphores;
    VkSwapchainKHR swap_chains[] = {swap_chain};
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swap_chains;
    present_info.pImageIndices = &image_index;
    present_info.pResults = nullptr;

    vkQueuePresentKHR(_presentationQueue, &present_info);
}
