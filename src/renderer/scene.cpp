#include "scene.h"
#include "renderer.hpp"

Scene::~Scene() {
    Renderer &creator = Renderer::Get();
    VkDevice dv = creator._device;

    descriptorPool.destroy_pools(dv);
    creator.destroy_buffer(materialDataBuffer);

    for (auto &m : meshes) {
        creator.destroy_buffer(m.meshBuffers.indexBuffer);
        creator.destroy_buffer(m.meshBuffers.vertexBuffer);
    }

    for (auto &[k, v] : images) {
        if (v.image == creator._errorCheckerboardImage.image) {
            // dont destroy the default images
            continue;
        }
        creator.destroy_image(v);
    }

    for (auto &sampler : samplers) {
        vkDestroySampler(dv, sampler, nullptr);
    }
}
