#pragma once
#include "../camera.h"
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

struct GLTFMetallic_Roughness {
    MaterialPipeline opaquePipeline;
    MaterialPipeline transparentPipeline;

    VkDescriptorSetLayout materialLayout;

    struct MaterialConstants {
        glm::vec4 colorFactors;
        glm::vec4 metalRoughFactors;
        // padding, we need it for uniform buffers
        glm::vec4 extra[14];
    };

    struct MaterialResources {
        AllocatedImage colorImage;
        VkSampler colorSampler;
        AllocatedImage metalRoughImage;
        VkSampler metalRoughSampler;
        VkBuffer dataBuffer;
        uint32_t dataBufferOffset;
    };

    DescriptorWriter writer;

    void build_pipelines(Renderer *renderer);
    void clear_resources(VkDevice device);

    MaterialInstance
    write_material(VkDevice device, MaterialPass pass,
                   const MaterialResources &resources,
                   DescriptorAllocatorGrowable &descriptorAllocator);
};

struct DrawContext {
    std::vector<RenderObject> opaqueSurfaces;
};

struct MeshNode : public Node {
    std::shared_ptr<MeshAsset> mesh;

    virtual void Draw(const glm::mat4 &topMatrix, DrawContext &ctx) override;
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

    VkSwapchainKHR _swapChain;
    VkExtent2D _swapChainExtent;
    VkSurfaceFormatKHR _swapChainImageFormat;
    std::vector<VkImage> _swapChainImages;
    std::vector<VkImageView> _swapChainImageViews;

    VkFence _immFence;
    VkCommandBuffer _immCommandBuffer;
    VkCommandPool _immCommandPool;

    DescriptorAllocatorGrowable _globalDescriptorAllocator;
    VkDescriptorSet _drawImageDescriptors;
    VkDescriptorSetLayout _drawImageDescriptorLayout;
    VkDescriptorSetLayout _singleImageDescriptorLayout;

    GPUSceneData sceneData;

    VkExtent2D _drawExtent;

    VmaAllocator _allocator;

    bool _resizeRequested;

    glm::mat4 _cameraViewMatrix;

    std::vector<std::shared_ptr<MeshAsset>> testMeshes;
    MaterialInstance defaultData;
    DrawContext mainDrawContext;
    std::unordered_map<std::string, std::shared_ptr<Node>> loadedNodes;
    std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> loadedScenes;

    void record_command_buffer(VkCommandBuffer buffer, uint32_t image_index);
    void update_scene();
    FrameData &get_current_frame() {
        return _frames[_frameNumber % FRAME_OVERLAP];
    };

    void immediate_submit(std::function<void(VkCommandBuffer cmd)> &&function);

    void destroy_buffer(const AllocatedBuffer &buffer);
    void destroy_image(const AllocatedImage &img);

    void init_default_data();
    void init_swap_chain();
    void init_pipelines();
    void init_commands(uint32_t queueFamilyIndex);
    void init_sync_structures();
    void init_descriptors();
    void destroy_swap_chain();

  public:
    VkDevice _device;
    AllocatedImage _drawImage;
    AllocatedImage _depthImage;
    VkDescriptorSetLayout _gpuSceneDataDescriptorLayout;

    AllocatedImage _whiteImage;
    AllocatedImage _blackImage;
    AllocatedImage _greyImage;
    AllocatedImage _errorCheckerboardImage;

    VkSampler _defaultSamplerLinear;
    VkSampler _defaultSamplerNearest;

    GLTFMetallic_Roughness metalRoughMaterial;

    Renderer(SDL_Window *window);
    void deinit();
    void draw_frame();

    GPUMeshBuffers uploadMesh(std::span<uint32_t> indices,
                              std::span<Vertex> vertices);

    void resize_swap_chain();
    void set_camera_view(glm::mat4 cameraViewMatrix);

    AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage,
                                  VmaMemoryUsage memoryUsage);
    AllocatedImage create_image(VkExtent3D size, VkFormat format,
                                VkImageUsageFlags usage,
                                bool mipmapped = false);
    AllocatedImage create_image(void *data, VkExtent3D size, VkFormat format,
                                VkImageUsageFlags usage,
                                bool mipmapped = false);
};
