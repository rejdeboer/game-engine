#ifndef RENDERER_CONTEXT_H

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

class VulkanContext {
  private:
    VkInstance instance;
    VkDevice device;
    VkSurfaceKHR surface;

  public:
    VulkanContext(VkInstance instance, VkDevice device, VkSurfaceKHR surface);
    void deinit();
};

#define RENDERER_CONTEXT_H
#endif
