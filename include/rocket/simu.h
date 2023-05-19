#pragma once

#include "core/vulkan/descriptorgen.h"
#include "core/vulkan/device.h"
#include "core/vulkan/vktypes.h"
#include "glm/vec2.hpp"
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"

#include <vulkan/vulkan.h>

namespace app::simu
{

struct GridCell
{
    glm::vec2 velocity;
    glm::vec2 externalForce;
    float pressure;
    float density;
    float temperature;
    int boundaryType;
};

struct ComputeUniformBuffer
{
    glm::vec4 color = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
    glm::ivec2 gridSize;
    float time = 0;
};

class Simu
{
public:
    Simu(Simu const&) = delete;
    Simu(Simu&&) = delete;
    auto operator=(Simu const&) -> Simu& = delete;
    auto operator=(Simu&&) -> Simu& = delete;

    Simu(vk::Device* device, uint32_t imageCount);
    ~Simu();

    auto clean() -> void;
    // auto getImageInfo() -> VkDescriptorImageInfo { return _imageInfo; }
    auto recordCommandBuffer(uint32_t index) -> VkCommandBuffer;
    auto update(float time, uint32_t index) -> void;
    [[nodiscard]] auto getGridSize() const -> glm::ivec2 { return _grid.size; }
    [[nodiscard]] auto getGridBufferInfo() const -> VkDescriptorBufferInfo;
    [[nodiscard]] auto getRenderImageInfo() -> VkDescriptorImageInfo;

private:
    auto generateGrid() -> void;
    auto createUniformBuffers() -> void;
    auto createRenderTarget() -> void;
    auto AllocateCommandBuffer(uint32_t count) -> void;
    auto setupDescriptors(uint32_t count) -> void;
    auto createComputePipeline() -> void;

private:
    vk::Device* _device = nullptr;
    ComputeUniformBuffer _ubo;
    std::shared_ptr<app::vk::DescriptorSetGenerator> _descGen;

    // VkExtent2D _extent{};
    // VkImage _image = VK_NULL_HANDLE;
    // VkImageView _view = VK_NULL_HANDLE;
    // VkImageLayout _layout{};
    // VmaAllocation _memory = VK_NULL_HANDLE;
    // VkDescriptorImageInfo _imageInfo{};
    // VkSampler _sampler = VK_NULL_HANDLE;
    vk::Buffer _uniformBuffer;

    // Texture for compute to draw on
    vk::Texture _texture;

    struct
    {
        std::vector<GridCell> data;

        // Two buffers, alternating between these after each update
        std::size_t currentBufferIndex = 0;
        std::vector<vk::Buffer> buffers;
        glm::ivec2 size = {1024, 1024};
    } _grid;

    struct
    {
        VkPipelineLayout layout = VK_NULL_HANDLE;
        VkPipeline calculate = VK_NULL_HANDLE;
        VkPipeline render = VK_NULL_HANDLE;
        std::vector<VkCommandBuffer> commandBuffers{};
    } _compute;

    struct
    {
        VkDescriptorPool pool = VK_NULL_HANDLE;
        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
        std::vector<VkDescriptorSet> sets;
    } _descriptors;
};
} // namespace app::simu
