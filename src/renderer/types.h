#pragma once
#include "../math/aabb.h"
#include "vk_mem_alloc.h"
#include <fmt/core.h>
#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.h>

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
    glm::mat4 lightViewproj;
    glm::vec4 ambientColor;
    glm::vec4 sunlightDirection; // w for sun power
    glm::vec4 sunlightColor;
};

struct Bounds {
    glm::vec3 origin;
    float sphereRadius;
    glm::vec3 extents;

    math::AABB get_aabb() const {
        return math::AABB{
            .min = origin - extents,
            .max = origin + extents,
        };
    }
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
    VkExtent2D drawExtent;
    VkDescriptorSet globalDescriptorSet;
    VkDescriptorSet shadowMapSet;
    glm::mat4 viewproj;
};

struct MeshDrawCommand {
    uint32_t indexCount;
    uint32_t firstIndex;
    VkBuffer indexBuffer;

    MaterialInstance *material;
    Bounds bounds;

    glm::mat4 transform;
    VkDeviceAddress vertexBufferAddress;

    bool isOutlined = false;
};

#define VK_CHECK(x)                                                            \
    do {                                                                       \
        VkResult err = x;                                                      \
        if (err) {                                                             \
            fmt::print("encountered vulkan error: {}", string_VkResult(err));  \
            abort();                                                           \
        }                                                                      \
    } while (0)
