#include "rocket/simu.h"

#include "utils/vkutils.h"

#include "glm/glm.hpp"

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
    update(0.0f, 0.0f, 0);
}

Simu::~Simu()
{
    _uniformBuffer.clean();
    _grid.buffers.clean();

    if(_compute.layout)
        vkDestroyPipelineLayout(_device->getLogicalDevice(), _compute.layout, nullptr);
    if(_compute.collision)
        vkDestroyPipeline(_device->getLogicalDevice(), _compute.collision, nullptr);
    if(_compute.streaming)
        vkDestroyPipeline(_device->getLogicalDevice(), _compute.streaming, nullptr);
    if(_compute.boundary)
        vkDestroyPipeline(_device->getLogicalDevice(), _compute.boundary, nullptr);
    if(_compute.macro)
        vkDestroyPipeline(_device->getLogicalDevice(), _compute.macro, nullptr);

    if(_compute.render)
        vkDestroyPipeline(_device->getLogicalDevice(), _compute.render, nullptr);

    if(_descriptors.layout)
        vkDestroyDescriptorSetLayout(_device->getLogicalDevice(), _descriptors.layout, nullptr);

    if(_descriptors.pool)
        vkDestroyDescriptorPool(_device->getLogicalDevice(), _descriptors.pool, nullptr);
}

auto Simu::equilibriumDistribution(size_t i, float rho, glm::vec2 u) -> float
{
    float eu = glm::dot(ei.at(i), u);
    float u2 = glm::dot(u, u);

    return wi.at(i) * rho * (1.0f + 3.0f * eu + 4.5f * eu * eu - 1.5f * u2);
}

auto Simu::generateGrid() -> void
{
    // allocating size for TWO buffers (current and next) and one additional
    // buffer for temporary memory space
    auto count = _grid.size.x * _grid.size.y * 1;
    auto sizeInBytes = count * sizeof(GridCell);
    _grid.data.resize(count);

    glm::ivec2 cylinderCenter(_grid.size.x / 5 * 1, _grid.size.y / 2 * 1);
    // glm::ivec2 cylinderCenter2(_grid.size.x / 5 * 1, _grid.size.y / 5 * 2);
    // glm::ivec2 cylinderCenter3(_grid.size.x / 5 * 1, _grid.size.y / 5 * 3);
    // glm::ivec2 cylinderCenter4(_grid.size.x / 5 * 1, _grid.size.y / 5 * 4);
    float cylinderRadius = (float)_grid.size.y / 15.f;

    // Populate grid
    for(int i = 0; i < _grid.size.y; ++i)
    {
        for(int j = 0; j < _grid.size.x; ++j)
        {
            auto& cell = _grid.data.at(i * _grid.size.x + j);
            cell.velocity = glm::vec2(-0.001f, 0.0f);

            {
                glm::ivec2 cellPos(j, i);
                glm::vec2 tmp = cellPos - cylinderCenter;
                float distance = glm::length(tmp);
                if(distance <= cylinderRadius)
                {
                    cell.isSolid = 1;
                    // Use a smooth gradient for the density
                    // cell.density = 2.0f * (1.0f - distance / cylinderRadius);
                }
            }

            // {
            //     glm::ivec2 cellPos(j, i);
            //     glm::vec2 tmp = cellPos - cylinderCenter2;
            //     float distance = glm::length(tmp);
            //     if(distance <= cylinderRadius)
            //     {
            //         cell.isSolid = 1;
            //     }
            // }
            // {
            //     glm::ivec2 cellPos(j, i);
            //     glm::vec2 tmp = cellPos - cylinderCenter3;
            //     float distance = glm::length(tmp);
            //     if(distance <= cylinderRadius)
            //     {
            //         cell.isSolid = 1;
            //     }
            // }
            // {
            //     glm::ivec2 cellPos(j, i);
            //     glm::vec2 tmp = cellPos - cylinderCenter4;
            //     float distance = glm::length(tmp);
            //     if(distance <= cylinderRadius)
            //     {
            //         cell.isSolid = 1;
            //     }
            // }

            cell.density = 1.f;

            for(int k = 0; k < 9; ++k)
            {
                cell.distribution.at(k) = equilibriumDistribution(k, cell.density, cell.velocity);
            }
        }
    }

    _grid.buffers = _device->createBufferOnGPU(
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
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            VK_SHADER_STAGE_COMPUTE_BIT);

    _descriptors.pool = _descGen->generatePool(100);
    _descriptors.layout = _descGen->generateLayout();
    _descriptors.sets.resize(count);

    for(auto& set : _descriptors.sets)
    {
        set = _descGen->generateSet(_descriptors.pool, _descriptors.layout);
        _descGen->bind(set, 0, {_uniformBuffer.info});
        _descGen->bind(set, 1, {_grid.buffers.info});
        _descGen->bind(set, 2, {_texture.getImageInfo()});
    }

    _descGen->updateSetContents();
}

