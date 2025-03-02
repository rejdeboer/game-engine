#pragma once
#include "descriptor.h"
#include "types.h"
#include <optional>
#include <unordered_map>
#include <vector>

class Renderer;

struct GLTFMaterial {
    MaterialInstance data;
};

struct Bounds {
    glm::vec3 origin;
    float sphereRadius;
    glm::vec3 extents;
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

struct LoadedGLTF : public IRenderable {
    std::unordered_map<std::string, std::shared_ptr<MeshAsset>> meshes;
    std::unordered_map<std::string, std::shared_ptr<Node>> nodes;
    std::unordered_map<std::string, AllocatedImage> images;
    std::unordered_map<std::string, std::shared_ptr<GLTFMaterial>> materials;

    std::vector<std::shared_ptr<Node>> topNodes;

    std::vector<VkSampler> samplers;

    DescriptorAllocatorGrowable descriptorPool;

    AllocatedBuffer materialDataBuffer;

    Renderer *creator;

    ~LoadedGLTF() { clearAll(); };

    virtual void Draw(const glm::mat4 &topMatrix, DrawContext &ctx);

  private:
    void clearAll();
};

std::optional<std::shared_ptr<LoadedGLTF>> loadGltf(Renderer *renderer,
                                                    std::string_view filePath);
