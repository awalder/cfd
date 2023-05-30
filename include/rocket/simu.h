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
    glm::vec2 velocity = glm::vec2(0.0f);
    float density = 0.0f;
    int isSolid = 0;
    std::array<float, 9> distribution{};
    int pad = 0;
};

struct ComputeUniformBuffer
{
    glm::vec4 color = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
    glm::ivec2 gridSize;
    float time = 0;
    int enabled = 0;
};

struct ComputePushConstant
{
    uint32_t readBufferOffset = 0;
    uint32_t writeBufferOffset = 0;
};

class Simu
{ // Lattice velocities for each direction in the 2DQ9 model
    const std::array<glm::vec2, 9> ei = {
            glm::vec2(0, 0),   // rest
            glm::vec2(1, 0),   // right
            glm::vec2(0, 1),   // top
            glm::vec2(-1, 0),  // left
            glm::vec2(0, -1),  // bottom
            glm::vec2(1, 1),   // top-right
            glm::vec2(-1, 1),  // top-left
            glm::vec2(-1, -1), // bottom-left
            glm::vec2(1, -1)   // bottom-right
    };

    // Weights for each direction in the 2DQ9 model
    const std::array<float, 9> wi = {
            4.0f / 9.0f,  // rest
            1.0f / 9.0f,  // right
            1.0f / 9.0f,  // top
            1.0f / 9.0f,  // left
            1.0f / 9.0f,  // bottom
            1.0f / 36.0f, // top-right
            1.0f / 36.0f, // top-left
            1.0f / 36.0f, // bottom-left
            1.0f / 36.0f  // bottom-right
    };

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
    auto update(float time, float elapsed, uint32_t index) -> void;
    [[nodiscard]] auto getGridSize() const -> glm::ivec2 { return _grid.size; }
    // [[nodiscard]] auto getGridBufferInfo() const -> VkDescriptorBufferInfo;
    [[nodiscard]] auto getRenderImageInfo() -> VkDescriptorImageInfo;

private:
    auto generateGrid() -> void;
    auto createUniformBuffers() -> void;
    auto createRenderTarget() -> void;
    auto AllocateCommandBuffer(uint32_t count) -> void;
    auto setupDescriptors(uint32_t count) -> void;
    auto createComputePipeline() -> void;
    auto equilibriumDistribution(size_t i, float rho, glm::vec2 u) -> float;

private:
    vk::Device* _device = nullptr;
    ComputeUniformBuffer _ubo;
    std::shared_ptr<app::vk::DescriptorSetGenerator> _descGen;

    vk::Buffer _uniformBuffer;

    // Texture for compute to draw on
    vk::Texture _texture;

    struct
    {
        std::vector<GridCell> data;

        uint32_t readBufferIndex = 0;
        uint32_t writeBufferIndex = 0;
        // This buffer contains data for both read and write. alternating between every frame read
        // and write indices are swapped.
        vk::Buffer buffers;
        glm::ivec2 size = {2048, 512};
    } _grid;

    struct
    {
        VkPipelineLayout layout = VK_NULL_HANDLE;

        // density step
        VkPipeline collision = VK_NULL_HANDLE;
        VkPipeline streaming = VK_NULL_HANDLE;
        VkPipeline boundary = VK_NULL_HANDLE;
        VkPipeline macro = VK_NULL_HANDLE;

        // velocity step
        // VkPipeline v_forces = VK_NULL_HANDLE;
        // VkPipeline v_diffuse = VK_NULL_HANDLE;
        // VkPipeline v_advect = VK_NULL_HANDLE;
        // VkPipeline v_project = VK_NULL_HANDLE;
        // VkPipeline v_divergence = VK_NULL_HANDLE;
        // VkPipeline v_update = VK_NULL_HANDLE;

        // render
        VkPipeline render = VK_NULL_HANDLE;
        std::vector<VkCommandBuffer> commandBuffers;
    } _compute;

    struct
    {
        VkDescriptorPool pool = VK_NULL_HANDLE;
        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
        std::vector<VkDescriptorSet> sets;
    } _descriptors;
};
} // namespace app::simu
