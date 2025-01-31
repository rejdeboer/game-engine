#include "config.h"
#include <optional>
#include <set>
#include <vector>

const std::vector<const char *> validation_layers = {
    "VK_LAYER_KHRONOS_validation"};

const std::vector<const char *> device_extensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME};

struct QueueFamilyIndices {
    std::optional<uint32_t> graphics_family;
    std::optional<uint32_t> present_family;
    bool is_complete() {
        return graphics_family.has_value() && present_family.has_value();
    }
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;
};

// TODO: Can we use validation layers on Apple?
#if defined(NDEBUG) || defined(__APPLE__)
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

static SwapChainSupportDetails query_swap_chain_support(VkPhysicalDevice device,
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

static inline VkSurfaceFormatKHR choose_swap_surface_format(
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

static inline VkPresentModeKHR
choose_swap_present_mode(const std::vector<VkPresentModeKHR> &available_modes) {
    for (const auto &mode : available_modes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return mode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

static VkExtent2D
choose_swap_extent(SDL_Window *window,
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

static QueueFamilyIndices
find_compatible_queue_family_indices(VkPhysicalDevice device,
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

static VkPhysicalDevice pick_physical_device(VkInstance instance,
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

static VkInstance create_vulkan_instance(SDL_Window *window) {
    if (enable_validation_layers && !has_validation_layer_support()) {
        printf("validation layers requested, but not available\n");
        return VK_NULL_HANDLE;
    }

    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "VulkanRenderer";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "No Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_4;

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
    VkResult result = vkCreateInstance(&create_info, nullptr, &instance);
    if (result != VK_SUCCESS) {
        printf("failed to create vulkan instance; result: %d\n", result);
        return VK_NULL_HANDLE;
    }

    return instance;
}

static VkSurfaceKHR create_surface(SDL_Window *window, VkInstance instance) {
    VkSurfaceKHR surface;
    if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface)) {
        fprintf(stderr, "error creating vulkan surface: %s\n", SDL_GetError());
        return VK_NULL_HANDLE;
    }
    return surface;
}

static VkDevice create_device(VkPhysicalDevice physical_device,
                              uint32_t graphics_index,
                              uint32_t presentation_index) {
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

    VkPhysicalDeviceFeatures physical_device_features;
    vkGetPhysicalDeviceFeatures(physical_device, &physical_device_features);

    VkDeviceCreateInfo device_create_info = {};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.pEnabledFeatures = &physical_device_features;
    device_create_info.queueCreateInfoCount =
        static_cast<uint32_t>(unique_queue_families.size());
    device_create_info.pQueueCreateInfos = &queue_create_infos[0];
    device_create_info.enabledExtensionCount =
        static_cast<uint32_t>(device_extensions.size());
    device_create_info.ppEnabledExtensionNames = &device_extensions[0];

    VkDevice device;
    VkResult result =
        vkCreateDevice(physical_device, &device_create_info, nullptr, &device);
    if (result != VK_SUCCESS) {
        printf("could not create logical device; error: %d", result);
        throw std::runtime_error("could not create logical device");
    };

    return device;
}

static VkSwapchainKHR create_swap_chain(SDL_Window *window,
                                        VkPhysicalDevice physical_device,
                                        VkDevice device, VkSurfaceKHR surface,
                                        uint32_t graphics_index,
                                        uint32_t presentation_index) {
    SwapChainSupportDetails support_details =
        query_swap_chain_support(physical_device, surface);
    VkSurfaceFormatKHR surface_format =
        choose_swap_surface_format(support_details.formats);
    VkPresentModeKHR present_mode =
        choose_swap_present_mode(support_details.present_modes);
    VkExtent2D extent =
        choose_swap_extent(window, support_details.capabilities);

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
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    create_info.preTransform = support_details.capabilities.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;

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
    VkResult result =
        vkCreateSwapchainKHR(device, &create_info, nullptr, &swap_chain);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "error with code %d creating swap chain\n", result);
        throw std::runtime_error("error creating swap chain");
    }

    return swap_chain;
}

static inline VkQueue get_device_queue(VkDevice device, uint32_t family_index,
                                       uint32_t queue_index) {
    VkQueue queue;
    vkGetDeviceQueue(device, family_index, queue_index, &queue);
    return queue;
}

VulkanContext vulkan_initialize(SDL_Window *window) {
    VkInstance instance = create_vulkan_instance(window);
    if (!instance) {
        throw std::runtime_error("could not create vulkan instance");
    }
    VkSurfaceKHR surface = create_surface(window, instance);
    if (!surface) {
        throw std::runtime_error("could not create vulkan surface");
    }

    VkPhysicalDevice physical_device = pick_physical_device(instance, surface);
    QueueFamilyIndices queue_family_indices =
        find_compatible_queue_family_indices(physical_device, surface);
    assert(queue_family_indices.is_complete());

    uint32_t graphics_index = queue_family_indices.graphics_family.value();
    uint32_t presentation_index = queue_family_indices.present_family.value();
    VkDevice device =
        create_device(physical_device, graphics_index, presentation_index);
    VkSwapchainKHR swap_chain =
        create_swap_chain(window, physical_device, device, surface,
                          graphics_index, presentation_index);

    return VulkanContext(instance, device, surface, swap_chain,
                         get_device_queue(device, graphics_index, 0),
                         get_device_queue(device, presentation_index, 0));
}
