#pragma once
#include "../math/aabb.h"
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
    std::vector<size_t> childrenIndices;

    glm::mat4 transform;

    std::optional<size_t> meshIndex{std::nullopt};
    // TODO: Use skin index if multiple skin support is implemented
    bool isSkinned{false};
};

struct Animation {
    std::string name;
};

struct Skin {
    std::string name;
    std::vector<glm::mat4> inverseBindMatrices;
    std::vector<std::shared_ptr<SceneNode>> joints;
};

struct Scene {
    std::vector<SceneNode> nodes;
    std::vector<MeshAsset> meshes;
    std::unordered_map<std::string, AllocatedImage> images;
    std::unordered_map<std::string, std::shared_ptr<GLTFMaterial>> materials;
    std::unordered_map<std::string, Animation> animations;
    // TODO: Support multiple skins?
    std::optional<Skin> skin;

    std::vector<size_t> topNodes;

    std::vector<VkSampler> samplers;

    DescriptorAllocatorGrowable descriptorPool;

    AllocatedBuffer materialDataBuffer;

    ~Scene();

    std::optional<math::AABB> get_local_aabb() const;
};
