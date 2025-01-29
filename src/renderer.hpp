#ifndef RENDERER_H

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <cassert>
#include <optional>
#include <vector>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

class Renderer {
  private:
    VkInstance instance;
    VkDevice device;
    VkSurfaceKHR surface;

  public:
    Renderer(SDL_Window *window);
    void deinit();
};

#define RENDERER_H
#endif
