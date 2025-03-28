#include "renderer.hpp"
#include "SDL3/SDL_vulkan.h"
#include "frustum_culling.h"
#include "image.h"
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_vulkan.h"
#include "pipeline.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <glm/gtx/transform.hpp>
#include <span>
#include <vulkan/vulkan_core.h>

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

Renderer *loadedRenderer = nullptr;
Renderer &Renderer::Get() { return *loadedRenderer; }

void Renderer::init(SDL_Window *window) {
    _window = window;
    _resizeRequested = false;
    _instance = create_vulkan_instance(window);
    _surface = create_surface(window, _instance);
    _physicalDevice = pick_physical_device(_instance, _surface);

    QueueFamilyIndices queueFamilyIndices =
        find_compatible_queue_family_indices(_physicalDevice, _surface);
    assert(queueFamilyIndices.is_complete());
    uint32_t graphicsIndex = queueFamilyIndices.graphics_family.value();
    uint32_t presentationIndex = queueFamilyIndices.present_family.value();

    _device = create_device(_physicalDevice, graphicsIndex, presentationIndex);
    _allocator = create_allocator(_instance, _physicalDevice, _device);

    init_swapchain();
    _drawImage = create_draw_image(_device, _allocator, _swapchainExtent);
    _depthImage = create_depth_image(_device, _allocator, _swapchainExtent);

    _mainDeletionQueue.push_function([&]() {
        vkDestroyImageView(_device, _drawImage.imageView, nullptr);
        vmaDestroyImage(_allocator, _drawImage.image, _drawImage.allocation);
        vkDestroyImageView(_device, _depthImage.imageView, nullptr);
        vmaDestroyImage(_allocator, _depthImage.image, _depthImage.allocation);
    });

    init_descriptors();
    init_pipelines();

    _graphicsQueue = get_device_queue(_device, graphicsIndex, 0);
    _presentationQueue = get_device_queue(_device, presentationIndex, 0);

    init_imgui();
    init_commands(graphicsIndex);
    init_sync_structures();
    _tileRenderer.init(this);
    init_shadow_map();
    init_default_data();

    _drawCommands.reserve(1024);
}

