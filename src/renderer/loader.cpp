#include "loader.h"
#include "renderer.hpp"
#include "types.h"
#include "vertex.h"
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <filesystem>
#include <glm/gtx/quaternion.hpp>
#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

VkFilter extract_filter(fastgltf::Filter filter) {
    switch (filter) {
    // nearest samplers
    case fastgltf::Filter::Nearest:
    case fastgltf::Filter::NearestMipMapNearest:
    case fastgltf::Filter::NearestMipMapLinear:
        return VK_FILTER_NEAREST;

    // linear samplers
    case fastgltf::Filter::Linear:
    case fastgltf::Filter::LinearMipMapNearest:
    case fastgltf::Filter::LinearMipMapLinear:
    default:
        return VK_FILTER_LINEAR;
    }
}

VkSamplerMipmapMode extract_mipmap_mode(fastgltf::Filter filter) {
    switch (filter) {
    case fastgltf::Filter::NearestMipMapNearest:
    case fastgltf::Filter::LinearMipMapNearest:
        return VK_SAMPLER_MIPMAP_MODE_NEAREST;

    case fastgltf::Filter::NearestMipMapLinear:
    case fastgltf::Filter::LinearMipMapLinear:
    default:
        return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }
}

std::optional<AllocatedImage>
load_image(Renderer *renderer, fastgltf::Asset &asset, fastgltf::Image &image) {
    AllocatedImage newImage{};

    int width, height, nrChannels;

    std::visit(
        fastgltf::visitor{
            [](auto &arg) {},
            [&](fastgltf::sources::URI &filePath) {
                assert(filePath.fileByteOffset ==
                       0); // We don't support offsets with stbi.
                assert(filePath.uri.isLocalPath()); // We're only capable of
                                                    // loading local files.

                const std::string path(
                    filePath.uri.path().begin(),
                    filePath.uri.path().end()); // Thanks C++.
                unsigned char *data =
                    stbi_load(path.c_str(), &width, &height, &nrChannels, 4);
                if (data) {
                    VkExtent3D imagesize;
                    imagesize.width = width;
                    imagesize.height = height;
                    imagesize.depth = 1;

                    newImage = renderer->create_image(
                        data, imagesize, VK_FORMAT_R8G8B8A8_UNORM,
                        VK_IMAGE_USAGE_SAMPLED_BIT, false);

                    stbi_image_free(data);
                }
            },
            [&](fastgltf::sources::Vector &vector) {
                unsigned char *data =
                    stbi_load_from_memory((unsigned char *)vector.bytes.data(),
                                          static_cast<int>(vector.bytes.size()),
                                          &width, &height, &nrChannels, 4);
                if (data) {
                    VkExtent3D imagesize;
                    imagesize.width = width;
                    imagesize.height = height;
                    imagesize.depth = 1;

                    newImage = renderer->create_image(
                        data, imagesize, VK_FORMAT_R8G8B8A8_UNORM,
                        VK_IMAGE_USAGE_SAMPLED_BIT, true);

                    stbi_image_free(data);
                }
            },
            [&](fastgltf::sources::BufferView &view) {
                auto &bufferView = asset.bufferViews[view.bufferViewIndex];
                auto &buffer = asset.buffers[bufferView.bufferIndex];

                std::visit(
                    fastgltf::visitor{
                        // We only care about VectorWithMime here, because we
                        // specify LoadExternalBuffers, meaning all buffers
                        // are already loaded into a vector.
                        [](auto &arg) {},
                        [&](fastgltf::sources::Vector &vector) {
                            unsigned char *data = stbi_load_from_memory(
                                (unsigned char *)vector.bytes.data() +
                                    bufferView.byteOffset,
                                static_cast<int>(bufferView.byteLength), &width,
                                &height, &nrChannels, 4);
                            if (data) {
                                VkExtent3D imagesize;
                                imagesize.width = width;
                                imagesize.height = height;
                                imagesize.depth = 1;

                                newImage = renderer->create_image(
                                    data, imagesize, VK_FORMAT_R8G8B8A8_UNORM,
                                    VK_IMAGE_USAGE_SAMPLED_BIT, false);

                                stbi_image_free(data);
                            }
                        }},
                    buffer.data);
            },
        },
        image.data);

    // if any of the attempts to load the data failed, we havent written the
    // image so handle is null
    if (newImage.image == VK_NULL_HANDLE) {
        return {};
    } else {
        return newImage;
    }
}

