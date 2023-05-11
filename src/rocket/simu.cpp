#include "rocket/simu.h"

namespace app::simu
{

Simu::Simu(vk::Device* device) : _device(device), _extent({1024, 1024})
{
    createUniformBuffers();
    AllocateCommandBuffer();
    setupDescriptors();
    createComputePipeline();
    updateUniformBuffers();
}

auto Simu::createUniformBuffers() -> void
{
    VkDeviceSize bufferSize = sizeof(ComputeUniformBuffer);
    _device->createBuffer(
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VMA_MEMORY_USAGE_CPU_TO_GPU,
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
                    | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
            bufferSize,
            &_uboResources.buffer,
            &_uboResources.memory);

    VkDescriptorBufferInfo bufferInfo = {};
    bufferInfo.buffer = _uboResources.buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = VK_WHOLE_SIZE;
    _uboResources.info = bufferInfo;
}

auto Simu::AllocateCommandBuffer() -> void
{
}

auto Simu::setupDescriptors() -> void
{
}

auto Simu::createComputePipeline() -> void
{
}

auto Simu::updateUniformBuffers() -> void
{
}

auto Simu::recordCommandBuffer() -> VkCommandBuffer
{
    return VkCommandBuffer{};
}

} // namespace app::simu