void Renderer::init_default_data() {

    uint32_t white = glm::packUnorm4x8(glm::vec4(1, 1, 1, 1));
    _whiteImage =
        create_image((void *)&white, VkExtent3D{1, 1, 1},
                     VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

    uint32_t grey = glm::packUnorm4x8(glm::vec4(0.66f, 0.66f, 0.66f, 1));
    _greyImage =
        create_image((void *)&grey, VkExtent3D{1, 1, 1},
                     VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

    uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 0));
    _blackImage =
        create_image((void *)&black, VkExtent3D{1, 1, 1},
                     VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

    uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
    std::array<uint32_t, 16 * 16> pixels;
    for (int x = 0; x < 16; x++) {
        for (int y = 0; y < 16; y++) {
            pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
        }
    }
    _errorCheckerboardImage =
        create_image(&pixels[0], VkExtent3D{16, 16, 1},
                     VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

    VkSamplerCreateInfo samplerCreateInfo = {};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
    samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
    vkCreateSampler(_device, &samplerCreateInfo, nullptr,
                    &_defaultSamplerNearest);
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    vkCreateSampler(_device, &samplerCreateInfo, nullptr,
                    &_defaultSamplerLinear);

    _mainDeletionQueue.push_function([&]() {
        vkDestroySampler(_device, _defaultSamplerNearest, nullptr);
        vkDestroySampler(_device, _defaultSamplerLinear, nullptr);

        destroy_image(_whiteImage);
        destroy_image(_blackImage);
        destroy_image(_greyImage);
        destroy_image(_errorCheckerboardImage);
    });
}

void Renderer::init_commands(uint32_t queueFamilyIndex) {
    for (uint32_t i = 0; i < FRAME_OVERLAP; i++) {
        _frames[i]._commandPool =
            create_command_pool(_device, queueFamilyIndex);
        _frames[i]._mainCommandBuffer =
            create_command_buffer(_device, _frames[i]._commandPool);
        _mainDeletionQueue.push_function([&]() {
            vkDestroyCommandPool(_device, _frames[i]._commandPool, nullptr);
        });
    }

    _immCommandPool = create_command_pool(_device, queueFamilyIndex);
    _immCommandBuffer = create_command_buffer(_device, _immCommandPool);

    _mainDeletionQueue.push_function([=, this]() {
        vkDestroyCommandPool(_device, _immCommandPool, nullptr);
    });
}

void Renderer::init_swapchain() {
    SwapChainSupportDetails swapchainSupportDetails =
        query_swap_chain_support(_physicalDevice, _surface);
    _swapchainExtent =
        choose_swap_extent(_window, swapchainSupportDetails.capabilities);
    _swapchainImageFormat =
        choose_swap_surface_format(swapchainSupportDetails.formats);

    _swapchain = create_swap_chain(_window, swapchainSupportDetails, _device,
                                   _surface, _swapchainExtent,
                                   _swapchainImageFormat, _physicalDevice);
    _swapchainImages = get_swap_chain_images(_device, _swapchain);
    _swapchainImageViews = create_image_views(_device, _swapchainImages,
                                              _swapchainImageFormat.format);
}

void Renderer::init_pipelines() {
    init_depth_pass_pipeline();
    metalRoughMaterial.build_pipelines(this);
}

void Renderer::init_depth_pass_pipeline() {
    VkShaderModule shadowFragShader;
    if (!vkutil::load_shader_module("shaders/spv/shadow.frag.spv", _device,
                                    &shadowFragShader)) {
        fmt::println("Error when building the shadow fragment shader module");
    }

    VkShaderModule shadowVertShader;
    if (!vkutil::load_shader_module("shaders/spv/shadow.vert.spv", _device,
                                    &shadowVertShader)) {
        fmt::println("Error when building the shadow vertex shader module");
    }

    DescriptorLayoutBuilder layoutBuilder;
    layoutBuilder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    layoutBuilder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    _shadowMap.layout = layoutBuilder.build(
        _device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    VkDescriptorSetLayout layouts[] = {_gpuSceneDataDescriptorLayout,
                                       _shadowMap.layout};

    VkPipelineLayoutCreateInfo meshLayoutInfo = {};
    meshLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    meshLayoutInfo.setLayoutCount = 2;
    meshLayoutInfo.pSetLayouts = layouts;
    meshLayoutInfo.pushConstantRangeCount = 0;

    VK_CHECK(vkCreatePipelineLayout(_device, &meshLayoutInfo, nullptr,
                                    &_depthPassPipeline.layout));

    PipelineBuilder pipelineBuilder;
    pipelineBuilder.set_shaders(shadowVertShader, shadowFragShader);
    pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
    pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    pipelineBuilder.set_multisampling_none();
    pipelineBuilder.disable_blending();
    pipelineBuilder.disable_color_attachment_write();
    pipelineBuilder.enable_depthtest(true, VK_COMPARE_OP_LESS_OR_EQUAL);
    pipelineBuilder.set_depth_format(_depthImage.format);
    pipelineBuilder.enable_depth_bias(4.0f, 1.5f, 0.0f);
    pipelineBuilder._pipelineLayout = _depthPassPipeline.layout;
    _depthPassPipeline.pipeline = pipelineBuilder.build_pipeline(_device);

    vkDestroyShaderModule(_device, shadowVertShader, nullptr);
    vkDestroyShaderModule(_device, shadowFragShader, nullptr);
}

void Renderer::init_descriptors() {
    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes = {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}};

    _globalDescriptorAllocator.init(_device, 10, sizes);

    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        _drawImageDescriptorLayout =
            builder.build(_device, VK_SHADER_STAGE_COMPUTE_BIT);
    }

    _drawImageDescriptors = _globalDescriptorAllocator.allocate(
        _device, _drawImageDescriptorLayout);

    DescriptorWriter writer;
    writer.write_image(0, _drawImage.imageView, VK_NULL_HANDLE,
                       VK_IMAGE_LAYOUT_GENERAL,
                       VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    writer.update_set(_device, _drawImageDescriptors);

    for (int i = 0; i < FRAME_OVERLAP; i++) {
        std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> frameSizes = {
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4},
        };

        _frames[i]._frameDescriptors = DescriptorAllocatorGrowable{};
        _frames[i]._frameDescriptors.init(_device, 1000, frameSizes);

        _mainDeletionQueue.push_function(
            [&]() { _frames[i]._frameDescriptors.destroy_pools(_device); });
    }

    // TODO: Having separate descriptor layouts for samplers and images is
    // more performant as there is less duplicate data
    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        _gpuSceneDataDescriptorLayout = builder.build(
            _device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    }

    _mainDeletionQueue.push_function([&]() {
        _globalDescriptorAllocator.destroy_pools(_device);
        vkDestroyDescriptorSetLayout(_device, _drawImageDescriptorLayout,
                                     nullptr);
        vkDestroyDescriptorSetLayout(_device, _gpuSceneDataDescriptorLayout,
                                     nullptr);
    });
}

void Renderer::init_shadow_map() {
    VkExtent3D shadowMapExtent = {_shadowMap.resolution, _shadowMap.resolution,
                                  1};

    _shadowMap.image =
        create_image(shadowMapExtent, VK_FORMAT_D32_SFLOAT,
                     VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                         VK_IMAGE_USAGE_SAMPLED_BIT,
                     false);

    VkSamplerCreateInfo samplerInfo = {};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.maxAnisotropy = 1.0f;
    // Enable comparison for PCF filtering
    samplerInfo.compareEnable = VK_TRUE;
    samplerInfo.compareOp = VK_COMPARE_OP_LESS;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    VK_CHECK(
        vkCreateSampler(_device, &samplerInfo, nullptr, &_shadowMap.sampler));

    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        _shadowMap.layout =
            builder.build(_device, VK_SHADER_STAGE_FRAGMENT_BIT);
    }

    _mainDeletionQueue.push_function([=, this]() {
        vkDestroySampler(_device, _shadowMap.sampler, nullptr);
        destroy_image(_shadowMap.image);
    });

    // TODO: Is this necessary?
    immediate_submit([&](VkCommandBuffer cmd) {
        vkutil::transition_image(cmd, _shadowMap.image.image,
                                 VK_IMAGE_LAYOUT_UNDEFINED,
                                 VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
    });
}

void Renderer::init_sync_structures() {
    VkSemaphoreCreateInfo semaphore_create_info = {};
    semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fence_create_info = {};
    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (uint32_t i = 0; i < FRAME_OVERLAP; i++) {
        VK_CHECK(vkCreateFence(_device, &fence_create_info, nullptr,
                               &_frames[i]._renderFence));
        VK_CHECK(vkCreateSemaphore(_device, &semaphore_create_info, nullptr,
                                   &_frames[i]._renderSemaphore));
        VK_CHECK(vkCreateSemaphore(_device, &semaphore_create_info, nullptr,
                                   &_frames[i]._swapchainSemaphore));
        _mainDeletionQueue.push_function([&]() {
            vkDestroySemaphore(_device, _frames[i]._renderSemaphore, nullptr);
            vkDestroySemaphore(_device, _frames[i]._swapchainSemaphore,
                               nullptr);
            vkDestroyFence(_device, _frames[i]._renderFence, nullptr);
        });
    }

    VK_CHECK(vkCreateFence(_device, &fence_create_info, nullptr, &_immFence));
    _mainDeletionQueue.push_function(
        [=, this]() { vkDestroyFence(_device, _immFence, nullptr); });
}

void Renderer::init_imgui() {
    //  the size of the pool is very oversize, but it's copied from imgui demo
    //  itself.
    VkDescriptorPoolSize pool_sizes[] = {
        {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    VkDescriptorPool imguiPool;
    VK_CHECK(vkCreateDescriptorPool(_device, &pool_info, nullptr, &imguiPool));

    ImGui::CreateContext();
    ImGui_ImplSDL3_InitForVulkan(_window);

    ImGui_ImplVulkan_InitInfo initInfo = {};
    initInfo.Instance = _instance;
    initInfo.PhysicalDevice = _physicalDevice;
    initInfo.Device = _device;
    initInfo.Queue = _graphicsQueue;
    initInfo.DescriptorPool = imguiPool;
    initInfo.MinImageCount = 3;
    initInfo.ImageCount = 3;
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.UseDynamicRendering = true;
    initInfo.PipelineRenderingCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    initInfo.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    initInfo.PipelineRenderingCreateInfo.pColorAttachmentFormats =
        &_swapchainImageFormat.format;
    ImGui_ImplVulkan_Init(&initInfo);
    ImGui_ImplVulkan_CreateFontsTexture();

    _mainDeletionQueue.push_function([=, this]() {
        ImGui_ImplVulkan_Shutdown();
        vkDestroyDescriptorPool(_device, imguiPool, nullptr);
    });
}

void Renderer::deinit() {
    vkDeviceWaitIdle(_device);
    _tileRenderer.deinit();
    for (auto &frame : _frames) {
        frame._deletionQueue.flush();
    }
    _mainDeletionQueue.flush();
    destroy_swapchain();
    vmaDestroyAllocator(_allocator);
    vkDestroyDevice(_device, nullptr);
    SDL_Vulkan_DestroySurface(_instance, _surface, nullptr);
    vkDestroyInstance(_instance, nullptr);
    SDL_Vulkan_UnloadLibrary();
}

void Renderer::resize_swapchain() {
    vkDeviceWaitIdle(_device);
    destroy_swapchain();
    init_swapchain();
    _resizeRequested = false;
}

void Renderer::set_camera_view(glm::mat4 cameraViewMatrix) {
    _cameraViewMatrix = cameraViewMatrix;
}

void Renderer::update_tile_draw_commands(
    std::vector<TileRenderingInput> inputs) {
    for (auto cmd : _tileDrawCommands) {
        destroy_buffer(cmd.instanceBuffer);
    }

    _tileDrawCommands.clear();
    _tileDrawCommands.resize(inputs.size());

    for (int i = 0; i < inputs.size(); i++) {
        TileDrawCommand cmd;
        cmd.transform = glm::translate(glm::mat4(1.f), inputs[i].chunkPosition);
        cmd.instanceCount = inputs[i].instances.size();

        size_t bufferSize = sizeof(TileInstance) * cmd.instanceCount;
        // TODO: This should probably be a GPU buffer
        cmd.instanceBuffer =
            create_buffer(bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                          VMA_MEMORY_USAGE_CPU_TO_GPU);

        VmaAllocationInfo allocInfo;
        vmaGetAllocationInfo(_allocator, cmd.instanceBuffer.allocation,
                             &allocInfo);
        void *data = allocInfo.pMappedData;
        memcpy(data, inputs[i].instances.data(), bufferSize);

        _tileDrawCommands[i] = cmd;
    }
}

void Renderer::draw(VkCommandBuffer cmd) {
    AllocatedBuffer gpuSceneDataBuffer =
        create_buffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                      VMA_MEMORY_USAGE_CPU_TO_GPU);

    get_current_frame()._deletionQueue.push_function(
        [=, this]() { destroy_buffer(gpuSceneDataBuffer); });

    GPUSceneData *sceneUniformData =
        (GPUSceneData *)gpuSceneDataBuffer.allocation->GetMappedData();
    *sceneUniformData = sceneData;

    VkDescriptorSet globalDescriptor =
        get_current_frame()._frameDescriptors.allocate(
            _device, _gpuSceneDataDescriptorLayout);

    DescriptorWriter writer;
    writer.write_buffer(0, gpuSceneDataBuffer.buffer, sizeof(GPUSceneData), 0,
                        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    writer.update_set(_device, globalDescriptor);

    RenderContext ctx = RenderContext{
        .cmd = cmd,
        .drawImageView = _drawImage.imageView,
        .depthImageView = _depthImage.imageView,
        .drawExtent = _drawExtent,
        .globalDescriptorSet = &globalDescriptor,
        .cameraViewproj = sceneData.viewproj,
    };

    _tileRenderer.draw(ctx, _tileDrawCommands);
    draw_objects(ctx);
}

void Renderer::draw_objects(RenderContext ctx) {
    Uint64 start = SDL_GetTicks();
    std::vector<uint32_t> objectIndices;
    objectIndices.reserve(_drawCommands.size());

    for (int i = 0; i < _drawCommands.size(); i++) {
        if (vkutil::is_visible(_drawCommands[i].transform,
                               _drawCommands[i].bounds, sceneData.viewproj)) {
            objectIndices.push_back(i);
        }
    }

    // sort the opaque surfaces by material and mesh
    std::sort(objectIndices.begin(), objectIndices.end(),
              [&](const auto &iA, const auto &iB) {
                  const DrawCommand &A = _drawCommands[iA];
                  const DrawCommand &B = _drawCommands[iB];
                  if (A.material == B.material) {
                      return A.indexBuffer < B.indexBuffer;
                  } else {
                      return A.material < B.material;
                  }
              });

    VkRenderingAttachmentInfo colorAttachment = {};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = _drawImage.imageView;
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingAttachmentInfo depthAttachment = {};
    depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.imageView = _depthImage.imageView;
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.clearValue.depthStencil.depth = 1.f;

    VkRenderingInfo renderInfo = {};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachments = &colorAttachment;
    renderInfo.pDepthAttachment = &depthAttachment;
    renderInfo.renderArea = VkRect2D{VkOffset2D{0, 0}, _drawExtent};
    renderInfo.layerCount = 1;

    vkCmdBeginRendering(ctx.cmd, &renderInfo);

    MaterialPipeline *lastPipeline = nullptr;
    MaterialInstance *lastMaterial = nullptr;
    VkBuffer lastIndexBuffer = VK_NULL_HANDLE;

    auto draw = [&](const DrawCommand &r) {
        if (r.material != lastMaterial) {
            lastMaterial = r.material;
            if (r.material->pipeline != lastPipeline) {
                lastPipeline = r.material->pipeline;
                vkCmdBindPipeline(ctx.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  r.material->pipeline->pipeline);
                vkCmdBindDescriptorSets(ctx.cmd,
                                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        r.material->pipeline->layout, 0, 1,
                                        ctx.globalDescriptorSet, 0, nullptr);

                VkViewport viewport = {};
                viewport.x = 0;
                viewport.y = 0;
                viewport.width = (float)_swapchainExtent.width;
                viewport.height = (float)_swapchainExtent.height;
                viewport.minDepth = 0.f;
                viewport.maxDepth = 1.f;
                vkCmdSetViewport(ctx.cmd, 0, 1, &viewport);

                VkRect2D scissor = {};
                scissor.offset.x = 0;
                scissor.offset.y = 0;
                scissor.extent.width = _swapchainExtent.width;
                scissor.extent.height = _swapchainExtent.height;
                vkCmdSetScissor(ctx.cmd, 0, 1, &scissor);
            }

            vkCmdBindDescriptorSets(ctx.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    r.material->pipeline->layout, 1, 1,
                                    &r.material->materialSet, 0, nullptr);
        }

        if (r.indexBuffer != lastIndexBuffer) {
            lastIndexBuffer = r.indexBuffer;
            vkCmdBindIndexBuffer(ctx.cmd, r.indexBuffer, 0,
                                 VK_INDEX_TYPE_UINT32);
        }

        GPUDrawPushConstants pushConstants;
        pushConstants.vertexBuffer = r.vertexBufferAddress;
        pushConstants.worldMatrix = r.transform;
        vkCmdPushConstants(ctx.cmd, r.material->pipeline->layout,
                           VK_SHADER_STAGE_VERTEX_BIT, 0,
                           sizeof(GPUDrawPushConstants), &pushConstants);

        _stats.drawcallCount++;
        _stats.triangleCount += r.indexCount / 3;
        vkCmdDrawIndexed(ctx.cmd, r.indexCount, 1, r.firstIndex, 0, 0);
    };

    _stats.drawcallCount = 0;
    _stats.triangleCount = 0;

    for (auto &r : objectIndices) {
        draw(_drawCommands[r]);
    }

    vkCmdEndRendering(ctx.cmd);
    _stats.meshDrawTime = SDL_GetTicks() - start;
}

VkCommandBuffer Renderer::begin_frame() {
    if (_resizeRequested) {
        resize_swapchain();
    }

    FrameData *currentFrame = &get_current_frame();
    VK_CHECK(vkWaitForFences(_device, 1, &currentFrame->_renderFence, VK_TRUE,
                             UINT64_MAX));
    currentFrame->_deletionQueue.flush();
    currentFrame->_frameDescriptors.clear_pools(_device);
    vkResetFences(_device, 1, &currentFrame->_renderFence);

    _drawExtent.width =
        std::min(_swapchainExtent.width, _drawImage.extent.width);
    _drawExtent.height =
        std::min(_swapchainExtent.height, _drawImage.extent.height);

    VkCommandBuffer cmd = currentFrame->_mainCommandBuffer;
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = 0;
    begin_info.pInheritanceInfo = nullptr;
    VK_CHECK(vkBeginCommandBuffer(cmd, &begin_info));

    vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_GENERAL);
    vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL,
                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    vkutil::transition_image(cmd, _depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    update_scene();
    _drawCommands.clear();
    return cmd;
}

void Renderer::end_frame(VkCommandBuffer cmd, Uint64 dt) {
    FrameData *currentFrame = &get_current_frame();
    uint32_t imageIndex;
    VkResult acquireResult = vkAcquireNextImageKHR(
        _device, _swapchain, UINT64_MAX, currentFrame->_swapchainSemaphore,
        VK_NULL_HANDLE, &imageIndex);
    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
        _resizeRequested = true;
        return;
    }

    vkutil::transition_image(cmd, _drawImage.image,
                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                             VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vkutil::transition_image(cmd, _swapchainImages[imageIndex],
                             VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    vkutil::copy_image_to_image(cmd, _drawImage.image,
                                _swapchainImages[imageIndex], _drawExtent,
                                _swapchainExtent);

    vkutil::transition_image(cmd, _swapchainImages[imageIndex],
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    prepare_imgui(dt);
    draw_imgui(cmd, _swapchainImageViews[imageIndex]);

    vkutil::transition_image(cmd, _swapchainImages[imageIndex],
                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                             VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmdSubmitInfo = {};
    cmdSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    cmdSubmitInfo.commandBuffer = currentFrame->_mainCommandBuffer;
    cmdSubmitInfo.deviceMask = 0;

    VkSemaphoreSubmitInfo waitInfo = create_semaphore_submit_info(
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
        currentFrame->_swapchainSemaphore);
    VkSemaphoreSubmitInfo signalInfo = create_semaphore_submit_info(
        VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, currentFrame->_renderSemaphore);

    VkSubmitInfo2 submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo.waitSemaphoreInfoCount = 1;
    submitInfo.pWaitSemaphoreInfos = &waitInfo;
    submitInfo.signalSemaphoreInfoCount = 1;
    submitInfo.pSignalSemaphoreInfos = &signalInfo;
    submitInfo.commandBufferInfoCount = 1;
    submitInfo.pCommandBufferInfos = &cmdSubmitInfo;

    VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submitInfo,
                            currentFrame->_renderFence));

    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &currentFrame->_renderSemaphore;
    VkSwapchainKHR swap_chains[] = {_swapchain};
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swap_chains;
    present_info.pImageIndices = &imageIndex;
    present_info.pResults = nullptr;

    VkResult presentResult =
        vkQueuePresentKHR(_presentationQueue, &present_info);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR) {
        _resizeRequested = true;
        return;
    }
    _frameNumber++;
}

void Renderer::update_scene() {
    float distance = 100.0f;            // Distance from the origin
    float angleX = glm::radians(30.0f); // Tilt angle
    float angleY = glm::radians(45.0f); // Rotation angle

    glm::vec3 cameraPosition = glm::vec3(
        distance * cos(angleY), distance * sin(angleX), distance * sin(angleY));
    glm::vec3 cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 upVector = glm::vec3(0.0f, 1.0f, 0.0f); // Y-axis is "up"

    // Orthographic projection parameters
    float aspectRatio =
        (float)_swapchainExtent.width / (float)_swapchainExtent.height;
    float zoom = 10.0f;
    float left = -aspectRatio * zoom; // Left boundary of the view
    float right = aspectRatio * zoom; // Right boundary of the view
    float bottom = -zoom;             // Bottom boundary of the view
    float top = zoom;                 // Top boundary of the view
    float nearPlane = 0.1f;           // Near clipping plane
    float farPlane = 1000.0f;         // Far clipping plane

    sceneData.proj = glm::ortho(left, right, bottom, top, nearPlane, farPlane);
    sceneData.view = glm::lookAt(cameraPosition, cameraTarget, upVector);

    sceneData.proj[1][1] *= -1; // invert Y direction
    sceneData.viewproj = sceneData.proj * sceneData.view;

    sceneData.ambientColor = glm::vec4(.1f);
    sceneData.sunlightColor = glm::vec4(1.f);
    sceneData.sunlightDirection = glm::vec4(0, 1, 0.5, 1.f);

    glm::vec3 lightPos =
        glm::vec3(0, 100, 0) - glm::vec3(sceneData.sunlightDirection) *
                                   150.0f; // Backtrack along direction
    glm::mat4 lightView =
        glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    // Define the light's orthographic projection matrix
    // The bounds [-orthoSize, orthoSize] define the area covered by shadows.
    // This needs to be large enough to cover the camera's view.
    // Consider techniques like Cascaded Shadow Maps (CSM) for large scenes.
    float orthoSize = 100.0f; // Adjust based on your scene scale
    glm::mat4 lightProj = glm::ortho(-orthoSize, orthoSize, -orthoSize,
                                     orthoSize, nearPlane, farPlane);
    lightProj[1][1] *= -1; // Vulkan Y-flip

    sceneData.lightViewproj = lightProj * lightView;
}

void Renderer::prepare_imgui(Uint64 dt) {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    ImGui::Begin("Stats");
    ImGui::Text("frametime %lu ms", dt);
    ImGui::Text("fps %f", 1000.f / (float)dt);
    ImGui::Text("drawtime %lu ms", _stats.meshDrawTime);
    ImGui::Text("triangles %i", _stats.triangleCount);
    ImGui::Text("draws %i", _stats.drawcallCount);
    ImGui::End();
    ImGui::Render();
}

void Renderer::draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView) {
    VkRenderingAttachmentInfo colorAttachment = create_color_attachment_info(
        targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingInfo renderInfo = {};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachments = &colorAttachment;
    renderInfo.renderArea = VkRect2D{VkOffset2D{0, 0}, _swapchainExtent};
    renderInfo.layerCount = 1;
    vkCmdBeginRendering(cmd, &renderInfo);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    vkCmdEndRendering(cmd);
}

void Renderer::immediate_submit(
    std::function<void(VkCommandBuffer cmd)> &&function) {
    VK_CHECK(vkResetFences(_device, 1, &_immFence));
    VK_CHECK(vkResetCommandBuffer(_immCommandBuffer, 0));

    VkCommandBuffer cmd = _immCommandBuffer;

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0;
    beginInfo.pInheritanceInfo = nullptr;

    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    function(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmdSubmitInfo = {};
    cmdSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    cmdSubmitInfo.commandBuffer = cmd;
    cmdSubmitInfo.deviceMask = 0;

    VkSubmitInfo2 submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo.commandBufferInfoCount = 1;
    submitInfo.pCommandBufferInfos = &cmdSubmitInfo;
    submitInfo.signalSemaphoreInfoCount = 0;
    submitInfo.waitSemaphoreInfoCount = 0;

    VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submitInfo, _immFence));

    VK_CHECK(vkWaitForFences(_device, 1, &_immFence, true, 9999999999));
}

void Renderer::write_draw_command(DrawCommand &&cmd) {
    _drawCommands.emplace_back(cmd);
}

GPUMeshBuffers Renderer::uploadMesh(std::span<uint32_t> indices,
                                    std::span<Vertex> vertices) {
    const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
    const size_t indexBufferSize = indices.size() * sizeof(uint32_t);

    GPUMeshBuffers newSurface;

    newSurface.vertexBuffer = create_buffer(
        vertexBufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);

    VkBufferDeviceAddressInfo deviceAdressInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = newSurface.vertexBuffer.buffer};
    newSurface.vertexBufferAddress =
        vkGetBufferDeviceAddress(_device, &deviceAdressInfo);

    newSurface.indexBuffer = create_buffer(indexBufferSize,
                                           VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                                               VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                           VMA_MEMORY_USAGE_GPU_ONLY);

    AllocatedBuffer staging = create_buffer(vertexBufferSize + indexBufferSize,
                                            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                            VMA_MEMORY_USAGE_CPU_ONLY);

    void *data = staging.allocation->GetMappedData();

    memcpy(data, vertices.data(), vertexBufferSize);
    memcpy((char *)data + vertexBufferSize, indices.data(), indexBufferSize);

    // TODO: Here we are waiting for the GPU command to fully execute before
    // continuing with CPU logic. It is probably better to use a separate thread
    // for uploading / creating / deleting the staging buffer
    immediate_submit([&](VkCommandBuffer cmd) {
        VkBufferCopy vertexCopy{0};
        vertexCopy.dstOffset = 0;
        vertexCopy.srcOffset = 0;
        vertexCopy.size = vertexBufferSize;

        vkCmdCopyBuffer(cmd, staging.buffer, newSurface.vertexBuffer.buffer, 1,
                        &vertexCopy);

        VkBufferCopy indexCopy{0};
        indexCopy.dstOffset = 0;
        indexCopy.srcOffset = vertexBufferSize;
        indexCopy.size = indexBufferSize;

        vkCmdCopyBuffer(cmd, staging.buffer, newSurface.indexBuffer.buffer, 1,
                        &indexCopy);
    });

    destroy_buffer(staging);

    return newSurface;
}

AllocatedBuffer Renderer::create_buffer(size_t allocSize,
                                        VkBufferUsageFlags usage,
                                        VmaMemoryUsage memoryUsage) {
    VkBufferCreateInfo bufferInfo = {.sType =
                                         VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.pNext = nullptr;
    bufferInfo.size = allocSize;
    bufferInfo.usage = usage;

    VmaAllocationCreateInfo vmaAllocInfo = {};
    vmaAllocInfo.usage = memoryUsage;
    vmaAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    AllocatedBuffer newBuffer;
    VK_CHECK(vmaCreateBuffer(_allocator, &bufferInfo, &vmaAllocInfo,
                             &newBuffer.buffer, &newBuffer.allocation,
                             &newBuffer.info));
    return newBuffer;
}

AllocatedImage Renderer::create_image(VkExtent3D size, VkFormat format,
                                      VkImageUsageFlags usage, bool mipmapped) {
    AllocatedImage newImage;
    newImage.format = format;
    newImage.extent = size;

    VkImageCreateInfo imageCreateInfo = {};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = format;
    imageCreateInfo.extent = size;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage = usage;
    imageCreateInfo.mipLevels =
        mipmapped ? static_cast<uint32_t>(std::floor(
                        std::log2(std::max(size.width, size.height)))) +
                        1
                  : 1;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocInfo.requiredFlags =
        VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK(vmaCreateImage(_allocator, &imageCreateInfo, &allocInfo,
                            &newImage.image, &newImage.allocation, nullptr));

    VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
    if (format == VK_FORMAT_D32_SFLOAT) {
        aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    VkImageViewCreateInfo imageViewCreateInfo = {};
    imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCreateInfo.image = newImage.image;
    imageViewCreateInfo.format = format;
    imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    imageViewCreateInfo.subresourceRange.levelCount = 1;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount = 1;
    imageViewCreateInfo.subresourceRange.aspectMask = aspectFlag;
    VK_CHECK(vkCreateImageView(_device, &imageViewCreateInfo, nullptr,
                               &newImage.imageView));

    return newImage;
}

AllocatedImage Renderer::create_image(void *data, VkExtent3D size,
                                      VkFormat format, VkImageUsageFlags usage,
                                      bool mipmapped) {
    size_t dataSize = size.depth * size.width * size.height * 4;
    AllocatedBuffer uploadBuffer =
        create_buffer(dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VMA_MEMORY_USAGE_CPU_TO_GPU);
    memcpy(uploadBuffer.info.pMappedData, data, dataSize);

    AllocatedImage newImage =
        create_image(size, format,
                     usage | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                         VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                     mipmapped);

    immediate_submit([&](VkCommandBuffer cmd) {
        vkutil::transition_image(cmd, newImage.image, VK_IMAGE_LAYOUT_UNDEFINED,
                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkBufferImageCopy copyRegion = {};
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0;
        copyRegion.bufferImageHeight = 0;

        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageExtent = size;

        vkCmdCopyBufferToImage(cmd, uploadBuffer.buffer, newImage.image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                               &copyRegion);

        if (mipmapped) {
            vkutil::generate_mipmaps(
                cmd, newImage.image,
                VkExtent2D{newImage.extent.width, newImage.extent.height});
        } else {
            vkutil::transition_image(cmd, newImage.image,
                                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
    });

    destroy_buffer(uploadBuffer);
    return newImage;
}

void Renderer::destroy_buffer(const AllocatedBuffer &buffer) {
    vmaDestroyBuffer(_allocator, buffer.buffer, buffer.allocation);
}

void Renderer::destroy_image(const AllocatedImage &img) {
    vkDestroyImageView(_device, img.imageView, nullptr);
    vmaDestroyImage(_allocator, img.image, img.allocation);
}

void Renderer::destroy_swapchain() {
    vkDestroySwapchainKHR(_device, _swapchain, nullptr);
    for (int i = 0; i < _swapchainImageViews.size(); i++) {
        vkDestroyImageView(_device, _swapchainImageViews[i], nullptr);
    }
}

void GLTFMetallic_Roughness::build_pipelines(Renderer *renderer) {
    VkShaderModule meshFragShader;
    if (!vkutil::load_shader_module("shaders/spv/mesh.frag.spv",
                                    renderer->_device, &meshFragShader)) {
        fmt::println("Error when building the mesh fragment shader module");
    }

    VkShaderModule meshVertShader;
    if (!vkutil::load_shader_module("shaders/spv/mesh.vert.spv",
                                    renderer->_device, &meshVertShader)) {
        fmt::println("Error when building the mesh vertex shader module");
    }

    VkPushConstantRange matrixRange{};
    matrixRange.offset = 0;
    matrixRange.size = sizeof(GPUDrawPushConstants);
    matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    DescriptorLayoutBuilder layoutBuilder;
    layoutBuilder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    layoutBuilder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    layoutBuilder.add_binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    materialLayout = layoutBuilder.build(renderer->_device,
                                         VK_SHADER_STAGE_VERTEX_BIT |
                                             VK_SHADER_STAGE_FRAGMENT_BIT);

    VkDescriptorSetLayout layouts[] = {renderer->_gpuSceneDataDescriptorLayout,
                                       materialLayout};

    VkPipelineLayoutCreateInfo meshLayoutInfo = {};
    meshLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    meshLayoutInfo.setLayoutCount = 2;
    meshLayoutInfo.pSetLayouts = layouts;
    meshLayoutInfo.pushConstantRangeCount = 1;
    meshLayoutInfo.pPushConstantRanges = &matrixRange;

    VkPipelineLayout newLayout;
    VK_CHECK(vkCreatePipelineLayout(renderer->_device, &meshLayoutInfo, nullptr,
                                    &newLayout));

    opaquePipeline.layout = newLayout;
    transparentPipeline.layout = newLayout;

    PipelineBuilder pipelineBuilder;
    pipelineBuilder.set_shaders(meshVertShader, meshFragShader);
    pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
    pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    pipelineBuilder.set_multisampling_none();
    pipelineBuilder.disable_blending();
    pipelineBuilder.enable_depthtest(true, VK_COMPARE_OP_LESS_OR_EQUAL);
    pipelineBuilder.set_color_attachment_format(renderer->_drawImage.format);
    pipelineBuilder.set_depth_format(renderer->_depthImage.format);
    pipelineBuilder._pipelineLayout = newLayout;
    opaquePipeline.pipeline = pipelineBuilder.build_pipeline(renderer->_device);

    pipelineBuilder.enable_blending_additive();
    pipelineBuilder.enable_depthtest(false, VK_COMPARE_OP_LESS_OR_EQUAL);
    transparentPipeline.pipeline =
        pipelineBuilder.build_pipeline(renderer->_device);

    vkDestroyShaderModule(renderer->_device, meshVertShader, nullptr);
    vkDestroyShaderModule(renderer->_device, meshFragShader, nullptr);
}

MaterialInstance GLTFMetallic_Roughness::write_material(
    VkDevice device, MaterialPass pass, const MaterialResources &resources,
    DescriptorAllocatorGrowable &descriptorAllocator) {

    MaterialInstance matData;
    matData.passType = pass;
    if (pass == MaterialPass::Transparent) {
        matData.pipeline = &transparentPipeline;
    } else {
        matData.pipeline = &opaquePipeline;
    }

    matData.materialSet = descriptorAllocator.allocate(device, materialLayout);

    writer.clear();
    writer.write_buffer(0, resources.dataBuffer, sizeof(MaterialConstants),
                        resources.dataBufferOffset,
                        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    writer.write_image(1, resources.colorImage.imageView,
                       resources.colorSampler,
                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                       VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    writer.write_image(2, resources.metalRoughImage.imageView,
                       resources.metalRoughSampler,
                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                       VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    writer.update_set(device, matData.materialSet);

    return matData;
}
