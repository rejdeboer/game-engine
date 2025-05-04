#pragma once
#include "descriptor.h"
#include "types.h"
#include <string>
#include <unordered_map>
#include <vector>

struct GLTFMaterial {
    MaterialInstance data;
};

struct GeoSurface {
    uint32_t startIndex;
    uint32_t count;
    Bounds bounds;
    std::shared_ptr<GLTFMaterial> material;
};

struct MeshAsset {
    std::string name;
    std::vector<GeoSurface> surfaces;
    GPUMeshBuffers meshBuffers;
};

struct SceneNode {
    std::weak_ptr<SceneNode> parent;
    std::vector<std::shared_ptr<SceneNode>> children;

    glm::mat4 localTransform;
    glm::mat4 worldTransform;

    std::optional<std::size_t> meshIndex{std::nullopt};

    void refresh_transform(const glm::mat4 &parentMatrix) {
        worldTransform = parentMatrix * localTransform;
        for (auto c : children) {
            c->refresh_transform(worldTransform);
        }
    }
};

struct Scene {
    std::unordered_map<std::string, std::shared_ptr<MeshAsset>> meshes;
    std::unordered_map<std::string, std::shared_ptr<SceneNode>> nodes;
    std::unordered_map<std::string, AllocatedImage> images;
    std::unordered_map<std::string, std::shared_ptr<GLTFMaterial>> materials;

    std::vector<std::shared_ptr<SceneNode>> topNodes;

    std::vector<VkSampler> samplers;

    DescriptorAllocatorGrowable descriptorPool;

    AllocatedBuffer materialDataBuffer;

    ~Scene();
};
