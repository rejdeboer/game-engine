#include "init.h"
#include "../file.h"
#include "vertex.h"
#include <algorithm>
#include <set>
#include <vector>
#include <vulkan/vulkan_core.h>

const std::vector<const char *> validation_layers = {
    "VK_LAYER_KHRONOS_validation"};

const std::vector<const char *> device_extensions = {
    VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
#if __APPLE__
    "VK_KHR_portability_subset",
#endif
};

#if defined(NDEBUG)
const bool enable_validation_layers = false;
#else
const bool enable_validation_layers = true;
#endif

static inline bool has_device_extension_support(VkPhysicalDevice device) {
    uint32_t extension_count;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count,
                                         nullptr);

    std::vector<VkExtensionProperties> available_extensions(extension_count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count,
                                         available_extensions.data());

    std::set<std::string> required_extensions(device_extensions.begin(),
                                              device_extensions.end());

    for (const auto &extension : available_extensions) {
        required_extensions.erase(extension.extensionName);
    }

    return required_extensions.empty();
}

static bool has_validation_layer_support() {
    uint32_t layer_count;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    std::vector<VkLayerProperties> available_layers(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

    for (const char *layer_name : validation_layers) {
        bool layer_found = false;

        for (const auto &layer_props : available_layers) {
            if (strcmp(layer_name, layer_props.layerName) == 0) {
                layer_found = true;
                break;
            }
        }

        if (!layer_found) {
            return false;
        }
    }

    return true;
}

static inline int rate_device_suitability(VkPhysicalDevice device) {
    int score = 0;

    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);
    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

    // Discrete GPUs have a significant performance advantage
    if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        score += 1000;
    }

    // Maximum possible size of textures affects graphics quality
    score += deviceProperties.limits.maxImageDimension2D;

    if (!deviceFeatures.geometryShader) {
        return 0;
    }

    return score;
}

SwapChainSupportDetails query_swap_chain_support(VkPhysicalDevice device,
                                                 VkSurfaceKHR surface) {
    SwapChainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface,
                                              &details.capabilities);

    uint32_t format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count,
                                         nullptr);
    if (format_count != 0) {
        details.formats.resize(format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count,
                                             &details.formats[0]);
    }

    uint32_t present_mode_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface,
                                              &present_mode_count, nullptr);
    if (present_mode_count != 0) {
        details.present_modes.resize(present_mode_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            device, surface, &present_mode_count, &details.present_modes[0]);
    }

    return details;
}

static bool is_suitable_physical_device(VkPhysicalDevice device,
                                        VkSurfaceKHR surface) {
    bool has_extension_support = has_device_extension_support(device);

    bool has_adequate_swap_chain = false;
    if (has_extension_support) {
        SwapChainSupportDetails swap_chain_support_details =
            query_swap_chain_support(device, surface);
        has_adequate_swap_chain =
            !swap_chain_support_details.formats.empty() &&
            !swap_chain_support_details.present_modes.empty();
    }

    return has_adequate_swap_chain &&
           find_compatible_queue_family_indices(device, surface)
               .is_complete() &&
           has_extension_support;
}

static VkShaderModule create_shader_module(VkDevice device,
                                           const std::vector<char> &code) {
    VkShaderModuleCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = code.size();
    create_info.pCode = reinterpret_cast<const uint32_t *>(code.data());

    VkShaderModule module;
    VK_CHECK(vkCreateShaderModule(device, &create_info, nullptr, &module));
    return module;
}

VkSurfaceFormatKHR choose_swap_surface_format(
    const std::vector<VkSurfaceFormatKHR> &available_formats) {
    for (const auto &format : available_formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }

    // TODO: Rank the remaining formats
    return available_formats[0];
}