auto Simu::createComputePipeline() -> void
{

    VkPushConstantRange pushConstantRange = {};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(ComputePushConstant);

    VkPipelineLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.pNext = nullptr;
    layoutInfo.flags = 0;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &_descriptors.layout;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushConstantRange;

    VK_CHECK(vkCreatePipelineLayout(
            _device->getLogicalDevice(), &layoutInfo, nullptr, &_compute.layout));

    VkComputePipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = nullptr;
    pipelineInfo.flags = 0;
    pipelineInfo.layout = _compute.layout;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = 0;

    auto addPipeline = [&](std::string const& shaderName, VkPipeline& pipeline) {
        auto shaderInfo = _device->loadShaderFromFile(shaderName, VK_SHADER_STAGE_COMPUTE_BIT);

        pipelineInfo.stage = shaderInfo;
        VK_CHECK(vkCreateComputePipelines(
                _device->getLogicalDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline));

        vkDestroyShaderModule(_device->getLogicalDevice(), shaderInfo.module, nullptr);
    };

    addPipeline("data/shaders/collision.comp.spv", _compute.collision);
    addPipeline("data/shaders/streaming.comp.spv", _compute.streaming);
    addPipeline("data/shaders/boundary.comp.spv", _compute.boundary);
    addPipeline("data/shaders/macro.comp.spv", _compute.macro);

    addPipeline("data/shaders/cfd_render.comp.spv", _compute.render);
}

auto Simu::update(float time, float elapsed, uint32_t index) -> void
{
    _ubo.color = glm::vec4(0.5f, 0.5f, 1.0f, 1.0f);
    _ubo.gridSize = _grid.size;
    _ubo.time = time;
    _ubo.enabled = static_cast<int>(elapsed < 10.0f);

    _uniformBuffer.copyTo(_ubo);
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

    auto barrierInfo = VkBufferMemoryBarrier{};
    barrierInfo.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrierInfo.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrierInfo.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrierInfo.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrierInfo.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrierInfo.buffer = _grid.buffers.buffer;
    barrierInfo.offset = 0;
    barrierInfo.size = VK_WHOLE_SIZE;

    uint32_t N = _grid.size.x * _grid.size.y;
    static auto pc = ComputePushConstant{.readBufferOffset = 0, .writeBufferOffset = 0};
    static bool toggle = true;

    auto swap = [&]() {
        // pc.readBufferOffset = toggle ? 0 : N;
        // pc.writeBufferOffset = toggle ? N : 0;
        pc.readBufferOffset = 0;
        pc.writeBufferOffset = 0;
        toggle = !toggle;
    };

    auto barrier = [&](VkCommandBuffer buf) {
        vkCmdPipelineBarrier(
                buf,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0,
                0,
                nullptr,
                1,
                &barrierInfo,
                0,
                nullptr);
    };

    auto pushConstants = [&](VkCommandBuffer buf) {
        vkCmdPushConstants(
                buf,
                _compute.layout,
                VK_SHADER_STAGE_COMPUTE_BIT,
                0,
                sizeof(ComputePushConstant),
                &pc);
    };

    uint32_t const workgroupSizeX = 16;
    uint32_t const workgroupSizeY = 16;
    uint32_t const numGroupsX = (_grid.size.x + workgroupSizeX - 1) / workgroupSizeX;
    uint32_t const numGroupsY = (_grid.size.y + workgroupSizeY - 1) / workgroupSizeY;

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

    for(int i = 0; i < 4; ++i)
    {
        { // Collision
            vkCmdBindPipeline(buf, VK_PIPELINE_BIND_POINT_COMPUTE, _compute.collision);
            pushConstants(buf);
            vkCmdDispatch(buf, numGroupsX, numGroupsY, 1);
            barrier(buf);
            swap();
        }
        { // Streaming
            vkCmdBindPipeline(buf, VK_PIPELINE_BIND_POINT_COMPUTE, _compute.streaming);
            pushConstants(buf);
            vkCmdDispatch(buf, numGroupsX, numGroupsY, 1);
            barrier(buf);
            swap();
        }
        { // Boundary
            vkCmdBindPipeline(buf, VK_PIPELINE_BIND_POINT_COMPUTE, _compute.boundary);
            pushConstants(buf);
            vkCmdDispatch(buf, numGroupsX, numGroupsY, 1);
            barrier(buf);
            swap();
        }
        { // Macro
            vkCmdBindPipeline(buf, VK_PIPELINE_BIND_POINT_COMPUTE, _compute.macro);
            pushConstants(buf);
            vkCmdDispatch(buf, numGroupsX, numGroupsY, 1);
            barrier(buf);
            swap();
        }
    }

    // Render !!
    vkCmdBindPipeline(buf, VK_PIPELINE_BIND_POINT_COMPUTE, _compute.render);
    vkCmdPushConstants(
            buf, _compute.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstant), &pc);

    vkCmdDispatch(buf, numGroupsX, numGroupsY, 1);

    VK_CHECK(vkEndCommandBuffer(buf));

    return buf;
}

} // namespace app::simu
