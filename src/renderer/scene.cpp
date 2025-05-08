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

std::optional<math::AABB> Scene::get_local_aabb() const {
    float minX = std::numeric_limits<float>::max();
    float minY = std::numeric_limits<float>::max();
    float minZ = std::numeric_limits<float>::max();

    float maxX = std::numeric_limits<float>::lowest();
    float maxY = std::numeric_limits<float>::lowest();
    float maxZ = std::numeric_limits<float>::lowest();

    math::AABB res;
    bool hasAABB = false;

    for (auto &node : nodes) {
        if (!node.meshIndex.has_value()) {
            continue;
        }

        const MeshAsset &mesh = meshes[node.meshIndex.value()];
        assert(!mesh.surfaces.empty());

        math::AABB nodeAABB =
            mesh.surfaces[0].bounds.get_aabb().transform(node.transform);
        for (int i = 1; i < mesh.surfaces.size(); i++) {
            nodeAABB.merge(
                mesh.surfaces[i].bounds.get_aabb().transform(node.transform));
        }

        if (hasAABB) {
            res.merge(nodeAABB);
        } else {
            res = nodeAABB;
            hasAABB = true;
        }
    }

    if (!hasAABB) {
        return std::nullopt;
    }

    return res;
}
