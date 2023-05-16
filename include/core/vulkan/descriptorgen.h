#pragma once

#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

namespace app::vk
{

class DescriptorSetGenerator
{
public:
    DescriptorSetGenerator(VkDevice device);
    void addBinding(
            std::uint32_t binding,
            std::uint32_t descriptorCount,
            VkDescriptorType type,
            VkShaderStageFlags stageFlags,
            VkSampler* sampler = nullptr);

    auto generatePool(std::uint32_t maxSets = 1) -> VkDescriptorPool;
    auto generateLayout() -> VkDescriptorSetLayout;
    auto generateSet(VkDescriptorPool pool, VkDescriptorSetLayout layout) -> VkDescriptorSet;

    void bind(
            VkDescriptorSet set,
            std::uint32_t binding,
            std::vector<VkDescriptorBufferInfo> const& bufferInfos,
            bool overwriteold = false);
    void bind(
            VkDescriptorSet set,
            std::uint32_t binding,
            std::vector<VkDescriptorImageInfo> const& imageInfos,
            bool overwriteold = false);

    void updateSetContents();

    template<class T, std::uint32_t offset>
    struct WriteInfo
    {
        std::vector<VkWriteDescriptorSet> writeDescriptors;
        std::vector<std::vector<T>> contents;

        void setPointers()
        {
            for(std::size_t i = 0; i < writeDescriptors.size(); ++i)
            {
                T** dest = reinterpret_cast<T**>(
                        reinterpret_cast<std::uint8_t*>(&writeDescriptors[i]) + offset);
                *dest = contents[i].data();
            }
        }

        void bind(
                VkDescriptorSet set,
                std::uint32_t binding,
                VkDescriptorType type,
                std::vector<T> const& info,
                bool overwriteold = false)
        {
            VkWriteDescriptorSet descriptorWrite = {};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.pNext = nullptr;
            descriptorWrite.dstSet = set;
            descriptorWrite.dstBinding = binding;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorCount = static_cast<std::uint32_t>(info.size());
            descriptorWrite.descriptorType = type;
            descriptorWrite.pImageInfo = nullptr;
            descriptorWrite.pBufferInfo = nullptr;
            descriptorWrite.pTexelBufferView = nullptr;

            if(overwriteold)
            {
                for(std::size_t i = 0; i < writeDescriptors.size(); ++i)
                {
                    if(writeDescriptors[i].dstBinding == binding
                       && writeDescriptors[i].dstSet == set)
                    {
                        writeDescriptors[i] = descriptorWrite;
                        contents[i] = info;
                        return;
                    }
                }
            }

            // if(!overwriteold)
            {
                writeDescriptors.push_back(descriptorWrite);
                contents.push_back(info);
            }
        }
    };

private:
    VkDevice _device;

    std::unordered_map<std::uint32_t, VkDescriptorSetLayoutBinding> _bindings;
    WriteInfo<VkDescriptorBufferInfo, offsetof(VkWriteDescriptorSet, pBufferInfo)> _buffers;
    WriteInfo<VkDescriptorImageInfo, offsetof(VkWriteDescriptorSet, pImageInfo)> _images;
};

}; // namespace app::vk
