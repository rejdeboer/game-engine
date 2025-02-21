#include "descriptor.h"
#include "types.h"
#include <cstdint>
#include <vulkan/vulkan_core.h>

void DescriptorLayoutBuilder::add_binding(uint32_t binding,
                                          VkDescriptorType type) {
    VkDescriptorSetLayoutBinding newBinding = {};
    newBinding.binding = binding;
    newBinding.descriptorCount = 1;
    newBinding.descriptorType = type;
    bindings.push_back(newBinding);
}

void DescriptorLayoutBuilder::clear() { bindings.clear(); }

VkDescriptorSetLayout
DescriptorLayoutBuilder::build(VkDevice device, VkShaderStageFlags shaderStages,
                               void *pNext,
                               VkDescriptorSetLayoutCreateFlags flags) {
    for (auto &b : bindings) {
        b.stageFlags |= shaderStages;
    }

    VkDescriptorSetLayoutCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    createInfo.pNext = pNext;
    createInfo.pBindings = &bindings[0];
    createInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    createInfo.flags = flags;

    VkDescriptorSetLayout layout;
    VK_CHECK(
        vkCreateDescriptorSetLayout(device, &createInfo, nullptr, &layout));
    return layout;
}

void DescriptorAllocator::init_pool(VkDevice device, uint32_t maxSets,
                                    std::span<PoolSizeRatio> ratios) {
    std::vector<VkDescriptorPoolSize> poolSizes;
    // TODO: What the hell is happening
    for (PoolSizeRatio ratio : ratios) {
        poolSizes.push_back(VkDescriptorPoolSize{
            .type = ratio.type,
            .descriptorCount = uint32_t(ratio.ratio * maxSets)});
    }

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = 0;
    poolInfo.maxSets = maxSets;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = &poolSizes[0];
    vkCreateDescriptorPool(device, &poolInfo, nullptr, &pool);
}

void DescriptorAllocator::clear_descriptors(VkDevice device) {
    vkResetDescriptorPool(device, pool, 0);
}

void DescriptorAllocator::destroy_pool(VkDevice device) {
    vkDestroyDescriptorPool(device, pool, nullptr);
}

VkDescriptorSet DescriptorAllocator::allocate(VkDevice device,
                                              VkDescriptorSetLayout layout) {
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSet ds;
    VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &ds));
    return ds;
}

void DescriptorAllocatorGrowable::init(VkDevice device, uint32_t maxSets,
                                       std::span<PoolSizeRatio> poolRatios) {
    ratios.clear();
    for (PoolSizeRatio ratio : poolRatios) {
        ratios.push_back(ratio);
    }

    VkDescriptorPool newPool = create_pool(device, maxSets, poolRatios);
    setsPerPool = maxSets * 1.5;
    readyPools.push_back(newPool);
}

VkDescriptorPool DescriptorAllocatorGrowable::get_pool(VkDevice device) {
    VkDescriptorPool newPool;
    if (readyPools.size() != 0) {
        newPool = readyPools.back();
        readyPools.pop_back();
    } else {
        newPool = create_pool(device, setsPerPool, ratios);
        setsPerPool *= 1.5;
        if (setsPerPool > 4092) {
            setsPerPool = 4092;
        }
    }

    return newPool;
}

VkDescriptorPool
DescriptorAllocatorGrowable::create_pool(VkDevice device, uint32_t setCount,
                                         std::span<PoolSizeRatio> poolRatios) {
    std::vector<VkDescriptorPoolSize> poolsizes;
    for (PoolSizeRatio ratio : poolRatios) {
        poolsizes.push_back({
            .type = ratio.type,
            .descriptorCount = uint32_t(setCount * ratio.ratio),
        });
    }

    VkDescriptorPoolCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    createInfo.flags = 0;
    createInfo.maxSets = setCount;
    createInfo.poolSizeCount = static_cast<uint32_t>(poolsizes.size());
    createInfo.pPoolSizes = &poolsizes[0];

    VkDescriptorPool newPool;
    vkCreateDescriptorPool(device, &createInfo, nullptr, &newPool);
    return newPool;
}

VkDescriptorSet DescriptorAllocatorGrowable::allocate(
    VkDevice device, VkDescriptorSetLayout layout, void *pNext) {
    VkDescriptorPool poolToUse = get_pool(device);

    VkDescriptorSetAllocateInfo allocateInfo = {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocateInfo.pNext = pNext;
    allocateInfo.descriptorPool = poolToUse;
    allocateInfo.descriptorSetCount = 1;
    allocateInfo.pSetLayouts = &layout;

    VkDescriptorSet ds;
    VkResult res = vkAllocateDescriptorSets(device, &allocateInfo, &ds);

    if (res == VK_ERROR_OUT_OF_POOL_MEMORY || res == VK_ERROR_FRAGMENTED_POOL) {
        fullPools.push_back(poolToUse);
        poolToUse = get_pool(device);
        allocateInfo.descriptorPool = poolToUse;
        VK_CHECK(vkAllocateDescriptorSets(device, &allocateInfo, &ds));
    }

    readyPools.push_back(poolToUse);
    return ds;
}

void DescriptorAllocatorGrowable::clear_pools(VkDevice device) {
    for (auto p : readyPools) {
        vkResetDescriptorPool(device, p, 0);
    }
    for (auto p : fullPools) {
        vkResetDescriptorPool(device, p, 0);
        readyPools.push_back(p);
    }
    fullPools.clear();
}

void DescriptorAllocatorGrowable::destroy_pools(VkDevice device) {
    for (auto p : readyPools) {
        vkDestroyDescriptorPool(device, p, nullptr);
    }
    readyPools.clear();
    for (auto p : fullPools) {
        vkDestroyDescriptorPool(device, p, nullptr);
    }
    fullPools.clear();
}

void DescriptorWriter::write_buffer(int binding, VkBuffer buffer, size_t size,
                                    size_t offset, VkDescriptorType type) {
    VkDescriptorBufferInfo &info =
        bufferInfos.emplace_back(VkDescriptorBufferInfo{
            .buffer = buffer,
            .offset = offset,
            .range = size,
        });

    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = VK_NULL_HANDLE;
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pBufferInfo = &info;
    writes.push_back(write);
}
