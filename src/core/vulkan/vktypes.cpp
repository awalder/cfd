#include "core/vulkan/vktypes.h"

#include <fmt/core.h>

namespace app::vk
{

Texture::~Texture()
{
    clean();
}

Texture::Texture(Texture&& other) noexcept
    : info(other.info)
    , device(other.device)
    , extent(other.extent)
    , image(other.image)
    , layout(other.layout)
    , view(other.view)
    , sampler(other.sampler)
    , memory(other.memory)
    , allocator(other.allocator)
{
    other.allocator = VK_NULL_HANDLE;
    other.device = VK_NULL_HANDLE;
    other.extent = {0, 0};
    other.image = VK_NULL_HANDLE;
    other.layout = VK_IMAGE_LAYOUT_UNDEFINED;
    other.memory = VK_NULL_HANDLE;
    other.sampler = VK_NULL_HANDLE;
    other.view = VK_NULL_HANDLE;
}

auto Texture::operator=(Texture&& other) noexcept -> Texture&
{
    if(this != &other)
    {
        clean(); // clean up the current resource

        // Move the resources from the other object
        allocator = other.allocator;
        device = other.device;
        extent = other.extent;
        image = other.image;
        info = other.info;
        layout = other.layout;
        memory = other.memory;
        sampler = other.sampler;
        view = other.view;

        // Set the other object's resource pointers to VK_NULL_HANDLE
        other.allocator = VK_NULL_HANDLE;
        other.device = VK_NULL_HANDLE;
        other.extent = {0, 0};
        other.image = VK_NULL_HANDLE;
        other.layout = VK_IMAGE_LAYOUT_UNDEFINED;
        other.memory = VK_NULL_HANDLE;
        other.sampler = VK_NULL_HANDLE;
        other.view = VK_NULL_HANDLE;
    }

    return *this;
}

auto Texture::clean() -> void
{
    if(image)
    {
        vmaDestroyImage(allocator, image, memory);
        image = VK_NULL_HANDLE;
        memory = VK_NULL_HANDLE;
        extent = {0, 0};
    }
    if(sampler)
    {
        vkDestroySampler(device, sampler, nullptr);
        sampler = VK_NULL_HANDLE;
    }
    if(view)
    {
        vkDestroyImageView(device, view, nullptr);
        view = VK_NULL_HANDLE;
    }
}

auto Texture::getImageInfo() -> VkDescriptorImageInfo
{
    info.sampler = sampler;
    info.imageView = view;
    info.imageLayout = layout;

    return info;
}

Buffer::Buffer(Buffer&& other) noexcept
    : buffer(other.buffer)
    , info(other.info)
    , size(other.size)
    , flags(other.flags)
    , memory(other.memory)
    , allocator(other.allocator)
    , usage(other.usage)
    , mapped(other.mapped)
{
    other.buffer = VK_NULL_HANDLE;
    other.memory = VK_NULL_HANDLE;
    other.mapped = nullptr;
}

auto Buffer::operator=(Buffer&& other) noexcept -> Buffer&
{
    if(this != &other)
    {
        clean(); // clean up the current resource

        // Move the resources from the other object
        allocator = other.allocator;
        size = other.size;
        buffer = other.buffer;
        memory = other.memory;
        mapped = other.mapped;
        info = other.info;
        flags = other.flags;
        usage = other.usage;

        // Set the other object's resource pointers to VK_NULL_HANDLE
        other.buffer = VK_NULL_HANDLE;
        other.memory = VK_NULL_HANDLE;
        other.mapped = nullptr;
    }
    return *this;
}

Buffer::~Buffer()
{
    clean();
}

void Buffer::clean()
{
    if(mapped)
    {
        unmap();
    }

    if(buffer)
    {
        vmaDestroyBuffer(allocator, buffer, memory);
        buffer = VK_NULL_HANDLE;
        memory = VK_NULL_HANDLE;
        mapped = nullptr;
    }
}

auto Buffer::map() -> VkResult
{
    return vmaMapMemory(allocator, memory, &mapped);
}

auto Buffer::unmap() -> void
{
    vmaUnmapMemory(allocator, memory);
    mapped = nullptr;
}

auto Buffer::copyData(void const* data, VkDeviceSize dataSize, VkDeviceSize offset) -> void
{
    if(dataSize + offset > this->size)
    {
        throw std::runtime_error(fmt::format(
                "Data size {} + offset {} is larger than buffer size {}",
                dataSize,
                offset,
                this->size));
    }

    if(map() != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to map buffer");
    }

    std::memcpy(mapped, data, dataSize);
    unmap();
}
} // namespace app::vk
