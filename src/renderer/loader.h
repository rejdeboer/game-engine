#pragma once
#include "types.h"
#include <filesystem>
#include <optional>
#include <unordered_map>
#include <vector>

class Renderer;

struct GeoSurface {
    uint32_t startIndex;
    uint32_t count;
};

struct MeshAsset {
    std::string name;
    std::vector<GeoSurface> surfaces;
    GPUMeshBuffers meshBuffers;
};

std::optional<std::vector<std::shared_ptr<MeshAsset>>>
load_gltf_meshes(Renderer *renderer, std::filesystem::path filePath);
