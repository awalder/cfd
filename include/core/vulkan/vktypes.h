#pragma once

#include "core/vulkan/vkmemalloc.h"

#include <cstring>
#include <fmt/core.h>
#include <stdexcept>
#include <vulkan/vulkan.h>

namespace app::vk
{

// clang-format off
template<typename T>
concept Container = requires(T a)
{
    { a.data() } -> std::same_as<typename T::value_type const*>;
    { a.size() } -> std::same_as<typename T::size_type>;
};
// clang-format on

struct Texture
{
    VkDescriptorImageInfo info = {};
    VkDevice device = VK_NULL_HANDLE;
    VkExtent2D extent = {};
    VkImage image = VK_NULL_HANDLE;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageView view = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    VmaAllocation memory = VK_NULL_HANDLE;
    VmaAllocator allocator = VK_NULL_HANDLE;

    explicit Texture() = default;
    Texture(Texture const&) = default;
    auto operator=(Texture const&) -> Texture& = default;
    Texture(Texture&&) noexcept;
    auto operator=(Texture&&) noexcept -> Texture&;
    ~Texture();

    auto clean() -> void;
    auto getImageInfo() -> VkDescriptorImageInfo;
};

struct Buffer
{
    explicit Buffer() = default;
    Buffer(Buffer const&) = default;
    auto operator=(Buffer const&) -> Buffer& = default;
    Buffer(Buffer&&) noexcept;
    auto operator=(Buffer&&) noexcept -> Buffer&;
    ~Buffer();

    auto clean() -> void;
    auto map() -> VkResult;
    auto unmap() -> void;

    template<Container C>
    auto copyTo(C const& data, VkDeviceSize offset = 0) -> void
    {
        auto dataSize = VkDeviceSize{data.size() * sizeof(C::value_type)};
        copyData(data.data(), dataSize, offset);
    }

    template<typename T>
    auto copyTo(T const& data, VkDeviceSize offset = 0) -> void
    {
        static_assert(!std::is_pointer_v<T>, "T must not be a pointer");
        auto dataSize = VkDeviceSize{sizeof(T)};
        copyData(&data, dataSize, offset);
    }

    VkBuffer buffer = VK_NULL_HANDLE;
    VkDescriptorBufferInfo info = {};
    VkDeviceSize size = 0;
    VkMemoryPropertyFlags flags = 0;
    VmaAllocation memory = VK_NULL_HANDLE;
    VmaAllocator allocator = VK_NULL_HANDLE;
    VmaMemoryUsage usage = VMA_MEMORY_USAGE_UNKNOWN;
    void* mapped = nullptr;

private:
    auto copyData(void const* data, VkDeviceSize dataSize, VkDeviceSize offset = 0) -> void;
};

} // namespace app::vk
