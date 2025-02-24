#pragma once
#include "descriptor.h"
#include "init.h"
#include "loader.h"
#include "types.h"
#include "vertex.h"
#include <SDL3/SDL.h>
#include <deque>
#include <functional>
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

    DescriptorAllocatorGrowable _frameDescriptors;
    DeletionQueue _deletionQueue;

    VkCommandPool _commandPool;
    VkCommandBuffer _mainCommandBuffer;
};

class Renderer {
  private:
    SDL_Window *_window;
    VkInstance _instance;
    VkPhysicalDevice _physicalDevice;
    VkDevice _device;
    VkSurfaceKHR _surface;
    FrameData _frames[FRAME_OVERLAP];
    uint32_t _frameNumber;
    VkQueue _graphicsQueue;
    VkQueue _presentationQueue;
    DeletionQueue _mainDeletionQueue;

    VkSwapchainKHR _swapChain;
    VkExtent2D _swapChainExtent;
    VkSurfaceFormatKHR _swapChainImageFormat;
    std::vector<VkImage> _swapChainImages;
    std::vector<VkImageView> _swapChainImageViews;

    VkPipelineLayout _meshPipelineLayout;
    VkPipeline _meshPipeline;

    VkFence _immFence;
    VkCommandBuffer _immCommandBuffer;
    VkCommandPool _immCommandPool;

    DescriptorAllocator _globalDescriptorAllocator;
    VkDescriptorSet _drawImageDescriptors;
    VkDescriptorSetLayout _drawImageDescriptorLayout;
    VkDescriptorSetLayout _gpuSceneDataDescriptorLayout;

    GPUSceneData sceneData;

    AllocatedImage _drawImage;
    AllocatedImage _depthImage;
    VkExtent2D _drawExtent;

    VmaAllocator _allocator;

    bool _resizeRequested;

    std::vector<std::shared_ptr<MeshAsset>> testMeshes;

    void record_command_buffer(VkCommandBuffer buffer, uint32_t image_index);
    FrameData &get_current_frame() {
        return _frames[_frameNumber % FRAME_OVERLAP];
    };

    void immediate_submit(std::function<void(VkCommandBuffer cmd)> &&function);

    AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage,
                                  VmaMemoryUsage memoryUsage);
    void destroy_buffer(const AllocatedBuffer &buffer);

    void init_default_data();
    void init_swap_chain();
    void init_pipelines();
    void init_mesh_pipeline();
    void init_commands(uint32_t queueFamilyIndex);
    void init_sync_structures();
    void init_descriptors();
    void destroy_swap_chain();

  public:
    Renderer(SDL_Window *window);
    void deinit();
    void draw_frame();

    GPUMeshBuffers uploadMesh(std::span<uint32_t> indices,
                              std::span<Vertex> vertices);

    void resize_swap_chain();
};
