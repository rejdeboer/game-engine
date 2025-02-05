#pragma once
#include "descriptor.h"
#include "types.h"
#include <SDL3/SDL.h>
#include <deque>
#include <vector>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

constexpr unsigned int FRAME_OVERLAP = 2;

struct DeletionQueue {
    std::deque<std::function<void()>> deletors;

    void push_function(std::function<void()> &&function) {
        deletors.push_back(function);
    }

    void flush() {
        // reverse iterate the deletion queue to execute all the functions
        for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
            (*it)(); // call functors
        }

        deletors.clear();
    }
};

struct FrameData {
    VkSemaphore _swapchainSemaphore, _renderSemaphore;
    VkFence _renderFence;

    // DescriptorAllocatorGrowable _frameDescriptors;
    DeletionQueue _deletionQueue;

    VkCommandPool _commandPool;
    VkCommandBuffer _mainCommandBuffer;
};

class Renderer {
  private:
    SDL_Window *_window;
    VkInstance instance;
    VkDevice device;
    VkSurfaceKHR surface;
    VkSwapchainKHR swap_chain;
    VkExtent2D _swapChainExtent;
    std::vector<VkImage> _swapChainImages;
    std::vector<VkImageView> _swapChainImageViews;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
    VkRenderPass render_pass;
    FrameData _frames[FRAME_OVERLAP];
    uint32_t _frameNumber;
    std::vector<VkFramebuffer> frame_buffers;
    VkBuffer vertex_buffer;
    VkDeviceMemory vertex_buffer_memory;
    VkQueue _graphicsQueue;
    VkQueue _presentationQueue;
    DeletionQueue _mainDeletionQueue;

    DescriptorAllocator _descriptorAllocator;
    VkDescriptorSet _drawImageDescriptors;
    VkDescriptorSetLayout _drawImageDescriptorLayout;

    AllocatedImage _drawImage;

    VmaAllocator _allocator;

    void record_command_buffer(VkCommandBuffer buffer, uint32_t image_index);
    FrameData &get_current_frame() {
        return _frames[_frameNumber % FRAME_OVERLAP];
    };

    void init_swap_chain(VkPhysicalDevice physicalDevice,
                         uint32_t graphicsQueueFamilyIndex,
                         uint32_t presentationQueueFamilyIndex);
    void init_commands(uint32_t queueFamilyIndex);
    void init_sync_structures();
    void init_descriptors();

  public:
    Renderer(SDL_Window *window);
    void deinit();
    void draw_frame();
};
