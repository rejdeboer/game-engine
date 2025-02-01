#include "context.hpp"
#include "SDL3/SDL_vulkan.h"

VulkanContext::VulkanContext(VkInstance instance, VkDevice device,
                             VkSurfaceKHR surface, VkSwapchainKHR swap_chain,
                             std::vector<VkImageView> image_views,
                             VkPipelineLayout pipeline_layout,
                             VkPipeline pipeline, VkRenderPass render_pass,
                             std::vector<VkFramebuffer> frame_buffers,
                             VkCommandPool command_pool, VkQueue graphics_queue,
                             VkQueue presentation_queue) {
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
    graphics_queue = graphics_queue;
    presentation_queue = presentation_queue;
}

void VulkanContext::deinit() {
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
