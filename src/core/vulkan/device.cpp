#include "core/vulkan/device.h"

#include <cassert>
#include <fstream>
#include <set>
#include <sstream>

#include "logs/log.h"
#include "utils/vkutils.h"

namespace app::vk
{

Device::Device(VkPhysicalDevice gpu, GLFWwindow* window)
    : _log(logs::getLogger("VkDevice")), _window(window), _physicalDevice(gpu)
{
    assert(gpu);
    assert(window);

    vkGetPhysicalDeviceProperties(gpu, &_physicalDeviceProperties);
    vkGetPhysicalDeviceFeatures(gpu, &_physicalDeviceFeatures);
    vkGetPhysicalDeviceMemoryProperties(gpu, &_physicalDeviceMemoryProperties);

    uint32_t queueFamilyPropertyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyPropertyCount, nullptr);
    assert(queueFamilyPropertyCount > 0);
    _queueFamilyProperties.resize(queueFamilyPropertyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(
            gpu, &queueFamilyPropertyCount, _queueFamilyProperties.data());
}

Device::~Device()
{
    if(_commandPools.graphics)
    {
        vkDestroyCommandPool(_logicalDevice, _commandPools.graphics, nullptr);
    }
    if(_commandPools.compute)
    {
        vkDestroyCommandPool(_logicalDevice, _commandPools.compute, nullptr);
    }
    if(_commandPools.transfer)
    {
        vkDestroyCommandPool(_logicalDevice, _commandPools.transfer, nullptr);
    }

    vmaDestroyAllocator(_allocator);
    if(_logicalDevice)
    {
        vkDestroyDevice(_logicalDevice, nullptr);
    }
}

void Device::createLogicalDevice(
        VkInstance const& instance,
        std::vector<char const*> requestedExtensions,
        VkQueueFlags requestedQueueTypes)
{
    float const queuePriority[] = {1.0f};
    std::set<uint32_t> uniqueIndices = {};

    if(requestedQueueTypes & VK_QUEUE_GRAPHICS_BIT)
    {
        _queueFamilyIndices.graphics = getQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT);
        uniqueIndices.insert(_queueFamilyIndices.graphics);
        _log->info("Graphics Queue Family index({})", _queueFamilyIndices.graphics);
    }

    if(requestedQueueTypes & VK_QUEUE_COMPUTE_BIT)
    {
        _queueFamilyIndices.compute = getQueueFamilyIndex(VK_QUEUE_COMPUTE_BIT);
        uniqueIndices.insert(_queueFamilyIndices.compute);
        _log->info("Compute Queue Family index({})", _queueFamilyIndices.compute);
    }
    else
    {
        _queueFamilyIndices.compute = _queueFamilyIndices.graphics;
    }

    if(requestedQueueTypes & VK_QUEUE_TRANSFER_BIT)
    {
        _queueFamilyIndices.transfer = getQueueFamilyIndex(VK_QUEUE_TRANSFER_BIT);
        uniqueIndices.insert(_queueFamilyIndices.transfer);
        _log->info("Transfer Queue Family index({})", _queueFamilyIndices.transfer);
    }
    else
    {
        _queueFamilyIndices.transfer = _queueFamilyIndices.graphics;
    }

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos = {};
    for(uint32_t uniqueIndex : uniqueIndices)
    {
        auto queueInfo = VkDeviceQueueCreateInfo{};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.pNext = nullptr;
        queueInfo.flags = 0;
        queueInfo.queueFamilyIndex = uniqueIndex;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = queuePriority;
        queueCreateInfos.push_back(queueInfo);
    }

    VkPhysicalDeviceDescriptorIndexingFeaturesEXT indexingFeatures = {};
    indexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;
    indexingFeatures.pNext = nullptr;
    indexingFeatures.runtimeDescriptorArray = VK_TRUE;

    VkPhysicalDeviceFeatures2KHR requestedFeatures = {};
    requestedFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR;
    requestedFeatures.features.samplerAnisotropy = VK_TRUE;
    requestedFeatures.pNext = &indexingFeatures;

    requestedExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    VkDeviceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext = &indexingFeatures;
    createInfo.flags = 0;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.enabledLayerCount = 0;
    createInfo.ppEnabledLayerNames = nullptr;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(requestedExtensions.size());
    createInfo.ppEnabledExtensionNames = requestedExtensions.data();
    createInfo.pEnabledFeatures = &requestedFeatures.features;

    VK_CHECK(vkCreateDevice(_physicalDevice, &createInfo, nullptr, &_logicalDevice));

