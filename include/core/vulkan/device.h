#pragma once

#include "GLFW/glfw3.h"
#include "core/vulkan/vkmemalloc.h"
#include "core/vulkan/vktypes.h"
#include "logs/log.h"
#include "vulkan/vulkan.h"

#include <memory>
#include <string>
#include <vector>

namespace app::vk
{

class Device final
{
public:
    explicit Device(VkPhysicalDevice gpu, GLFWwindow* window);
    ~Device();

    Device() = delete;
    Device(Device const&) = delete;
    Device(Device&&) = delete;
    auto operator=(Device const&) -> Device& = delete;
    auto operator=(Device&&) -> Device& = delete;

    void createLogicalDevice(
            VkInstance const& instance,
            std::vector<char const*> requestedExtensions,
            VkQueueFlags requestedQueueTypes);

    [[nodiscard]] auto getGLFWwindow() const -> GLFWwindow* { return _window; }
    [[nodiscard]] auto getLogicalDevice() const -> VkDevice { return _logicalDevice; }
    [[nodiscard]] auto getPhysicalDevice() const -> VkPhysicalDevice { return _physicalDevice; }
    [[nodiscard]] auto getAllocator() const -> VmaAllocator { return _allocator; }

    [[nodiscard]] auto getGraphicsQueueFamily() const -> uint32_t
    {
        return _queueFamilyIndices.graphics;
    }
    [[nodiscard]] auto getComputeQueueFamily() const -> uint32_t
    {
        return _queueFamilyIndices.compute;
    }
    [[nodiscard]] auto getTransferQueueFamily() const -> uint32_t
    {
        return _queueFamilyIndices.transfer;
    }

    [[nodiscard]] auto getGraphicsQueue() const -> VkQueue { return _queue.graphics; }
    [[nodiscard]] auto getComputeQueue() const -> VkQueue { return _queue.compute; }
    [[nodiscard]] auto getPresentQueue() const -> VkQueue { return _queue.present; }
    [[nodiscard]] auto getTransferQueue() const -> VkQueue { return _queue.transfer; }

    [[nodiscard]] auto getGraphicsCommandPool() const -> VkCommandPool
    {
        return _commandPools.graphics;
    }
    [[nodiscard]] auto getComputeCommandPool() const -> VkCommandPool
    {
        return _commandPools.compute;
    }
    [[nodiscard]] auto getTransferCommandPool() const -> VkCommandPool
    {
        return _commandPools.transfer;
    }

    auto createCommandBuffer(
            VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            VkQueueFlags queueType = VK_QUEUE_GRAPHICS_BIT,
            bool begin = true) -> VkCommandBuffer;

    void beginCommandBuffer(VkCommandBuffer commandBuffer, VkCommandBufferUsageFlags flags = 0);

    void flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free = true);

    auto createBuffer(
            VkBufferUsageFlags usage,
            VmaMemoryUsage vmaUsage,
            VkMemoryPropertyFlags properties,
            VkDeviceSize size,
            void* data = nullptr) -> vk::Buffer;

    void createBuffer(
            VkBufferUsageFlags usage,
            VmaMemoryUsage vmaUsage,
            VkMemoryPropertyFlags properties,
            VkDeviceSize size,
            VkBuffer* buffer,
            VmaAllocation* bufferMemory,
            void* data = nullptr);

    void createBufferOnGPU(
            VkBufferUsageFlags usage,
            VkDeviceSize size,
            VkBuffer* buffer,
            VmaAllocation* bufferMemory,
            void* data);

    auto createBufferOnGPU(
            VkBufferUsageFlags usage,
            VkDeviceSize size,
            void* data) -> vk::Buffer;

    auto createImageOnGPU(
            VkImageUsageFlags usage,
            VkDeviceSize size,
            VkFormat format,
            VkImageLayout layout,
            VkExtent2D extent,
            void* data) -> vk::Texture;

    void createImageOnGPU(
            VkImageUsageFlags usage,
            VkDeviceSize size,
            VkFormat format,
            VkImageLayout layout,
            VkExtent2D extent,
            VkImage* image,
            VmaAllocation* imageMemory,
            void* data);

    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

    void copyBufferToImage(
            VkBuffer srcBuffer,
            VkImage dstImage,
            VkExtent2D extent,
            VkImageLayout layout,
            VkDeviceSize size);

    void createImageView(
            VkImage image, VkFormat format, VkImageAspectFlags aspect, VkImageView* view);

    void createImage(
            VkImageUsageFlags usage,
            VmaMemoryUsage vmaMemoryUsage,
            VkFormat format,
            VkImageTiling tiling,
            VkExtent2D extent,
            VkImage* image,
            VmaAllocation* memory,
            VmaAllocationCreateFlags vmaFlags = 0,
            VkImageCreateFlags imageFlags = 0,
            uint32_t mipLevels = 1,
            uint32_t arrayLayers = 1);

    auto createSampler() -> VkSampler;

    auto loadShaderFromFile(std::string const& path, VkShaderStageFlagBits stage)
            -> VkPipelineShaderStageCreateInfo;
    auto findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) -> uint32_t;

private:
    [[nodiscard]] auto getQueueFamilyIndex(VkQueueFlagBits queueFlags) const -> uint32_t;
    [[nodiscard]] auto createCommandPool(
            uint32_t queueFamilyIndex, VkCommandPoolCreateFlags poolFlags) const -> VkCommandPool;

private:
    logs::Log _log;
    [[maybe_unused]] GLFWwindow* _window;
    VmaAllocator _allocator = VK_NULL_HANDLE;
    VkDevice _logicalDevice = VK_NULL_HANDLE;
    VkPhysicalDevice _physicalDevice = VK_NULL_HANDLE;

    VkPhysicalDeviceProperties _physicalDeviceProperties = {};
    VkPhysicalDeviceFeatures _physicalDeviceFeatures = {};
    VkPhysicalDeviceMemoryProperties _physicalDeviceMemoryProperties = {};
    std::vector<VkQueueFamilyProperties> _queueFamilyProperties;

    struct
    {
        VkQueue graphics = VK_NULL_HANDLE;
        VkQueue compute = VK_NULL_HANDLE;
        VkQueue present = VK_NULL_HANDLE;
        VkQueue transfer = VK_NULL_HANDLE;
    } _queue;

    struct
    {
        uint32_t graphics = 0;
        uint32_t compute = 0;
        uint32_t transfer = 0;
    } _queueFamilyIndices;

    struct
    {
        VkCommandPool graphics = VK_NULL_HANDLE;
        VkCommandPool compute = VK_NULL_HANDLE;
        VkCommandPool transfer = VK_NULL_HANDLE;
    } _commandPools;
};
} // namespace app::vk
