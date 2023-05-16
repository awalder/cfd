#include "core/vulkan/descriptorgen.h"
#include "utils/vkutils.h"

#include <algorithm>
#include <stdexcept>

namespace app::vk
{
DescriptorSetGenerator::DescriptorSetGenerator(VkDevice device) : _device(device)
{
}

auto DescriptorSetGenerator::addBinding(
        std::uint32_t binding,
        std::uint32_t descriptorCount,
        VkDescriptorType type,
        VkShaderStageFlags stageFlags,
        VkSampler* sampler) -> void
{
    VkDescriptorSetLayoutBinding b = {};
    b.binding = binding;
    b.descriptorType = type;
    b.descriptorCount = descriptorCount;
    b.stageFlags = stageFlags;
    b.pImmutableSamplers = sampler;

    if(auto [it, inserted] = _bindings.emplace(binding, b); !inserted)
    {
        throw std::logic_error("Binding reassignment");
    }
}

auto DescriptorSetGenerator::generatePool(std::uint32_t maxSets) -> VkDescriptorPool
{
    VkDescriptorPool pool = VK_NULL_HANDLE;

    std::vector<VkDescriptorPoolSize> poolSizes;
    poolSizes.reserve(_bindings.size());

    for(auto const& [index, bindingLayout] : _bindings)
    {
        poolSizes.push_back({bindingLayout.descriptorType, bindingLayout.descriptorCount * 5});
    }

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.pNext = nullptr;
    poolInfo.flags = 0;
    poolInfo.maxSets = maxSets;
    poolInfo.poolSizeCount = static_cast<std::uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();

    VK_CHECK(vkCreateDescriptorPool(_device, &poolInfo, nullptr, &pool));

    return pool;
}

auto DescriptorSetGenerator::generateLayout() -> VkDescriptorSetLayout
{
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;

    std::vector<VkDescriptorSetLayoutBinding> bindings;
    bindings.reserve(_bindings.size());

    for(auto const& [index, bindingLayout] : _bindings)
    {
        bindings.push_back(bindingLayout);
    }

    VkDescriptorSetLayoutCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.bindingCount = static_cast<std::uint32_t>(bindings.size());
    createInfo.pBindings = bindings.data();

    VK_CHECK(vkCreateDescriptorSetLayout(_device, &createInfo, nullptr, &layout));

    return layout;
}

auto DescriptorSetGenerator::generateSet(
        VkDescriptorPool pool, VkDescriptorSetLayout layout) -> VkDescriptorSet
{
    VkDescriptorSet set = VK_NULL_HANDLE;

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.pNext = nullptr;
    allocInfo.descriptorPool = pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VK_CHECK(vkAllocateDescriptorSets(_device, &allocInfo, &set));

    return set;
}

auto DescriptorSetGenerator::bind(
        VkDescriptorSet set,
        std::uint32_t binding,
        std::vector<VkDescriptorBufferInfo> const& bufferInfos,
        bool overwriteold) -> void
{
    _buffers.bind(set, binding, _bindings.at(binding).descriptorType, bufferInfos, overwriteold);
}

auto DescriptorSetGenerator::bind(
        VkDescriptorSet set,
        std::uint32_t binding,
        std::vector<VkDescriptorImageInfo> const& imageInfos,
        bool overwriteold) -> void
{
    _images.bind(set, binding, _bindings.at(binding).descriptorType, imageInfos, overwriteold);
}

auto DescriptorSetGenerator::updateSetContents() -> void
{
    _buffers.setPointers();
    _images.setPointers();

    vkUpdateDescriptorSets(
            _device,
            static_cast<std::uint32_t>(_buffers.writeDescriptors.size()),
            _buffers.writeDescriptors.data(),
            0,
            nullptr);

    vkUpdateDescriptorSets(
            _device,
            static_cast<std::uint32_t>(_images.writeDescriptors.size()),
            _images.writeDescriptors.data(),
            0,
            nullptr);
}

} // namespace app::vk