    assert(_physicalDevice);
    assert(_logicalDevice);
    // Create allocator
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = _physicalDevice;
    allocatorInfo.device = _logicalDevice;
    allocatorInfo.instance = instance;

    VK_CHECK(vmaCreateAllocator(&allocatorInfo, &_allocator));

    // Create commandpools and queues, assuming graphics is always required
    _commandPools.graphics = createCommandPool(
            _queueFamilyIndices.graphics, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    if(requestedQueueTypes & VK_QUEUE_COMPUTE_BIT)
    {
        _commandPools.compute = createCommandPool(
                _queueFamilyIndices.compute, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    }

    if(requestedQueueTypes & VK_QUEUE_TRANSFER_BIT)
    {
        _commandPools.transfer = createCommandPool(_queueFamilyIndices.transfer, 0);
    }

    vkGetDeviceQueue(_logicalDevice, _queueFamilyIndices.graphics, 0, &_queue.graphics);

    vkGetDeviceQueue(_logicalDevice, _queueFamilyIndices.graphics, 0, &_queue.present);

    vkGetDeviceQueue(_logicalDevice, _queueFamilyIndices.compute, 0, &_queue.compute);

    vkGetDeviceQueue(_logicalDevice, _queueFamilyIndices.transfer, 0, &_queue.transfer);
}

auto Device::getQueueFamilyIndex(VkQueueFlagBits queueFlags) const -> uint32_t
{
    assert(_queueFamilyProperties.size() > 0);
    // Try finding compute queue that is separate from graphics queue
    if(queueFlags & VK_QUEUE_COMPUTE_BIT)
    {
        for(size_t i = 0; i < _queueFamilyProperties.size(); ++i)
        {
            if((_queueFamilyProperties[i].queueFlags & queueFlags)
               && (_queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0)
            {
                return static_cast<uint32_t>(i);
            }
        }
    }

    if(queueFlags & VK_QUEUE_TRANSFER_BIT)
    {
        for(size_t i = 0; i < _queueFamilyProperties.size(); ++i)
        {
            if((_queueFamilyProperties[i].queueFlags & queueFlags)
               && (_queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0)
            {
                return static_cast<uint32_t>(i);
            }
        }
    }

    for(size_t i = 0; i < _queueFamilyProperties.size(); ++i)
    {
        if(_queueFamilyProperties[i].queueFlags & queueFlags)
        {
            return static_cast<uint32_t>(i);
        }
    }

    assert(false);
    return 0;
}

auto Device::createCommandBuffer(VkCommandBufferLevel level, VkQueueFlags queueType, bool begin)
        -> VkCommandBuffer
{
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;

    VkCommandBufferAllocateInfo allocateInfo = {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.pNext = nullptr;
    allocateInfo.level = level;
    allocateInfo.commandBufferCount = 1;
    switch(queueType)
    {
    case VK_QUEUE_GRAPHICS_BIT: allocateInfo.commandPool = _commandPools.graphics; break;
    case VK_QUEUE_COMPUTE_BIT: allocateInfo.commandPool = _commandPools.compute; break;
    case VK_QUEUE_TRANSFER_BIT: allocateInfo.commandPool = _commandPools.transfer; break;
    default: _log->warn("Unsupported queue type"); assert(false);
    }

    VK_CHECK(vkAllocateCommandBuffers(_logicalDevice, &allocateInfo, &commandBuffer));

    if(begin)
    {
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.pNext = nullptr;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        beginInfo.pInheritanceInfo = nullptr;
        VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));
    }

    return commandBuffer;
}

void Device::beginCommandBuffer(VkCommandBuffer commandBuffer, VkCommandBufferUsageFlags flags)
{
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.pNext = nullptr;
    beginInfo.flags = flags;
    beginInfo.pInheritanceInfo = nullptr;
    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));
}

void Device::flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free)
{
    VK_CHECK(vkEndCommandBuffer(commandBuffer));

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = nullptr;
    submitInfo.waitSemaphoreCount = 0;
    submitInfo.pWaitSemaphores = nullptr;
    submitInfo.pWaitDstStageMask = nullptr;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    submitInfo.signalSemaphoreCount = 0;
    submitInfo.pSignalSemaphores = nullptr;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.pNext = nullptr;
    fenceInfo.flags = 0;

    VkFence fence = VK_NULL_HANDLE;
    VK_CHECK(vkCreateFence(_logicalDevice, &fenceInfo, nullptr, &fence));

    VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, fence));

    VK_CHECK(vkWaitForFences(_logicalDevice, 1, &fence, VK_TRUE, UINT64_MAX));

    vkDestroyFence(_logicalDevice, fence, nullptr);

    if(free)
    {
        // TODO: cmdbuf may be alloceted from other than graphics pool
        vkFreeCommandBuffers(_logicalDevice, _commandPools.graphics, 1, &commandBuffer);
    }
}

