#ifndef RENDERER_RENDERER_H

#include <SDL3/SDL.h>
#include <vector>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

class Renderer {
  private:
    SDL_Window *window;
    VkInstance instance;
    VkDevice device;
    VkSurfaceKHR surface;
    VkSwapchainKHR swap_chain;
    VkExtent2D swap_chain_extent;
    std::vector<VkImageView> image_views;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
    VkRenderPass render_pass;
    std::vector<VkFramebuffer> frame_buffers;
    VkBuffer vertex_buffer;
    VkCommandPool command_pool;
    VkCommandBuffer command_buffer;
    VkQueue graphics_queue;
    VkQueue presentation_queue;

    VkSemaphore image_available;
    VkSemaphore render_finished;
    VkFence in_flight;

    void record_command_buffer(VkCommandBuffer buffer, uint32_t image_index);

  public:
    Renderer(SDL_Window *window);
    void deinit();
    void draw_frame();
};

#define RENDERER_RENDERER_H
#endif
