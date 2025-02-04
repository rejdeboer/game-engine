#include "descriptor.h"
#include "types.h"

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
