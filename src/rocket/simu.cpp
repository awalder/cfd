#include "rocket/simu.h"

#include "utils/vkutils.h"

namespace app::simu
{

Simu::Simu(vk::Device* device, uint32_t imageCount) : _device(device)
{
    _descGen = std::make_shared<app::vk::DescriptorSetGenerator>(_device->getLogicalDevice());
    createUniformBuffers();
    createRenderTarget();
    generateGrid();
    AllocateCommandBuffer(imageCount);
    setupDescriptors(imageCount);
    createComputePipeline();
    update(0.0f, 0);
}

Simu::~Simu()
{
    _uniformBuffer.clean();
    for(auto& buffer : _grid.buffers)
        buffer.clean();

    if(_compute.layout)
        vkDestroyPipelineLayout(_device->getLogicalDevice(), _compute.layout, nullptr);
    if(_compute.calculate)
        vkDestroyPipeline(_device->getLogicalDevice(), _compute.calculate, nullptr);
    if(_compute.render)
        vkDestroyPipeline(_device->getLogicalDevice(), _compute.render, nullptr);

    if(_descriptors.layout)
        vkDestroyDescriptorSetLayout(_device->getLogicalDevice(), _descriptors.layout, nullptr);

    if(_descriptors.pool)
        vkDestroyDescriptorPool(_device->getLogicalDevice(), _descriptors.pool, nullptr);
}

auto Simu::generateGrid() -> void
{
    auto count = _grid.size.x * _grid.size.y;
    auto sizeInBytes = count * sizeof(GridCell);
    _grid.data.resize(count);

    for(auto& cell : _grid.data)
    {
        cell.velocity = glm::vec2(0.0f);
        cell.externalForce = glm::vec2(0.0f);
        cell.pressure = 1.0f;
        cell.density = 1.0f;
        cell.temperature = 20.0f;
        cell.boundaryType = 0;
    }

    _grid.buffers.resize(2);
    _grid.buffers[0] = _device->createBufferOnGPU(
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, sizeInBytes, _grid.data.data());
    _grid.buffers[1] = _device->createBufferOnGPU(
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, sizeInBytes, _grid.data.data());
}

auto Simu::createUniformBuffers() -> void
{
    VkDeviceSize bufferSize = sizeof(ComputeUniformBuffer);
    _uniformBuffer = _device->createBuffer(
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VMA_MEMORY_USAGE_CPU_TO_GPU,
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
            bufferSize);
}

auto Simu::createRenderTarget() -> void
{
    auto extent =
            VkExtent2D{static_cast<uint32_t>(_grid.size.x), static_cast<uint32_t>(_grid.size.y)};
    auto size = extent.width * extent.height * 4;

    _texture = _device->createImageOnGPU(
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            size,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_LAYOUT_GENERAL,
            extent,
            nullptr);

    _device->createImageView(
            _texture.image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, &_texture.view);

    _texture.sampler = _device->createSampler();
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

auto Simu::setupDescriptors(uint32_t count) -> void
{
    _descGen->addBinding(
            0, // binding
            1, // count
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            VK_SHADER_STAGE_COMPUTE_BIT);

    _descGen->addBinding(
            1, // binding
            1,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            VK_SHADER_STAGE_COMPUTE_BIT);

    _descGen->addBinding(
            2, // binding
            1,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            VK_SHADER_STAGE_COMPUTE_BIT);

    _descGen->addBinding(
            3, // binding
            1,
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            VK_SHADER_STAGE_COMPUTE_BIT);

    _descriptors.pool = _descGen->generatePool(100);
    _descriptors.layout = _descGen->generateLayout();
    _descriptors.sets.resize(count);

    for(auto& set : _descriptors.sets)
    {
        set = _descGen->generateSet(_descriptors.pool, _descriptors.layout);
        _descGen->bind(set, 0, {_uniformBuffer.info});
        _descGen->bind(set, 1, {_grid.buffers.at(0).info});
        _descGen->bind(set, 2, {_grid.buffers.at(1).info});
        _descGen->bind(set, 3, {_texture.getImageInfo()});
    }

    _descGen->updateSetContents();
}

auto Simu::createComputePipeline() -> void
{
    auto shaderInfo =
            _device->loadShaderFromFile("data/shaders/cfd.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);

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

    auto renderShaderInfo = _device->loadShaderFromFile(
            "data/shaders/cfd_render.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);
    pipelineInfo.stage = renderShaderInfo;

    VK_CHECK(vkCreateComputePipelines(
            _device->getLogicalDevice(),
            VK_NULL_HANDLE,
            1,
            &pipelineInfo,
            nullptr,
            &_compute.render));

    vkDestroyShaderModule(_device->getLogicalDevice(), shaderInfo.module, nullptr);
    vkDestroyShaderModule(_device->getLogicalDevice(), renderShaderInfo.module, nullptr);
}

auto Simu::update(float time, uint32_t index) -> void
{
    _ubo.color = glm::vec4(0.5f, 0.5f, 1.0f, 1.0f);
    _ubo.gridSize = _grid.size;
    _ubo.time = time;
    _uniformBuffer.copyTo(_ubo);

    auto index1 = _grid.currentBufferIndex % 2;
    auto index2 = (index1 + 1) % 2;
    _grid.currentBufferIndex += 1;

    auto descriptorWrites = std::array<VkWriteDescriptorSet, 2>{};

    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = _descriptors.sets.at(index);
    descriptorWrites[0].dstBinding = 1;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[0].pBufferInfo = &_grid.buffers.at(index1).info;

    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = _descriptors.sets.at(index);
    descriptorWrites[1].dstBinding = 2;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[1].pBufferInfo = &_grid.buffers.at(index2).info;

    vkUpdateDescriptorSets(
            _device->getLogicalDevice(),
            static_cast<uint32_t>(descriptorWrites.size()),
            descriptorWrites.data(),
            0,
            nullptr);
}

auto Simu::getGridBufferInfo() const -> VkDescriptorBufferInfo
{
    auto readIdx = _grid.currentBufferIndex % 2;
    return _grid.buffers.at(readIdx).info;
}

auto Simu::getRenderImageInfo() -> VkDescriptorImageInfo
{
    return _texture.getImageInfo();
}

auto Simu::recordCommandBuffer(uint32_t index) -> VkCommandBuffer
{
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

    auto* buf = _compute.commandBuffers.at(index);

    VK_CHECK(vkResetCommandBuffer(buf, 0));
    VK_CHECK(vkBeginCommandBuffer(buf, &beginInfo));


    vkCmdBindDescriptorSets(
            buf,
            VK_PIPELINE_BIND_POINT_COMPUTE,
            _compute.layout,
            0,
            1,
            &_descriptors.sets.at(index),
            0,
            nullptr);

    vkCmdBindPipeline(buf, VK_PIPELINE_BIND_POINT_COMPUTE, _compute.calculate);

    uint32_t const workgroupSizeX = 16;
    uint32_t const workgroupSizeY = 16;

    uint32_t const numGroupsX = (_grid.size.x + workgroupSizeX - 1) / workgroupSizeX;
    uint32_t const numGroupsY = (_grid.size.y + workgroupSizeY - 1) / workgroupSizeY;

    vkCmdDispatch(buf, numGroupsX, numGroupsY, 1);

    vkCmdBindPipeline(buf, VK_PIPELINE_BIND_POINT_COMPUTE, _compute.render);

    vkCmdDispatch(buf, numGroupsX, numGroupsY, 1);

    VK_CHECK(vkEndCommandBuffer(buf));

    return buf;
}

} // namespace app::simu
