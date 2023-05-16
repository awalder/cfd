#include "rocket/simu.h"

#include "core/vulkan/descriptorgen.h"
#include "utils/vkutils.h"

namespace app::simu
{

Simu::Simu(vk::Device* device, uint32_t imageCount) : _device(device), _extent({1024, 1024})
{
    createTexture();
    createUniformBuffers();
    AllocateCommandBuffer(imageCount);
    setupDescriptors();
    createComputePipeline();
    updateUniformBuffers(0.0f);
}

Simu::~Simu()
{
    clean();

    if(_compute.layout)
        vkDestroyPipelineLayout(_device->getLogicalDevice(), _compute.layout, nullptr);
    if(_compute.calculate)
        vkDestroyPipeline(_device->getLogicalDevice(), _compute.calculate, nullptr);

    if(_uboResources.buffer)
        vmaDestroyBuffer(_device->getAllocator(), _uboResources.buffer, _uboResources.memory);

    if(_descriptors.layout)
        vkDestroyDescriptorSetLayout(_device->getLogicalDevice(), _descriptors.layout, nullptr);

    if(_descriptors.pool)
        vkDestroyDescriptorPool(_device->getLogicalDevice(), _descriptors.pool, nullptr);
}

void Simu::clean()
{
    if(_view)
        vkDestroyImageView(_device->getLogicalDevice(), _view, nullptr);
    if(_image)
        vmaDestroyImage(_device->getAllocator(), _image, _memory);
    if(_sampler)
        vkDestroySampler(_device->getLogicalDevice(), _sampler, nullptr);
}

auto Simu::createUniformBuffers() -> void
{
    VkDeviceSize bufferSize = sizeof(ComputeUniformBuffer);
    _device->createBuffer(
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VMA_MEMORY_USAGE_CPU_TO_GPU,
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
            bufferSize,
            &_uboResources.buffer,
            &_uboResources.memory);

    VkDescriptorBufferInfo bufferInfo = {};
    bufferInfo.buffer = _uboResources.buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = VK_WHOLE_SIZE;
    _uboResources.info = bufferInfo;
}

auto Simu::AllocateCommandBuffer(uint32_t count) -> void
{
    _compute.commandBuffers.resize(count);
    for(uint32_t i = 0; i < count; ++i)
    {
        _compute.commandBuffers[i] = _device->createCommandBuffer(
                VK_COMMAND_BUFFER_LEVEL_PRIMARY, VK_QUEUE_GRAPHICS_BIT, false);
    }
}

auto Simu::createTexture() -> void
{
    VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT
                              | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
    _layout = VK_IMAGE_LAYOUT_GENERAL;
    VkExtent2D extent = _extent;
    VkDeviceSize size = extent.width * extent.height * 4;

    std::vector<uint8_t> clearData(size);

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VmaAllocation stagingMemory = VK_NULL_HANDLE;

    _device->createBuffer(
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VMA_MEMORY_USAGE_CPU_ONLY,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            size,
            &stagingBuffer,
            &stagingMemory,
            clearData.data());

    _device->createImage(
            usage,
            VMA_MEMORY_USAGE_GPU_ONLY,
            format,
            VK_IMAGE_TILING_OPTIMAL,
            extent,
            &_image,
            &_memory);

    auto* cmdBuffer = _device->createCommandBuffer();

    auto barrier = VkImageMemoryBarrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = _image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    vkCmdPipelineBarrier(
            cmdBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &barrier);

    VkBufferImageCopy copyRegion = {};
    copyRegion.bufferOffset = 0;
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.mipLevel = 0;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageOffset = {0, 0, 0};
    copyRegion.imageExtent.width = extent.width;
    copyRegion.imageExtent.height = extent.height;
    copyRegion.imageExtent.depth = 1;

    vkCmdCopyBufferToImage(
            cmdBuffer, stagingBuffer, _image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;

    vkCmdPipelineBarrier(
            cmdBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &barrier);

    _device->flushCommandBuffer(cmdBuffer, _device->getGraphicsQueue());
    _device->createImageView(_image, format, VK_IMAGE_ASPECT_COLOR_BIT, &_view);
    vmaDestroyBuffer(_device->getAllocator(), stagingBuffer, stagingMemory);

    _sampler = _device->createSampler();

    {
        VkDescriptorImageInfo info = {};
        info.sampler = _sampler;
        info.imageView = _view;
        info.imageLayout = _layout;
        _imageInfo = info;
    }
}

auto Simu::setupDescriptors() -> void
{
    auto gen = app::vk::DescriptorSetGenerator(_device->getLogicalDevice());

    gen.addBinding(
            0, // binding
            1, // count
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            VK_SHADER_STAGE_COMPUTE_BIT);

    gen.addBinding(
            1, // binding
            1,
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            VK_SHADER_STAGE_COMPUTE_BIT);

    _descriptors.pool = gen.generatePool(100);
    _descriptors.layout = gen.generateLayout();

    _descriptors.set = gen.generateSet(_descriptors.pool, _descriptors.layout);

    {
        gen.bind(_descriptors.set, 0, {_uboResources.info});
        gen.bind(_descriptors.set, 1, {_imageInfo});
    }

    gen.updateSetContents();
}

auto Simu::createComputePipeline() -> void
{
    auto shaderInfo =
            _device->loadShaderFromFile("data/shaders/draw.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);

    VkPipelineLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.pNext = nullptr;
    layoutInfo.flags = 0;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &_descriptors.layout;
    layoutInfo.pushConstantRangeCount = 0;
    layoutInfo.pPushConstantRanges = nullptr;

    VK_CHECK(vkCreatePipelineLayout(
            _device->getLogicalDevice(), &layoutInfo, nullptr, &_compute.layout));

    VkComputePipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = nullptr;
    pipelineInfo.flags = 0;
    pipelineInfo.stage = shaderInfo;
    pipelineInfo.layout = _compute.layout;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = 0;

    VK_CHECK(vkCreateComputePipelines(
            _device->getLogicalDevice(),
            VK_NULL_HANDLE,
            1,
            &pipelineInfo,
            nullptr,
            &_compute.calculate));

    vkDestroyShaderModule(_device->getLogicalDevice(), shaderInfo.module, nullptr);
}

auto Simu::updateUniformBuffers(float time) -> void
{
    _ubo.color = glm::vec4(0.5f, 0.5f, 1.0f, 1.0f);
    _ubo.size = glm::ivec2(_extent.width, _extent.height);
    _ubo.time = time;

    void* data = nullptr;
    vmaMapMemory(_device->getAllocator(), _uboResources.memory, &data);
    std::memcpy(data, &_ubo, sizeof(ComputeUniformBuffer));
    vmaUnmapMemory(_device->getAllocator(), _uboResources.memory);
}

auto Simu::recordCommandBuffer(uint32_t index) -> VkCommandBuffer
{
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

    auto* buf = _compute.commandBuffers.at(index);

    VK_CHECK(vkResetCommandBuffer(buf, 0));
    VK_CHECK(vkBeginCommandBuffer(buf, &beginInfo));

    VkImageMemoryBarrier imageBarrier = {};
    imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageBarrier.pNext = nullptr;
    imageBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    imageBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    imageBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier.image = _image;
    imageBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    vkCmdPipelineBarrier(
            buf,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &imageBarrier);

    vkCmdBindPipeline(buf, VK_PIPELINE_BIND_POINT_COMPUTE, _compute.calculate);

    vkCmdBindDescriptorSets(
            buf,
            VK_PIPELINE_BIND_POINT_COMPUTE,
            _compute.layout,
            0,
            1,
            &_descriptors.set,
            0,
            nullptr);

    uint32_t const workgroupSizeX = 16;
    uint32_t const workgroupSizeY = 16;

    uint32_t const numGroupsX = (_extent.width + workgroupSizeX - 1) / workgroupSizeX;
    uint32_t const numGroupsY = (_extent.height + workgroupSizeY - 1) / workgroupSizeY;

    vkCmdDispatch(buf, numGroupsX, numGroupsY, 1);

    imageBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    imageBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(
            buf,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &imageBarrier);

    VK_CHECK(vkEndCommandBuffer(buf));

    return buf;
}

} // namespace app::simu
