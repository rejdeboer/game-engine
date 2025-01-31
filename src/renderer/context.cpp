#include "context.hpp"
#include "SDL3/SDL_vulkan.h"

VulkanContext::VulkanContext(VkInstance instance, VkDevice device,
                             VkSurfaceKHR surface, VkSwapchainKHR swap_chain,
                             VkQueue graphics_queue,
                             VkQueue presentation_queue) {
    instance = instance;
    device = device;
    surface = surface;
    swap_chain = swap_chain;
    graphics_queue = graphics_queue;
    presentation_queue = presentation_queue;
}

void VulkanContext::deinit() {
    vkDestroySwapchainKHR(device, swap_chain, nullptr);
    vkDestroyDevice(device, nullptr);
    SDL_Vulkan_DestroySurface(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);
    SDL_Vulkan_UnloadLibrary();
}
