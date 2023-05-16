#pragma once

#include "core/vulkan/device.h"
#include "glm/vec2.hpp"
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"

#include <vulkan/vulkan.h>

namespace app::simu
{
struct ComputeUniformBuffer
{
    glm::vec4 color = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
    glm::ivec2 size;
    float time = 0;
};

class Simu
{
public:
    Simu(vk::Device* device, uint32_t imageCount);
    ~Simu();

    auto clean() -> void;
    auto getImageInfo() -> VkDescriptorImageInfo { return _imageInfo; }
    auto recordCommandBuffer(uint32_t index) -> VkCommandBuffer;
    auto updateUniformBuffers(float time) -> void;

private:
    auto createUniformBuffers() -> void;
    auto AllocateCommandBuffer(uint32_t count) -> void;
    auto createTexture() -> void;
    auto setupDescriptors() -> void;
    auto createComputePipeline() -> void;

private:
    vk::Device* _device = nullptr;
    ComputeUniformBuffer _ubo;

    VkExtent2D _extent{};
    VkImage _image = VK_NULL_HANDLE;
    VkImageView _view = VK_NULL_HANDLE;
    VkImageLayout _layout{};
    VmaAllocation _memory = VK_NULL_HANDLE;
    VkDescriptorImageInfo _imageInfo{};
    VkSampler _sampler = VK_NULL_HANDLE;

    struct
    {
        VkBuffer buffer = VK_NULL_HANDLE;
        VmaAllocation memory = VK_NULL_HANDLE;
        VkDescriptorBufferInfo info{};
    } _uboResources;

    struct
    {
        VkPipelineLayout layout = VK_NULL_HANDLE;
        VkPipeline calculate = VK_NULL_HANDLE;
        std::vector<VkCommandBuffer> commandBuffers{};
    } _compute;

    struct
    {
        VkDescriptorPool pool = VK_NULL_HANDLE;
        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
        VkDescriptorSet set = VK_NULL_HANDLE;
    } _descriptors;
};
} // namespace app::simu
