#include "context.hpp"
#include "SDL3/SDL_vulkan.h"

VulkanContext::VulkanContext(
    VkInstance instance, VkDevice device, VkSurfaceKHR surface,
    VkSwapchainKHR swap_chain, std::vector<VkImageView> image_views,
    VkPipelineLayout pipeline_layout, VkPipeline pipeline,
    VkRenderPass render_pass, std::vector<VkFramebuffer> frame_buffers,
    VkCommandPool command_pool, VkCommandBuffer command_buffer,
    VkQueue graphics_queue, VkQueue presentation_queue) {
    instance = instance;
    device = device;
    surface = surface;
    swap_chain = swap_chain;
    image_views = image_views;
    pipeline_layout = pipeline_layout;
    pipeline = pipeline;
    render_pass = render_pass;
    frame_buffers = frame_buffers;
    command_pool = command_pool;
    command_buffer = command_buffer;
    graphics_queue = graphics_queue;
    presentation_queue = presentation_queue;

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
}

void VulkanContext::deinit() {
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
    vkDestroyDevice(device, nullptr);
    SDL_Vulkan_DestroySurface(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);
    SDL_Vulkan_UnloadLibrary();
}

static void record_command_buffer(VkCommandBuffer buffer,
                                  uint32_t image_index) {
    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = 0;
    begin_info.pInheritanceInfo = nullptr;
    if (vkBeginCommandBuffer(buffer, &begin_info) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin command buffer");
    }
}

void VulkanContext::draw_frame() {
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