VkPresentModeKHR
choose_swap_present_mode(const std::vector<VkPresentModeKHR> &available_modes) {
    for (const auto &mode : available_modes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return mode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D choose_swap_extent(SDL_Window *window,
                              const VkSurfaceCapabilitiesKHR &capabilities) {
    if (capabilities.currentExtent.width !=
        std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    int width, height;
    SDL_GetWindowSizeInPixels(window, &width, &height);
    VkExtent2D extent = {
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height),
    };
    extent.width = std::clamp(extent.width, capabilities.minImageExtent.width,
                              capabilities.maxImageExtent.width);
    extent.height =
        std::clamp(extent.height, capabilities.minImageExtent.height,
                   capabilities.maxImageExtent.height);
    return extent;
}

QueueFamilyIndices find_compatible_queue_family_indices(VkPhysicalDevice device,
                                                        VkSurfaceKHR surface) {
    QueueFamilyIndices res = {};

    uint32_t queue_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_count, nullptr);
    std::vector<VkQueueFamilyProperties> queue_families(queue_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_count,
                                             queue_families.data());

    uint32_t index = 0;
    for (const auto &queue_family : queue_families) {
        if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            res.graphics_family = index;
        }

        VkBool32 present_support = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, index, surface,
                                             &present_support);
        if (present_support) {
            res.present_family = index;
        }

        if (res.is_complete()) {
            break;
        }

        index++;
    }

    return res;
}

VkPhysicalDevice pick_physical_device(VkInstance instance,
                                      VkSurfaceKHR surface) {
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(instance, &device_count, nullptr);

    if (device_count == 0) {
        throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }

    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(instance, &device_count, devices.data());

    int max_score = -1;
    VkPhysicalDevice best_device;
    for (int i = 0; i < device_count; i++) {
        if (!is_suitable_physical_device(devices[i], surface)) {
            continue;
        }
        int score = rate_device_suitability(devices[i]);
        if (max_score < score) {
            max_score = score;
            best_device = devices[i];
        }
    }

    if (!best_device) {
        throw std::runtime_error("no suitable physical device found");
    }

    return best_device;
}

VkInstance create_vulkan_instance(SDL_Window *window) {
    if (enable_validation_layers && !has_validation_layer_support()) {
        printf("validation layers requested, but not available\n");
        throw std::runtime_error("could not create vulkan instance");
    }

    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "VulkanRenderer";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "No Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount =
        static_cast<uint32_t>(device_extensions.size());
    create_info.ppEnabledExtensionNames = &device_extensions[0];

    if (enable_validation_layers) {
        create_info.enabledLayerCount =
            static_cast<uint32_t>(validation_layers.size());
        create_info.ppEnabledLayerNames = validation_layers.data();
    } else {
        create_info.enabledLayerCount = 0;
    }

    uint32_t extension_count;
    const char *const *extensions =
        SDL_Vulkan_GetInstanceExtensions(&extension_count);

#ifdef __APPLE__
    create_info.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    create_info.enabledExtensionCount = extension_count;
    create_info.ppEnabledExtensionNames = extensions;

    VkInstance instance;
    VK_CHECK(vkCreateInstance(&create_info, nullptr, &instance));

    return instance;
}

VkSurfaceKHR create_surface(SDL_Window *window, VkInstance instance) {
    VkSurfaceKHR surface;
    if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface)) {
        fprintf(stderr, "error creating vulkan surface: %s\n", SDL_GetError());
        throw std::runtime_error("could not create vulkan surface");
    }
    return surface;
}

VkDevice create_device(VkPhysicalDevice physical_device,
                       uint32_t graphics_index, uint32_t presentation_index) {
    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    std::set<uint32_t> unique_queue_families = {graphics_index,
                                                presentation_index};

    for (uint32_t queue_family_index : unique_queue_families) {
        VkDeviceQueueCreateInfo queue_create_info = {};
        queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_info.queueFamilyIndex = queue_family_index;
        queue_create_info.queueCount = 1;
        float queue_priority = 1.0f;
        queue_create_info.pQueuePriorities = &queue_priority;
        queue_create_infos.push_back(queue_create_info);
    }

    VkPhysicalDeviceVulkan12Features physical_device_features_12 = {};
    physical_device_features_12.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    physical_device_features_12.bufferDeviceAddress = true;
    physical_device_features_12.descriptorIndexing = true;

    VkPhysicalDeviceVulkan13Features physical_device_features_13 = {};
    physical_device_features_13.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    physical_device_features_13.dynamicRendering = true;
    physical_device_features_13.synchronization2 = true;

    VkPhysicalDeviceFeatures2 physical_device_features = {};
    physical_device_features.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    physical_device_features.pNext = &physical_device_features_12;
    physical_device_features_12.pNext = &physical_device_features_13;
    vkGetPhysicalDeviceFeatures2(physical_device, &physical_device_features);

    VkDeviceCreateInfo device_create_info = {};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.pNext = &physical_device_features;
    device_create_info.queueCreateInfoCount =
        static_cast<uint32_t>(unique_queue_families.size());
    device_create_info.pQueueCreateInfos = &queue_create_infos[0];
    device_create_info.enabledExtensionCount =
        static_cast<uint32_t>(device_extensions.size());
    device_create_info.ppEnabledExtensionNames = &device_extensions[0];

    VkDevice device;
    VK_CHECK(
        vkCreateDevice(physical_device, &device_create_info, nullptr, &device));

    return device;
}

