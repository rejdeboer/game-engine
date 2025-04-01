#pragma once
#include "../camera.h"
#include "descriptor.h"
#include "init.h"
#include "loader.h"
#include "pipelines/depth_pass.h"
#include "pipelines/mesh.h"
#include "pipelines/tile.h"
#include "types.h"
#include "vertex.h"
#include <SDL3/SDL.h>
#include <deque>
#include <functional>
#include <vector>
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

struct RenderStats {
    int triangleCount;
    int drawcallCount;
    Uint64 meshDrawTime;
};

struct ShadowMapResources {
    AllocatedImage image;
    VkDescriptorSetLayout layout;
    VkDescriptorSet descriptor;
    VkSampler sampler;
    uint32_t resolution = 2048;
};

struct MeshNode : public Node {
    std::shared_ptr<MeshAsset> mesh;
};

class Renderer {
  private:
    SDL_Window *_window;
    VkInstance _instance;
    VkPhysicalDevice _physicalDevice;
    VkSurfaceKHR _surface;
    FrameData _frames[FRAME_OVERLAP];
    uint32_t _frameNumber;
    VkQueue _graphicsQueue;
    VkQueue _presentationQueue;
    DeletionQueue _mainDeletionQueue;
    RenderStats _stats;

    VkSwapchainKHR _swapchain;
    VkExtent2D _swapchainExtent;
    VkSurfaceFormatKHR _swapchainImageFormat;
    std::vector<VkImage> _swapchainImages;
    std::vector<VkImageView> _swapchainImageViews;

    VkFence _immFence;
    VkCommandBuffer _immCommandBuffer;
    VkCommandPool _immCommandPool;

    DescriptorAllocatorGrowable _globalDescriptorAllocator;
    VkDescriptorSet _drawImageDescriptors;
    VkDescriptorSetLayout _drawImageDescriptorLayout;

    TilePipeline _tilePipeline;
    DepthPassPipeline _depthPassPipeline;

    std::vector<TileDrawCommand> _tileDrawCommands;
    std::vector<MeshDrawCommand> _drawCommands;

    ShadowMapResources _shadowMap;

    GPUSceneData sceneData;

    VkExtent2D _drawExtent;

    bool _resizeRequested;

    glm::mat4 _cameraViewMatrix;

    void prepare_imgui(Uint64 dt);
    void draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView);
    void update_scene();
    FrameData &get_current_frame() {
        return _frames[_frameNumber % FRAME_OVERLAP];
    };

    void init_default_data();
    void init_swapchain();
    void init_pipelines();
    void init_commands(uint32_t queueFamilyIndex);
    void init_sync_structures();
    void init_descriptors();
    void init_shadow_map();
    void init_imgui();
    void destroy_swapchain();

  public:
    static Renderer &Get();

    VkDevice _device;
    VmaAllocator _allocator;

    AllocatedImage _drawImage;
    AllocatedImage _depthImage;
    VkDescriptorSetLayout _gpuSceneDataDescriptorLayout;

    AllocatedImage _whiteImage;
    AllocatedImage _blackImage;
    AllocatedImage _greyImage;
    AllocatedImage _errorCheckerboardImage;

    VkSampler _defaultSamplerLinear;
    VkSampler _defaultSamplerNearest;

    MeshPipeline _meshPipeline;

    void init(SDL_Window *window);
    void deinit();
    VkCommandBuffer begin_frame();
    void draw_world(VkCommandBuffer cmd);
    void draw(VkCommandBuffer cmd);
    void end_frame(VkCommandBuffer cmd, Uint64 dt);

    void immediate_submit(std::function<void(VkCommandBuffer cmd)> &&function);

    GPUMeshBuffers uploadMesh(std::span<uint32_t> indices,
                              std::span<Vertex> vertices);

    void resize_swapchain();
    void set_camera_view(glm::mat4 cameraViewMatrix);
    void set_camera_projection(glm::mat4 cameraProjectionMatrix);

    void write_draw_command(MeshDrawCommand &&cmd);
    void update_tile_draw_commands(std::vector<TileRenderingInput> inputs);

    AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage,
                                  VmaMemoryUsage memoryUsage);
    AllocatedImage create_image(VkExtent3D size, VkFormat format,
                                VkImageUsageFlags usage,
                                bool mipmapped = false);
    AllocatedImage create_image(void *data, VkExtent3D size, VkFormat format,
                                VkImageUsageFlags usage,
                                bool mipmapped = false);

    void destroy_buffer(const AllocatedBuffer &buffer);
    void destroy_image(const AllocatedImage &img);
};
