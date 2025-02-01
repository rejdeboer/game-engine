#ifndef RENDERER_CONTEXT_H

#include <vector>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

class VulkanContext {
  private:
    VkInstance instance;
    VkDevice device;
    VkSurfaceKHR surface;
    VkSwapchainKHR swap_chain;
    std::vector<VkImageView> image_views;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
    VkRenderPass render_pass;
    std::vector<VkFramebuffer> frame_buffers;
    VkCommandPool command_pool;
    VkCommandBuffer command_buffer;
    VkQueue graphics_queue;
    VkQueue presentation_queue;

  public:
    VulkanContext(VkInstance instance, VkDevice device, VkSurfaceKHR surface,
                  VkSwapchainKHR swap_chain,
                  std::vector<VkImageView> image_views,
                  VkPipelineLayout pipeline_layout, VkPipeline pipeline,
                  VkRenderPass render_pass,
                  std::vector<VkFramebuffer> frame_buffers,
                  VkCommandPool command_pool, VkCommandBuffer command_buffer,
                  VkQueue graphics_queue, VkQueue presentation_queue);
    void deinit();
};

#define RENDERER_CONTEXT_H
#endif
