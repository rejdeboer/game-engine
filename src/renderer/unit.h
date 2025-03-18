#pragma once
#include "types.h"
#include <glm/glm.hpp>
#include <vulkan/vulkan_core.h>

struct UnitVertex {
    glm::vec3 pos;
    float uv_x;
    glm::vec3 normal;
    float uv_y;
    glm::vec4 color;
};

class Renderer; // Forward declaration

class UnitRenderer {
  public:
    void init(Renderer *renderer);
    void deinit();
    void render(RenderContext ctx);

  private:
    Renderer *_renderer;
    MaterialPipeline _pipeline;

    void init_pipeline();
    void init_buffers();
};
