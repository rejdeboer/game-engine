#include "context.hpp"
#include "SDL3/SDL_vulkan.h"

VulkanContext::VulkanContext(VkInstance instance, VkDevice device,
                             VkSurfaceKHR surface) {
    instance = instance;
    device = device;
    surface = surface;
}

void VulkanContext::deinit() {
    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);
    SDL_Vulkan_UnloadLibrary();
}
