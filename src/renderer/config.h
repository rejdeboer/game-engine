#include "context.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <cassert>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

VulkanContext vulkan_initialize(SDL_Window *window);