VkSwapchainKHR create_swap_chain(SDL_Window *window,
                                 SwapChainSupportDetails support_details,
                                 VkDevice device, VkSurfaceKHR surface,
                                 VkExtent2D extent,
                                 VkSurfaceFormatKHR surface_format,
                                 VkPhysicalDevice physicalDevice) {
    VkPresentModeKHR present_mode =
        choose_swap_present_mode(support_details.present_modes);

    uint32_t image_count = support_details.capabilities.minImageCount + 1;
    if (support_details.capabilities.maxImageCount > 0 &&
        support_details.capabilities.maxImageCount < image_count) {
        image_count = support_details.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    create_info.preTransform = support_details.capabilities.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;

    QueueFamilyIndices queueFamilyIndices =
        find_compatible_queue_family_indices(physicalDevice, surface);
    assert(queueFamilyIndices.is_complete());
    uint32_t graphics_index = queueFamilyIndices.graphics_family.value();
    uint32_t presentation_index = queueFamilyIndices.present_family.value();

    if (graphics_index != presentation_index) {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        uint32_t queue_family_indices[] = {graphics_index, presentation_index};
        create_info.pQueueFamilyIndices = queue_family_indices;
    } else {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        create_info.queueFamilyIndexCount = 0;
        create_info.pQueueFamilyIndices = nullptr;
    }

    VkSwapchainKHR swap_chain;
    VK_CHECK(vkCreateSwapchainKHR(device, &create_info, nullptr, &swap_chain));
    return swap_chain;
}

std::vector<VkImage> get_swap_chain_images(VkDevice device,
                                           VkSwapchainKHR swap_chain) {
    uint32_t image_count;
    vkGetSwapchainImagesKHR(device, swap_chain, &image_count, nullptr);
    std::vector<VkImage> images(image_count);
    vkGetSwapchainImagesKHR(device, swap_chain, &image_count, &images[0]);
    return images;
}

std::vector<VkImageView> create_image_views(VkDevice device,
                                            std::vector<VkImage> images,
                                            VkFormat image_format) {
    std::vector<VkImageView> image_views(images.size());
    for (size_t i = 0; i < images.size(); i++) {
        VkImageViewCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        create_info.image = images[i];
        create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        create_info.format = image_format;
        create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        create_info.subresourceRange.baseMipLevel = 0;
        create_info.subresourceRange.levelCount = 1;
        create_info.subresourceRange.baseArrayLayer = 0;
        create_info.subresourceRange.layerCount = 1;

        VK_CHECK(
            vkCreateImageView(device, &create_info, nullptr, &image_views[i]));
    }

    return image_views;
}

AllocatedImage create_draw_image(VkDevice device, VmaAllocator allocator,
                                 VkExtent2D swapChainExtent) {
    VkExtent3D drawImageExtent = {swapChainExtent.width, swapChainExtent.height,
                                  1};

    AllocatedImage allocatedImage;
    allocatedImage.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    allocatedImage.extent = drawImageExtent;

    VkImageUsageFlags drawImageUsages{};
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VkImageCreateInfo imageCreateInfo = {};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = allocatedImage.format;
    imageCreateInfo.extent = allocatedImage.extent;
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage = drawImageUsages;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocInfo.requiredFlags =
        VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VK_CHECK(vmaCreateImage(allocator, &imageCreateInfo, &allocInfo,
                            &allocatedImage.image, &allocatedImage.allocation,
                            nullptr));

    VkImageViewCreateInfo imageViewCreateInfo = {};
    imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCreateInfo.image = allocatedImage.image;
    imageViewCreateInfo.format = allocatedImage.format;
    imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    imageViewCreateInfo.subresourceRange.levelCount = 1;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount = 1;
    imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    VK_CHECK(vkCreateImageView(device, &imageViewCreateInfo, nullptr,
                               &allocatedImage.imageView));

    return allocatedImage;
}

AllocatedImage create_depth_image(VkDevice device, VmaAllocator allocator,
                                  VkExtent2D swapChainExtent) {
    VkExtent3D depthImageExtent = {swapChainExtent.width,
                                   swapChainExtent.height, 1};

    AllocatedImage allocatedImage;
    allocatedImage.format = VK_FORMAT_D32_SFLOAT;
    allocatedImage.extent = depthImageExtent;

    VkImageUsageFlags depthImageUsages{};
    depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    VkImageCreateInfo imageCreateInfo = {};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = allocatedImage.format;
    imageCreateInfo.extent = allocatedImage.extent;
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage = depthImageUsages;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocInfo.requiredFlags =
        VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VK_CHECK(vmaCreateImage(allocator, &imageCreateInfo, &allocInfo,
                            &allocatedImage.image, &allocatedImage.allocation,
                            nullptr));

    VkImageViewCreateInfo imageViewCreateInfo = {};
    imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCreateInfo.image = allocatedImage.image;
    imageViewCreateInfo.format = allocatedImage.format;
    imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    imageViewCreateInfo.subresourceRange.levelCount = 1;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount = 1;
    imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    VK_CHECK(vkCreateImageView(device, &imageViewCreateInfo, nullptr,
                               &allocatedImage.imageView));

    return allocatedImage;
}

VkCommandPool create_command_pool(VkDevice device,
                                  uint32_t queue_family_index) {
    VkCommandPoolCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    create_info.queueFamilyIndex = queue_family_index;

    VkCommandPool pool;
    VK_CHECK(vkCreateCommandPool(device, &create_info, nullptr, &pool));
    return pool;
}

VkCommandBuffer create_command_buffer(VkDevice device,
                                      VkCommandPool command_pool) {
    VkCommandBufferAllocateInfo allocate_info = {};
    allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocate_info.commandPool = command_pool;
    allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate_info.commandBufferCount = 1;

    VkCommandBuffer buffer;
    VK_CHECK(vkAllocateCommandBuffers(device, &allocate_info, &buffer));
    return buffer;
}

VkQueue get_device_queue(VkDevice device, uint32_t family_index,
                         uint32_t queue_index) {
    VkQueue queue;
    vkGetDeviceQueue(device, family_index, queue_index, &queue);
    return queue;
}

static uint32_t find_memory_type(VkPhysicalDevice physical_device,
                                 uint32_t type_filter,
                                 VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_props);

    for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
        if (((1 << i) & type_filter) &&
            mem_props.memoryTypes[i].propertyFlags & props) {
            return i;
        }
    }
    throw std::runtime_error("failed to find suitable memory type");
}

VkSemaphoreSubmitInfo
create_semaphore_submit_info(VkPipelineStageFlags2 stageMask,
                             VkSemaphore semaphore) {
    VkSemaphoreSubmitInfo sumbitInfo = {};
    sumbitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    sumbitInfo.semaphore = semaphore;
    sumbitInfo.stageMask = stageMask;
    sumbitInfo.value = 1;
    sumbitInfo.deviceIndex = 0;
    return sumbitInfo;
}

VmaAllocator create_allocator(VkInstance instance,
                              VkPhysicalDevice physicalDevice,
                              VkDevice device) {
    VmaAllocatorCreateInfo createInfo = {};
    createInfo.physicalDevice = physicalDevice;
    createInfo.device = device;
    createInfo.instance = instance;
    createInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

    VmaAllocator allocator;
    vmaCreateAllocator(&createInfo, &allocator);
    return allocator;
}

VkRenderingAttachmentInfo create_color_attachment_info(VkImageView view,
                                                       VkClearValue *clear,
                                                       VkImageLayout layout) {
    VkRenderingAttachmentInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    info.imageView = view;
    info.imageLayout = layout;
    info.loadOp =
        clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    if (clear) {
        info.clearValue = *clear;
    }
    return info;
}
