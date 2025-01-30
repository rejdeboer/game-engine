#include "context.hpp"
#include "SDL3/SDL_vulkan.h"

VulkanContext::VulkanContext(VkInstance instance, VkDevice device,
                             VkSurfaceKHR surface, VkQueue graphics_queue,
                             VkQueue presentation_queue) {
    instance = instance;
    device = device;
    surface = surface;
    graphics_queue = graphics_queue;
    presentation_queue = presentation_queue;
}

void VulkanContext::deinit() {
    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);
    SDL_Vulkan_UnloadLibrary();
}
