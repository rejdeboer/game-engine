#ifndef RENDERER_CONFIG_H

#include "types.h"
#include "vk_mem_alloc.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <cassert>
#include <optional>
#include <vector>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;
};

struct SwapChainProps {
    VkSurfaceFormatKHR imageFormat;
    VkExtent2D extent;
    VkPresentModeKHR presentMode;
    uint32_t imageCount;
    uint32_t graphicsQueueFamilyIndex;
    uint32_t presentQueueFamilyIndex;
};

struct QueueFamilyIndices {
    std::optional<uint32_t> graphics_family;
    std::optional<uint32_t> present_family;
    bool is_complete() {
        return graphics_family.has_value() && present_family.has_value();
    }
};

struct PipelineContext {
    VkPipelineLayout layout;
    VkPipeline pipeline;
};

VkInstance create_vulkan_instance(SDL_Window *window);
VkSurfaceKHR create_surface(SDL_Window *window, VkInstance instance);
VkPhysicalDevice pick_physical_device(VkInstance instance,
                                      VkSurfaceKHR surface);
VkSurfaceFormatKHR choose_swap_surface_format(
    const std::vector<VkSurfaceFormatKHR> &available_formats);
VkPresentModeKHR
choose_swap_present_mode(const std::vector<VkPresentModeKHR> &available_modes);
VkExtent2D choose_swap_extent(SDL_Window *window,
                              const VkSurfaceCapabilitiesKHR &capabilities);
QueueFamilyIndices find_compatible_queue_family_indices(VkPhysicalDevice device,
                                                        VkSurfaceKHR surface);
SwapChainSupportDetails query_swap_chain_support(VkPhysicalDevice device,
                                                 VkSurfaceKHR surface);
VkDevice create_device(VkPhysicalDevice physical_device,
                       uint32_t graphics_index, uint32_t presentation_index);
VkSwapchainKHR create_swap_chain(SDL_Window *window,
                                 SwapChainSupportDetails support_details,
                                 VkDevice device, VkSurfaceKHR surface,
                                 VkExtent2D extent,
                                 VkSurfaceFormatKHR surface_format,
                                 VkPhysicalDevice physicalDevice);
std::vector<VkFramebuffer>
create_frame_buffers(VkDevice device, VkRenderPass render_pass,
                     std::vector<VkImageView> image_views,
                     VkExtent2D swap_chain_extent);
std::vector<VkImage> get_swap_chain_images(VkDevice device,
                                           VkSwapchainKHR swap_chain);
std::vector<VkImageView> create_image_views(VkDevice device,
                                            std::vector<VkImage> images,
                                            VkFormat image_format);
AllocatedImage create_draw_image(VkDevice device, VmaAllocator allocator,
                                 VkExtent2D swapChainExtent);
AllocatedImage create_depth_image(VkDevice device, VmaAllocator allocator,
                                  VkExtent2D swapChainExtent);
PipelineContext create_graphics_pipeline(VkDevice device, VkExtent2D extent,
                                         VkFormat colorAttachmentFormat);
VkRenderPass create_render_pass(VkDevice device,
                                VkFormat color_attachment_format);
VkCommandPool create_command_pool(VkDevice device, uint32_t queue_family_index);
VkCommandBuffer create_command_buffer(VkDevice device,
                                      VkCommandPool command_pool);
VkBuffer create_vertex_buffer(VkDevice device);
VkQueue get_device_queue(VkDevice device, uint32_t family_index,
                         uint32_t queue_index);
VkDeviceMemory allocate_vertex_buffer(VkPhysicalDevice physical_device,
                                      VkDevice device, VkBuffer buffer);
VkSemaphoreSubmitInfo
create_semaphore_submit_info(VkPipelineStageFlags2 stageMask,
                             VkSemaphore semaphore);
VmaAllocator create_allocator(VkInstance instance,
                              VkPhysicalDevice physicalDevice, VkDevice device);

#define RENDERER_CONFIG_H
#endif
