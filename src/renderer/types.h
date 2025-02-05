#pragma once
#include "vk_mem_alloc.h"
#include <fmt/core.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.h>

struct AllocatedImage {
    VkImage image;
    VkImageView imageView;
    VmaAllocation allocation;
    VkExtent3D extent;
    VkFormat format;
};

#define VK_CHECK(x)                                                            \
    do {                                                                       \
        VkResult err = x;                                                      \
        if (err) {                                                             \
            fmt::print("encountered vulkan error: {}", string_VkResult(err));  \
            abort();                                                           \
        }                                                                      \
    } while (0)
