#pragma once
#include "descriptor.h"
#include "types.h"
#include <optional>
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
    std::string name;
    std::vector<std::size_t> childrenIndices;

    glm::mat4 transform;

    std::optional<std::size_t> meshIndex{std::nullopt};
};

struct Scene {
    std::unordered_map<std::string, std::shared_ptr<MeshAsset>> meshes;
    std::vector<SceneNode> nodes;
    std::unordered_map<std::string, AllocatedImage> images;
    std::unordered_map<std::string, std::shared_ptr<GLTFMaterial>> materials;

    std::vector<std::size_t> topNodes;

    std::vector<VkSampler> samplers;

    DescriptorAllocatorGrowable descriptorPool;

    AllocatedBuffer materialDataBuffer;

    ~Scene();
};
