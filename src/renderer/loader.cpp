#include "loader.h"
#include "renderer.hpp"
#include "stb_image.h"
#include "types.h"
#include "vertex.h"
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <glm/gtx/quaternion.hpp>
#include <iostream>

std::optional<std::vector<std::shared_ptr<MeshAsset>>>
load_gltf_meshes(Renderer *renderer, std::filesystem::path filePath) {
    std::cout << "Loading GLTF: " << filePath << std::endl;

    constexpr auto GLTF_OPTIONS = fastgltf::Options::LoadExternalBuffers;
    fastgltf::Parser parser{};

    auto gltfFile = fastgltf::MappedGltfFile::FromPath(filePath);
    if (!bool(gltfFile)) {
        std::cerr << "Failed to open glTF file: "
                  << fastgltf::getErrorMessage(gltfFile.error()) << '\n';
        return {};
    }

    auto asset = parser.loadGltf(gltfFile.get(), filePath.parent_path());
    if (asset.error() != fastgltf::Error::None) {
        fmt::print("Failed to load glTF: {} \n",
                   fastgltf::getErrorMessage(asset.error()));
        return {};
    }
    fastgltf::Asset gltf = std::move(asset.get());

    std::vector<std::shared_ptr<MeshAsset>> meshes;

    // Note: Use the same vectors for all meshes so the memory doesn't
    // reallocate as often
    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;
    for (fastgltf::Mesh &mesh : gltf.meshes) {
        MeshAsset newMesh;
        newMesh.name = mesh.name;

        // clear the mesh array each mesh, we don't want to merge them by error
        indices.clear();
        vertices.clear();

        for (auto &&p : mesh.primitives) {
            GeoSurface newSurface;
            newSurface.startIndex = static_cast<uint32_t>(indices.size());
            newSurface.count = static_cast<uint32_t>(
                gltf.accessors[p.indicesAccessor.value()].count);

            size_t initialVtx = vertices.size();

            // load indices
            {
                fastgltf::Accessor &indexAccessor =
                    gltf.accessors[p.indicesAccessor.value()];
                indices.reserve(indices.size() + indexAccessor.count);

                fastgltf::iterateAccessor<std::uint32_t>(
                    gltf, indexAccessor, [&](std::uint32_t idx) {
                        indices.push_back(idx + initialVtx);
                    });
            }

            // load vertex positions
            {
                fastgltf::Accessor &posAccessor =
                    gltf.accessors[p.findAttribute("POSITION")->accessorIndex];
                vertices.resize(vertices.size() + posAccessor.count);

                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
                    gltf, posAccessor,
                    [&](fastgltf::math::fvec3 v, size_t index) {
                        Vertex newVtx;
                        newVtx.pos.x = v.x();
                        newVtx.pos.y = v.y();
                        newVtx.pos.z = v.z();
                        newVtx.color = glm::vec4{1.f};
                        newVtx.normal = {1, 0, 0};
                        newVtx.uv_x = 0;
                        newVtx.uv_y = 0;
                        vertices[initialVtx + index] = newVtx;
                    });
            }

            // load vertex normals
            auto normals = p.findAttribute("NORMAL");
            if (normals != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
                    gltf, gltf.accessors[(*normals).accessorIndex],
                    [&](fastgltf::math::fvec3 v, size_t index) {
                        vertices[initialVtx + index].normal.x = v.x();
                        vertices[initialVtx + index].normal.y = v.y();
                        vertices[initialVtx + index].normal.z = v.z();
                    });
            }

            // load UVs
            auto uv = p.findAttribute("TEXCOORD_0");
            if (uv != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(
                    gltf, gltf.accessors[(*uv).accessorIndex],
                    [&](fastgltf::math::fvec2 v, size_t index) {
                        vertices[initialVtx + index].uv_x = v.x();
                        vertices[initialVtx + index].uv_y = v.y();
                    });
            }

            // load vertex colors
            auto colors = p.findAttribute("COLOR_0");
            if (colors != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(
                    gltf, gltf.accessors[(*uv).accessorIndex],
                    [&](fastgltf::math::fvec4 v, size_t index) {
                        vertices[initialVtx + index].color.x = v.x();
                        vertices[initialVtx + index].color.y = v.y();
                        vertices[initialVtx + index].color.z = v.z();
                        vertices[initialVtx + index].color.w = v.w();
                    });
            }

            newMesh.surfaces.push_back(newSurface);
        }

        // display the vertex normals
        constexpr bool OVERRIDE_COLORS = true;
        if (OVERRIDE_COLORS) {
            for (Vertex &vtx : vertices) {
                vtx.color = glm::vec4(vtx.normal, 1.f);
            }
        }
        newMesh.meshBuffers = renderer->uploadMesh(indices, vertices);

        meshes.emplace_back(std::make_shared<MeshAsset>(std::move(newMesh)));
    }

    return meshes;
}
