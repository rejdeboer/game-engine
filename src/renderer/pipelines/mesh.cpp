#include "mesh.h"
#include "../frustum_culling.h"
#include "builder.h"
#include <algorithm>

void MeshPipeline::init(VkDevice device, VkFormat drawImageFormat,
                        VkFormat depthImageFormat,
                        VkDescriptorSetLayout gpuSceneDataDescriptorLayout,
                        VkDescriptorSetLayout shadowDescriptorSetLayout) {
    VkShaderModule meshFragShader;
    if (!vkutil::load_shader_module("shaders/spv/mesh.frag.spv", device,
                                    &meshFragShader)) {
        fmt::println("Error when building the mesh fragment shader module");
    }

    VkShaderModule meshVertShader;
    if (!vkutil::load_shader_module("shaders/spv/mesh.vert.spv", device,
                                    &meshVertShader)) {
        fmt::println("Error when building the mesh vertex shader module");
    }

    VkPushConstantRange matrixRange{};
    matrixRange.offset = 0;
    matrixRange.size = sizeof(GPUDrawPushConstants);
    matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    DescriptorLayoutBuilder layoutBuilder;
    layoutBuilder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    layoutBuilder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    layoutBuilder.add_binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    _materialLayout = layoutBuilder.build(
        device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    VkDescriptorSetLayout layouts[] = {
        gpuSceneDataDescriptorLayout,
        _materialLayout,
        shadowDescriptorSetLayout,
    };

    VkPipelineLayoutCreateInfo meshLayoutInfo = {};
    meshLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    meshLayoutInfo.setLayoutCount = 3;
    meshLayoutInfo.pSetLayouts = layouts;
    meshLayoutInfo.pushConstantRangeCount = 1;
    meshLayoutInfo.pPushConstantRanges = &matrixRange;

    VkPipelineLayout newLayout;
    VK_CHECK(
        vkCreatePipelineLayout(device, &meshLayoutInfo, nullptr, &newLayout));

    _opaquePipeline.layout = newLayout;
    _transparentPipeline.layout = newLayout;
    _stencilWritePipeline.layout = newLayout;
    _outlinePipeline.layout = newLayout;

    PipelineBuilder pipelineBuilder;
    pipelineBuilder.set_shaders(meshVertShader, meshFragShader);
    pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
    pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    pipelineBuilder.set_multisampling_none();
    pipelineBuilder.disable_blending();
    pipelineBuilder.enable_depthtest(true, VK_COMPARE_OP_LESS_OR_EQUAL);
    pipelineBuilder.set_color_attachment_format(drawImageFormat);
    pipelineBuilder.set_depth_format(depthImageFormat);
    pipelineBuilder.set_stencil_format(depthImageFormat);
    pipelineBuilder._pipelineLayout = newLayout;
    _opaquePipeline.pipeline = pipelineBuilder.build_pipeline(device);

    pipelineBuilder.enable_blending_additive();
    pipelineBuilder.enable_depthtest(false, VK_COMPARE_OP_LESS_OR_EQUAL);
    _transparentPipeline.pipeline = pipelineBuilder.build_pipeline(device);

    pipelineBuilder.enable_depthtest(true, VK_COMPARE_OP_LESS_OR_EQUAL);
    pipelineBuilder.disable_blending();
    pipelineBuilder.enable_stenciltest(
        VK_COMPARE_OP_ALWAYS, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_KEEP,
        VK_STENCIL_OP_KEEP, 0xFF, 0xFF);
    _stencilWritePipeline.pipeline = pipelineBuilder.build_pipeline(device);

    vkDestroyShaderModule(device, meshVertShader, nullptr);
    vkDestroyShaderModule(device, meshFragShader, nullptr);

    VkShaderModule outlineFragShader;
    if (!vkutil::load_shader_module("shaders/spv/outline.frag.spv", device,
                                    &outlineFragShader)) {
        fmt::println("Error when building the outline fragment shader module");
    }

    VkShaderModule outlineVertShader;
    if (!vkutil::load_shader_module("shaders/spv/outline.vert.spv", device,
                                    &outlineVertShader)) {
        fmt::println("Error when building the outline vertex shader module");
    }

    pipelineBuilder.set_shaders(outlineVertShader, outlineFragShader);
    pipelineBuilder.set_cull_mode(VK_CULL_MODE_FRONT_BIT,
                                  VK_FRONT_FACE_CLOCKWISE);
    pipelineBuilder.set_color_write_mask(0);
    pipelineBuilder.disable_depthtest();
    pipelineBuilder.enable_stenciltest(VK_COMPARE_OP_NOT_EQUAL,
                                       VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP,
                                       VK_STENCIL_OP_KEEP, 0xFF, 0x00);
    _outlinePipeline.pipeline = pipelineBuilder.build_pipeline(device);

    vkDestroyShaderModule(device, outlineVertShader, nullptr);
    vkDestroyShaderModule(device, outlineFragShader, nullptr);
}

void MeshPipeline::draw(const RenderContext &ctx,
                        const std::vector<MeshDrawCommand> &drawCommands) {
    std::vector<uint32_t> objectIndices;
    objectIndices.reserve(drawCommands.size());

    for (int i = 0; i < drawCommands.size(); i++) {
        if (vkutil::is_visible(drawCommands[i].transform,
                               drawCommands[i].bounds, ctx.viewproj)) {
            objectIndices.push_back(i);
        }
    }

    // sort the opaque surfaces by material and mesh
    std::sort(objectIndices.begin(), objectIndices.end(),
              [&](const auto &iA, const auto &iB) {
                  const MeshDrawCommand &A = drawCommands[iA];
                  const MeshDrawCommand &B = drawCommands[iB];
                  if (A.material == B.material) {
                      return A.indexBuffer < B.indexBuffer;
                  } else {
                      return A.material < B.material;
                  }
              });

    MaterialPipeline *lastPipeline = nullptr;
    MaterialInstance *lastMaterial = nullptr;
    VkBuffer lastIndexBuffer = VK_NULL_HANDLE;

    for (auto &i : objectIndices) {
        auto r = drawCommands[i];
        if (r.material != lastMaterial) {
            lastMaterial = r.material;
            if (r.material->pipeline != lastPipeline) {
                lastPipeline = r.material->pipeline;
                vkCmdBindPipeline(ctx.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  r.material->pipeline->pipeline);
                vkCmdBindDescriptorSets(ctx.cmd,
                                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        r.material->pipeline->layout, 0, 1,
                                        &ctx.globalDescriptorSet, 0, nullptr);
                vkCmdBindDescriptorSets(ctx.cmd,
                                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        r.material->pipeline->layout, 2, 1,
                                        &ctx.shadowMapSet, 0, nullptr);

                VkViewport viewport = {};
                viewport.x = 0;
                viewport.y = 0;
                viewport.width = (float)ctx.drawExtent.width;
                viewport.height = (float)ctx.drawExtent.height;
                viewport.minDepth = 0.f;
                viewport.maxDepth = 1.f;
                vkCmdSetViewport(ctx.cmd, 0, 1, &viewport);

                VkRect2D scissor = {};
                scissor.offset.x = 0;
                scissor.offset.y = 0;
                scissor.extent.width = ctx.drawExtent.width;
                scissor.extent.height = ctx.drawExtent.height;
                vkCmdSetScissor(ctx.cmd, 0, 1, &scissor);
            }

            vkCmdBindDescriptorSets(ctx.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    r.material->pipeline->layout, 1, 1,
                                    &r.material->materialSet, 0, nullptr);
        }

        if (r.indexBuffer != lastIndexBuffer) {
            lastIndexBuffer = r.indexBuffer;
            vkCmdBindIndexBuffer(ctx.cmd, r.indexBuffer, 0,
                                 VK_INDEX_TYPE_UINT32);
        }

        GPUDrawPushConstants pushConstants;
        pushConstants.vertexBuffer = r.vertexBufferAddress;
        pushConstants.worldMatrix = r.transform;
        vkCmdPushConstants(ctx.cmd, r.material->pipeline->layout,
                           VK_SHADER_STAGE_VERTEX_BIT, 0,
                           sizeof(GPUDrawPushConstants), &pushConstants);

        vkCmdDrawIndexed(ctx.cmd, r.indexCount, 1, r.firstIndex, 0, 0);
    }
}

MaterialInstance
MeshPipeline::write_material(VkDevice device, MaterialPass pass,
                             const MaterialResources &resources,
                             DescriptorAllocatorGrowable &descriptorAllocator) {

    MaterialInstance matData;
    matData.passType = pass;
    if (pass == MaterialPass::Transparent) {
        matData.pipeline = &_transparentPipeline;
    } else {
        matData.pipeline = &_opaquePipeline;
    }

    matData.materialSet = descriptorAllocator.allocate(device, _materialLayout);

    _writer.clear();
    _writer.write_buffer(0, resources.dataBuffer, sizeof(MaterialConstants),
                         resources.dataBufferOffset,
                         VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    _writer.write_image(1, resources.colorImage.imageView,
                        resources.colorSampler,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    _writer.write_image(2, resources.metalRoughImage.imageView,
                        resources.metalRoughSampler,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    _writer.update_set(device, matData.materialSet);

    return matData;
}
