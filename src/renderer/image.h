#pragma once
#include <vulkan/vulkan.h>

namespace vkutil {
VkImageSubresourceRange image_subresource_range(VkImageAspectFlags aspectMask);

void transition_image(VkCommandBuffer cmd, VkImage image,
                      VkImageLayout currentLayout, VkImageLayout newLayout);

void copy_image_to_image(VkCommandBuffer cmd, VkImage source,
                         VkImage destination, VkExtent2D srcSize,
                         VkExtent2D dstSize);

void generate_mipmaps(VkCommandBuffer cmd, VkImage image, VkExtent2D imageSize);
} // namespace vkutil