void Device::createBuffer(
        VkBufferUsageFlags usage,
        VmaMemoryUsage vmaUsage,
        VkMemoryPropertyFlags properties,
        VkDeviceSize size,
        VkBuffer* buffer,
        VmaAllocation* bufferMemory,
        void* srcData)
{
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.pNext = nullptr;
    bufferInfo.flags = 0;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    bufferInfo.queueFamilyIndexCount = 0;
    bufferInfo.pQueueFamilyIndices = nullptr;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = vmaUsage;
    allocInfo.preferredFlags = properties;

    VK_CHECK(vmaCreateBuffer(_allocator, &bufferInfo, &allocInfo, buffer, bufferMemory, nullptr));

    if(srcData)
    {
        void* dstData = nullptr;
        vmaMapMemory(_allocator, *bufferMemory, &dstData);
        memcpy(dstData, srcData, size);
        vmaUnmapMemory(_allocator, *bufferMemory);
        if((properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
        {
            vmaFlushAllocation(_allocator, *bufferMemory, 0, size);
        }
    }
}

void Device::createBufferOnGPU(
        VkBufferUsageFlags usage,
        VkDeviceSize size,
        VkBuffer* buffer,
        VmaAllocation* bufferMemory,
        void* data)
{
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VmaAllocation stagingBufferMemory = VK_NULL_HANDLE;

    createBuffer(
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VMA_MEMORY_USAGE_CPU_ONLY,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            size,
            &stagingBuffer,
            &stagingBufferMemory,
            data);

    createBuffer(
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage,
            VMA_MEMORY_USAGE_GPU_ONLY,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            size,
            buffer,
            bufferMemory);

    copyBuffer(stagingBuffer, *buffer, size);
    vmaDestroyBuffer(_allocator, stagingBuffer, stagingBufferMemory);
}

void Device::createImageOnGPU(
        VkImageUsageFlags usage,
        VkDeviceSize size,
        VkFormat format,
        VkImageLayout layout,
        VkExtent2D extent,
        VkImage* image,
        VmaAllocation* imageMemory,
        void* data)
{
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VmaAllocation stagingBufferMemory = VK_NULL_HANDLE;

    createBuffer(
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VMA_MEMORY_USAGE_CPU_ONLY,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            size,
            &stagingBuffer,
            &stagingBufferMemory,
            data);

    createImage(
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | usage,
            VMA_MEMORY_USAGE_GPU_ONLY,
            format,
            VK_IMAGE_TILING_OPTIMAL,
            extent,
            image,
            imageMemory);

    VkCommandBuffer cmdBuf = createCommandBuffer();
    VkImageMemoryBarrier imageBarrier = {};
    {
        imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageBarrier.pNext = nullptr;
        imageBarrier.srcAccessMask = 0;
        imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageBarrier.image = *image;
        imageBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdPipelineBarrier(
                cmdBuf,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                0,
                0,
                nullptr,
                0,
                nullptr,
                1,
                &imageBarrier);
    }

    {
        VkBufferImageCopy copyRegion = {};
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0;
        copyRegion.bufferImageHeight = 0;
        copyRegion.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copyRegion.imageOffset = {0, 0, 0};
        copyRegion.imageExtent = {extent.width, extent.height, 1};

        vkCmdCopyBufferToImage(
                cmdBuf, stagingBuffer, *image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
    }
    {

        imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageBarrier.newLayout = layout;
        vkCmdPipelineBarrier(
                cmdBuf,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0,
                0,
                nullptr,
                0,
                nullptr,
                1,
                &imageBarrier);
    }

    flushCommandBuffer(cmdBuf, _queue.graphics, true);
    vmaDestroyBuffer(_allocator, stagingBuffer, stagingBufferMemory);
}

void Device::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
    VkBufferCopy copyRegion = {};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = size;

    VkCommandBuffer commandBuffer =
            createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, VK_QUEUE_TRANSFER_BIT);
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
    flushCommandBuffer(commandBuffer, _queue.transfer, false);
    vkFreeCommandBuffers(_logicalDevice, _commandPools.transfer, 1, &commandBuffer);
}

void Device::copyBufferToImage(
        VkBuffer srcBuffer,
        VkImage dstImage,
        VkExtent2D extent,
        VkImageLayout layout,
        VkDeviceSize size)
{

    (void)layout;
    (void)size;

    VkBufferImageCopy copyRegion = {};
    copyRegion.bufferOffset = 0;
    // copyRegion.bufferRowLength;
    // copyRegion.bufferImageHeight;
    copyRegion.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copyRegion.imageOffset = {0, 0, 0};
    copyRegion.imageExtent = {extent.width, extent.height, 1};

    VkCommandBuffer commandBuffer =
            createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, VK_QUEUE_TRANSFER_BIT);
    vkCmdCopyBufferToImage(
            commandBuffer,
            srcBuffer,
            dstImage,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &copyRegion);

    flushCommandBuffer(commandBuffer, _queue.transfer, false);
    vkFreeCommandBuffers(_logicalDevice, _commandPools.transfer, 1, &commandBuffer);
}