std::optional<std::shared_ptr<Scene>> loadGltf(Renderer *renderer,
                                               std::string_view filePath) {
    std::shared_ptr<Scene> scenePtr = std::make_shared<Scene>();
    Scene &scene = *scenePtr.get();

    constexpr auto GLTF_OPTIONS =
        fastgltf::Options::DontRequireValidAssetMember |
        fastgltf::Options::AllowDouble | fastgltf::Options::LoadExternalBuffers;

    auto gltfFile = fastgltf::MappedGltfFile::FromPath(filePath);
    if (!bool(gltfFile)) {
        std::cerr << "Failed to open glTF file: "
                  << fastgltf::getErrorMessage(gltfFile.error()) << '\n';
        return {};
    }

    std::filesystem::path path = filePath;
    auto type = fastgltf::determineGltfFileType(gltfFile.get());
    fastgltf::Parser parser{};
    auto load = (type == fastgltf::GltfType::glTF)
                    ? parser.loadGltf(gltfFile.get(), path.parent_path())
                : (type == fastgltf::GltfType::GLB)
                    ? parser.loadGltfBinary(gltfFile.get(), path.parent_path())
                    : fastgltf::Error::InvalidPath;
    if (load.error() != fastgltf::Error::None) {
        fmt::print("Failed to load glTF: {} \n",
                   fastgltf::getErrorMessage(load.error()));
        return {};
    }
    fastgltf::Asset gltf = std::move(load.get());

    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}};

    scene.descriptorPool.init(renderer->_device, gltf.materials.size(), sizes);

    // load samplers
    for (fastgltf::Sampler &sampler : gltf.samplers) {

        VkSamplerCreateInfo sampl = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext = nullptr};
        sampl.maxLod = VK_LOD_CLAMP_NONE;
        sampl.minLod = 0;

        sampl.magFilter = extract_filter(
            sampler.magFilter.value_or(fastgltf::Filter::Nearest));
        sampl.minFilter = extract_filter(
            sampler.minFilter.value_or(fastgltf::Filter::Nearest));

        sampl.mipmapMode = extract_mipmap_mode(
            sampler.minFilter.value_or(fastgltf::Filter::Nearest));

        VkSampler newSampler;
        vkCreateSampler(renderer->_device, &sampl, nullptr, &newSampler);
        scene.samplers.push_back(newSampler);
    }

    // temporal arrays for all the objects to use while creating the GLTF data
    std::vector<SceneNode> nodes;
    std::vector<AllocatedImage> images;
    std::vector<std::shared_ptr<GLTFMaterial>> materials;

    for (fastgltf::Image &image : gltf.images) {
        std::optional<AllocatedImage> img = load_image(renderer, gltf, image);

        if (img.has_value()) {
            images.push_back(*img);
            scene.images[image.name.c_str()] = *img;
        } else {
            // we failed to load, so lets give the slot a default white texture
            // to not completely break loading
            images.push_back(renderer->_errorCheckerboardImage);
            std::cout << "gltf failed to load texture " << image.name
                      << std::endl;
        }
    }

    // create buffer to hold the material data
    scene.materialDataBuffer = renderer->create_buffer(
        sizeof(MeshPipeline::MaterialConstants) * gltf.materials.size(),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    int data_index = 0;
    MeshPipeline::MaterialConstants *sceneMaterialConstants =
        (MeshPipeline::MaterialConstants *)
            scene.materialDataBuffer.info.pMappedData;

    for (fastgltf::Material &mat : gltf.materials) {
        std::shared_ptr<GLTFMaterial> newMat = std::make_shared<GLTFMaterial>();
        materials.push_back(newMat);
        scene.materials[mat.name.c_str()] = newMat;

        MeshPipeline::MaterialConstants constants;
        constants.colorFactors.x = mat.pbrData.baseColorFactor[0];
        constants.colorFactors.y = mat.pbrData.baseColorFactor[1];
        constants.colorFactors.z = mat.pbrData.baseColorFactor[2];
        constants.colorFactors.w = mat.pbrData.baseColorFactor[3];

        constants.metalRoughFactors.x = mat.pbrData.metallicFactor;
        constants.metalRoughFactors.y = mat.pbrData.roughnessFactor;
        // write material parameters to buffer
        sceneMaterialConstants[data_index] = constants;

        MaterialPass passType = MaterialPass::MainColor;
        if (mat.alphaMode == fastgltf::AlphaMode::Blend) {
            passType = MaterialPass::Transparent;
        }

        MeshPipeline::MaterialResources materialResources;
        // default the material textures
        materialResources.colorImage = renderer->_whiteImage;
        materialResources.colorSampler = renderer->_defaultSamplerLinear;
        materialResources.metalRoughImage = renderer->_whiteImage;
        materialResources.metalRoughSampler = renderer->_defaultSamplerLinear;

        // set the uniform buffer for the material data
        materialResources.dataBuffer = scene.materialDataBuffer.buffer;
        materialResources.dataBufferOffset =
            data_index * sizeof(MeshPipeline::MaterialConstants);
        // grab textures from gltf file
        if (mat.pbrData.baseColorTexture.has_value()) {
            size_t img =
                gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex]
                    .imageIndex.value();
            size_t sampler =
                gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex]
                    .samplerIndex.value();

            materialResources.colorImage = images[img];
            materialResources.colorSampler = scene.samplers[sampler];
        }
        // build material
        newMat->data = renderer->_meshPipeline.write_material(
            renderer->_device, passType, materialResources,
            scene.descriptorPool);

        data_index++;
    }

    // use the same vectors for all meshes so that the memory doesnt reallocate
    // as often
    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;

    scene.meshes.reserve(gltf.meshes.size());
    for (fastgltf::Mesh &mesh : gltf.meshes) {
        MeshAsset newMesh;
        newMesh.name = mesh.name;

        // clear the mesh arrays each mesh, we dont want to merge them by error
        indices.clear();
        vertices.clear();

        for (auto &&p : mesh.primitives) {
            GeoSurface newSurface;
            newSurface.startIndex = (uint32_t)indices.size();
            newSurface.count =
                (uint32_t)gltf.accessors[p.indicesAccessor.value()].count;

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
                    gltf, gltf.accessors[(*colors).accessorIndex],
                    [&](fastgltf::math::fvec4 v, size_t index) {
                        vertices[initialVtx + index].color.x = v.x();
                        vertices[initialVtx + index].color.y = v.y();
                        vertices[initialVtx + index].color.z = v.z();
                        vertices[initialVtx + index].color.w = v.w();
                    });
            }

            // load joints
            auto joints = p.findAttribute("JOINTS_0");
            if (joints != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<fastgltf::math::u8vec4>(
                    gltf, gltf.accessors[(*joints).accessorIndex],
                    [&](fastgltf::math::u8vec4 v, size_t index) {
                        vertices[initialVtx + index].jointIndices.x = v.x();
                        vertices[initialVtx + index].jointIndices.y = v.y();
                        vertices[initialVtx + index].jointIndices.z = v.z();
                        vertices[initialVtx + index].jointIndices.w = v.w();
                    });

                auto weights = p.findAttribute("WEIGHTS_0");
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(
                    gltf, gltf.accessors[(*weights).accessorIndex],
                    [&](fastgltf::math::fvec4 v, size_t index) {
                        vertices[initialVtx + index].jointWeights.x = v.x();
                        vertices[initialVtx + index].jointWeights.y = v.y();
                        vertices[initialVtx + index].jointWeights.z = v.z();
                        vertices[initialVtx + index].jointWeights.w = v.w();
                    });
            }

            if (p.materialIndex.has_value()) {
                newSurface.material = materials[p.materialIndex.value()];
            } else {
                newSurface.material = materials[0];
            }

            glm::vec3 minPos = vertices[initialVtx].pos;
            glm::vec3 maxPos = vertices[initialVtx].pos;
            for (int i = initialVtx; i < vertices.size(); i++) {
                minPos = glm::min(minPos, vertices[i].pos);
                maxPos = glm::max(maxPos, vertices[i].pos);
            }

            newSurface.bounds.origin = (maxPos + minPos) / 2.f;
            newSurface.bounds.extents = (maxPos - minPos) / 2.f;
            newSurface.bounds.sphereRadius =
                glm::length(newSurface.bounds.extents);

            newMesh.surfaces.push_back(newSurface);
        }

        newMesh.meshBuffers = renderer->uploadMesh(indices, vertices);
        scene.meshes.emplace_back(newMesh);
    }

    for (fastgltf::Animation &animation : gltf.animations) {
    }

    // load all nodes and their meshes
    scene.nodes.reserve(gltf.nodes.size());
    for (fastgltf::Node &node : gltf.nodes) {
        SceneNode newNode;
        newNode.childrenIndices.reserve(node.children.size());
        for (int i = 0; i < node.children.size(); i++) {
            newNode.childrenIndices.push_back(node.children[i]);
        }

        if (node.meshIndex.has_value()) {
            newNode.meshIndex = node.meshIndex.value();
        }

        std::visit(
            fastgltf::visitor{
                [&](fastgltf::math::fmat4x4 matrix) {
                    memcpy(&newNode.transform, matrix.data(), sizeof(matrix));
                },
                [&](fastgltf::TRS transform) {
                    glm::vec3 tl(transform.translation[0],
                                 transform.translation[1],
                                 transform.translation[2]);
                    glm::quat rot(transform.rotation[3], transform.rotation[0],
                                  transform.rotation[1], transform.rotation[2]);
                    glm::vec3 sc(transform.scale[0], transform.scale[1],
                                 transform.scale[2]);

                    glm::mat4 tm = glm::translate(glm::mat4(1.f), tl);
                    glm::mat4 rm = glm::toMat4(rot);
                    glm::mat4 sm = glm::scale(glm::mat4(1.f), sc);

                    newNode.transform = tm * rm * sm;
                }},
            node.transform);

        scene.nodes.emplace_back(newNode);
    }

    if (!gltf.scenes.empty()) {
        fastgltf::Scene primaryScene = gltf.scenes[0];
        scene.topNodes.reserve(primaryScene.nodeIndices.size());
        for (int i = 0; i < primaryScene.nodeIndices.size(); i++) {
            scene.topNodes.emplace_back(primaryScene.nodeIndices[i]);
        }
    }

    return scenePtr;
}
