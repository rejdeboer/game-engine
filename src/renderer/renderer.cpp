#include "renderer.hpp"
#include "SDL3/SDL_vulkan.h"
#include "bootstrap.h"
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
    command_pool = create_command_pool(device, graphics_index);
    command_buffer = create_command_buffer(device, command_pool);
    graphics_queue = get_device_queue(device, graphics_index, 0);
    presentation_queue = get_device_queue(device, presentation_index, 0);

    VkSemaphoreCreateInfo semaphore_create_info = {};
    semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fence_create_info = {};
    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    if (vkCreateSemaphore(device, &semaphore_create_info, nullptr,
                          &image_available) != VK_SUCCESS ||
        vkCreateSemaphore(device, &semaphore_create_info, nullptr,
                          &render_finished) != VK_SUCCESS ||
        vkCreateFence(device, &fence_create_info, nullptr, &in_flight) !=
            VK_SUCCESS) {
        throw std::runtime_error("failed to create sync primitives");
    }

    // TODO: Cleanup
    void *data;
    vkMapMemory(device, vertex_buffer_memory, 0,
                vertices.size() * sizeof(Vertex), 0, &data);
    memcpy(data, vertices.data(), vertices.size() * sizeof(Vertex));
    vkUnmapMemory(device, vertex_buffer_memory);
}

void Renderer::deinit() {
    vkDestroySemaphore(device, image_available, nullptr);
    vkDestroySemaphore(device, render_finished, nullptr);
    vkDestroyFence(device, in_flight, nullptr);
    vkDestroyCommandPool(device, command_pool, nullptr);
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
    if (vkBeginCommandBuffer(buffer, &begin_info) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin command buffer");
    }

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
    if (vkEndCommandBuffer(buffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer");
    }
}

void Renderer::draw_frame() {
    vkWaitForFences(device, 1, &in_flight, VK_TRUE, UINT64_MAX);
    vkResetFences(device, 1, &in_flight);

    uint32_t image_index;
    vkAcquireNextImageKHR(device, swap_chain, UINT64_MAX, image_available,
                          VK_NULL_HANDLE, &image_index);

    vkResetCommandBuffer(command_buffer, 0);
    record_command_buffer(command_buffer, image_index);

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore wait_semaphores[] = {image_available};
    VkPipelineStageFlags wait_stages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;
    VkSemaphore signal_semaphores[] = {render_finished};
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;

    if (vkQueueSubmit(graphics_queue, 1, &submit_info, in_flight) !=
        VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer");
    }

    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = signal_semaphores;
    VkSwapchainKHR swap_chains[] = {swap_chain};
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swap_chains;
    present_info.pImageIndices = &image_index;
    present_info.pResults = nullptr;

    vkQueuePresentKHR(presentation_queue, &present_info);
}
