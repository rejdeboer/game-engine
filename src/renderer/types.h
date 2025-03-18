#pragma once
#include <fmt/core.h>
#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.h>

#define GLM_ENABLE_EXPERIMENTAL

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

struct AllocatedImage {
    VkImage image;
    VkImageView imageView;
    VmaAllocation allocation;
    VkExtent3D extent;
    VkFormat format;
};

struct AllocatedBuffer {
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo info;
};

struct GPUMeshBuffers {
    AllocatedBuffer indexBuffer;
    AllocatedBuffer vertexBuffer;
    VkDeviceAddress vertexBufferAddress;
};

struct GPUDrawPushConstants {
    glm::mat4 worldMatrix;
    VkDeviceAddress vertexBuffer;
};

struct GPUSceneData {
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 viewproj;
    glm::vec4 ambientColor;
    glm::vec4 sunlightDirection; // w for sun power
    glm::vec4 sunlightColor;
};

enum class MaterialPass : uint8_t {
    MainColor,
    Transparent,
    Other,
};

struct MaterialPipeline {
    VkPipeline pipeline;
    VkPipelineLayout layout;
};

struct MaterialInstance {
    MaterialPipeline *pipeline;
    VkDescriptorSet materialSet;
    MaterialPass passType;
};

struct DrawContext;

struct RenderContext {
    VkCommandBuffer cmd;
    VkImageView drawImageView;
    VkImageView depthImageView;
    VkExtent2D drawExtent;
    VkDescriptorSet *globalDescriptorSet;
};

class IRenderable {
    virtual void Draw(const glm::mat4 &topMatrix, DrawContext &ctx) = 0;
};

struct Node : public IRenderable {
    std::weak_ptr<Node> parent;
    std::vector<std::shared_ptr<Node>> children;

    glm::mat4 localTransform;
    glm::mat4 worldTransform;

    void refresh_transform(const glm::mat4 &parentMatrix) {
        worldTransform = parentMatrix * localTransform;
        for (auto c : children) {
            c->refresh_transform(worldTransform);
        }
    }

    virtual void Draw(const glm::mat4 &topMatrix, DrawContext &ctx) {
        for (auto &c : children) {
            c->Draw(topMatrix, ctx);
        }
    }
};

#define VK_CHECK(x)                                                            \
    do {                                                                       \
        VkResult err = x;                                                      \
        if (err) {                                                             \
            fmt::print("encountered vulkan error: {}", string_VkResult(err));  \
            abort();                                                           \
        }                                                                      \
    } while (0)
