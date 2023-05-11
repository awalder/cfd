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
};

class Simu
{
public:
    Simu(vk::Device* device);

    auto createUniformBuffers() -> void;
    auto AllocateCommandBuffer() -> void;
    auto setupDescriptors() -> void;
    auto createComputePipeline() -> void;
    auto updateUniformBuffers() -> void;
    auto recordCommandBuffer() -> VkCommandBuffer;

private:
    vk::Device* _device = nullptr;
    ComputeUniformBuffer _ubo;

    struct
    {
        VkBuffer buffer = VK_NULL_HANDLE;
        VmaAllocation memory = VK_NULL_HANDLE;
        VkDescriptorBufferInfo info{};
    } _uboResources;

    VkExtent2D _extent{};
    VkImage _image = VK_NULL_HANDLE;
    VkImageView _view = VK_NULL_HANDLE;
    VkImageLayout _layout{};
    VmaAllocation _memory = VK_NULL_HANDLE;
    VkDescriptorImageInfo _imageInfo{};
    VkSampler _sampler = VK_NULL_HANDLE;

    struct
    {
        VkPipelineLayout layout = VK_NULL_HANDLE;
        VkPipeline calculate = VK_NULL_HANDLE;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    } _compute;
};
} // namespace app::simu
