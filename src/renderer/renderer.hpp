#pragma once
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
    FrameData _frames[FRAME_OVERLAP];
    uint32_t _frameNumber;
    std::vector<VkFramebuffer> frame_buffers;
    VkBuffer vertex_buffer;
    VkDeviceMemory vertex_buffer_memory;
    VkCommandPool command_pool;
    VkCommandBuffer command_buffer;
    VkQueue graphics_queue;
    VkQueue presentation_queue;
    DeletionQueue _mainDeletionQueue;

    VkSemaphore image_available;
    VkSemaphore render_finished;
    VkFence in_flight;

    void record_command_buffer(VkCommandBuffer buffer, uint32_t image_index);
    FrameData &get_current_frame() {
        return _frames[_frameNumber % FRAME_OVERLAP];
    };

  public:
    Renderer(SDL_Window *window);
    void deinit();
    void draw_frame();
};
