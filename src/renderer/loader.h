#pragma once
#include "types.h"
#include <filesystem>
#include <unordered_map>
#include <vector>

struct GeoSurface {
    uint32_t startIndex;
    uint32_t count;
};

struct MeshAsset {
    std::string name;
    std::vector<GeoSurface> surfaces;
    GPUMeshBuffers meshBuffers;
};

class Renderer;
