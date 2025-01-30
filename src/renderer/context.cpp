#include "context.hpp"
#include "SDL3/SDL_vulkan.h"

VulkanContext::VulkanContext(VkInstance instance, VkDevice device) {
    instance = instance;
    device = device;
}

void VulkanContext::deinit() {
    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);
    SDL_Vulkan_UnloadLibrary();
}