auto Device::createCommandPool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags poolFlags) const
        -> VkCommandPool
{
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkCommandPoolCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = poolFlags;
    createInfo.queueFamilyIndex = queueFamilyIndex;
    VK_CHECK(vkCreateCommandPool(_logicalDevice, &createInfo, nullptr, &commandPool));
    return commandPool;
}

void Device::createImageView(
        VkImage image, VkFormat format, VkImageAspectFlags aspect, VkImageView* view)
{
    VkImageViewCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.flags = 0;
    createInfo.image = image;
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format = format;
    createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.subresourceRange = {aspect, 0, 1, 0, 1};

    VK_CHECK(vkCreateImageView(_logicalDevice, &createInfo, nullptr, view));
}

void Device::createImage(
        VkImageUsageFlags usage,
        VmaMemoryUsage vmaMemoryUsage,
        VkFormat format,
        VkImageTiling tiling,
        VkExtent2D extent,
        VkImage* image,
        VmaAllocation* memory,
        VmaAllocationCreateFlags vmaFlags,
        VkImageCreateFlags imageFlags,
        uint32_t mipLevels,
        uint32_t arrayLayers)
{
    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.pNext = nullptr;
    imageInfo.flags = imageFlags;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = format;
    imageInfo.extent.width = extent.width;
    imageInfo.extent.height = extent.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = mipLevels;
    imageInfo.arrayLayers = arrayLayers;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = tiling;
    imageInfo.usage = usage;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    // imageInfo.queueFamilyIndexCount;
    // imageInfo.pQueueFamilyIndices;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.flags = vmaFlags;
    allocInfo.usage = vmaMemoryUsage;

    VK_CHECK(vmaCreateImage(_allocator, &imageInfo, &allocInfo, image, memory, nullptr));
}

void Device::createSampler(VkSampler* sampler)
{
    VkSamplerCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.magFilter = VK_FILTER_LINEAR;
    createInfo.minFilter = VK_FILTER_LINEAR;
    createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    createInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    createInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    createInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    createInfo.mipLodBias = 0.0f;
    createInfo.anisotropyEnable = VK_TRUE;
    createInfo.maxAnisotropy = 16.0f;
    createInfo.compareEnable = VK_FALSE;
    createInfo.compareOp = VK_COMPARE_OP_NEVER;
    // createInfo.minLod;
    // createInfo.maxLod;
    // createInfo.borderColor;
    createInfo.unnormalizedCoordinates = VK_FALSE;

    VK_CHECK(vkCreateSampler(_logicalDevice, &createInfo, nullptr, sampler));
}

auto Device::loadShaderFromFile(std::string const& path, VkShaderStageFlagBits stage)
        -> VkPipelineShaderStageCreateInfo
{
    std::ifstream file(path.c_str(), std::ios::in | std::ios::binary);
    if(!file)
    {
        _log->warn("Unable to open shader file {}", path);
        assert(false);
        throw std::runtime_error("");
    }
    _log->info("Load shader {}", path);
    std::ostringstream oss;
    oss << file.rdbuf();
    auto const code = oss.str();

    VkPipelineShaderStageCreateInfo shaderStageInfo = {};
    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.pNext = nullptr;
    shaderStageInfo.stage = stage;
    shaderStageInfo.pName = "main";
    shaderStageInfo.pSpecializationInfo = nullptr;

    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<uint32_t const*>(code.c_str());

    VK_CHECK(vkCreateShaderModule(_logicalDevice, &createInfo, nullptr, &shaderStageInfo.module));

    return shaderStageInfo;
}

auto Device::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) -> uint32_t
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(_physicalDevice, &memProperties);

    for(uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
        if((typeFilter & (1 << i))
           && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }

    _log->warn("failed to find suitable memory type!");
    assert(false);
}
} // namespace app::vk
