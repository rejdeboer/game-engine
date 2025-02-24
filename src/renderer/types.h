#pragma once
#include "vk_mem_alloc.h"
#include <fmt/core.h>
#include <glm/glm.hpp>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.h>

#define GLM_ENABLE_EXPERIMENTAL

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

#define VK_CHECK(x)                                                            \
    do {                                                                       \
        VkResult err = x;                                                      \
        if (err) {                                                             \
            fmt::print("encountered vulkan error: {}", string_VkResult(err));  \
            abort();                                                           \
        }                                                                      \
    } while (0)
