#include "swapchain.h"

#include "logs/log.h"
#include "utils/vkutils.h"

#include <algorithm>
#include <cassert>

#include "GLFW/glfw3.h"

namespace app::vk
{

// ----------------------------------------------------------------------------
//
//

Swapchain::Swapchain(VkInstance instance, Device* device)
    : _log(logs::getLogger("VkSwapchain")), _device(device), _instance(instance)
{
    auto* window = _device->getGLFWwindow();
    assert(window);
    VK_CHECK(glfwCreateWindowSurface(_instance, window, nullptr, &_surface));

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            _device->getPhysicalDevice(), _surface, &_surfaceCapabilities);

    // TODO improve, take just type of T, not instance
    auto deviceQuery = [device = _device, surface = _surface]<typename T>(auto&& Func, T t) {
        uint32_t count = 0;
        Func(device->getPhysicalDevice(), surface, &count, nullptr);
        assert(count > 0);
        std::vector<decltype(t)> res(count);
        Func(device->getPhysicalDevice(), surface, &count, res.data());
        return res;
    };

    _presentModeList = deviceQuery(vkGetPhysicalDeviceSurfacePresentModesKHR, VkPresentModeKHR{});
    _surfaceFormatList = deviceQuery(vkGetPhysicalDeviceSurfaceFormatsKHR, VkSurfaceFormatKHR{});

    // uint32_t presentModeCount = 0;
    // vkGetPhysicalDeviceSurfacePresentModesKHR(
    // m_Device->getPhysicalDevice(),
    // m_Surface,
    // &presentModeCount,
    // nullptr);

    // assert(presentModeCount > 0);
    // m_PresentModeList.resize(presentModeCount);
    // vkGetPhysicalDeviceSurfacePresentModesKHR(
    // m_Device->getPhysicalDevice(),
    // m_Surface,
    // &presentModeCount,
    // m_PresentModeList.data());

    // uint32_t formatCount = 0;
    // vkGetPhysicalDeviceSurfaceFormatsKHR(
    // m_Device->getPhysicalDevice(), m_Surface, &formatCount, nullptr);
    // assert(formatCount > 0);

    // m_SurfaceFormatList.resize(formatCount);
    // vkGetPhysicalDeviceSurfaceFormatsKHR(
    // m_Device->getPhysicalDevice(),
    // m_Surface,
    // &formatCount,
    // m_SurfaceFormatList.data());
}

// ----------------------------------------------------------------------------
//
//

Swapchain::~Swapchain()
{
    if(_depth.view)
    {
        vkDestroyImageView(_device->getLogicalDevice(), _depth.view, nullptr);
    }
    if(_depth.image)
    {
        vmaDestroyImage(_device->getAllocator(), _depth.image, _depth.memory);
    }
    for(auto& framebuffer : _frameBuffers)
    {
        if(framebuffer)
        {
            vkDestroyFramebuffer(_device->getLogicalDevice(), framebuffer, nullptr);
        }
    }
    for(auto& view : _imageViews)
    {
        if(view)
        {
            vkDestroyImageView(_device->getLogicalDevice(), view, nullptr);
        }
    }
    if(_swapchain)
    {
        vkDestroySwapchainKHR(_device->getLogicalDevice(), _swapchain, nullptr);
    }
    if(_surface)
    {
        vkDestroySurfaceKHR(_instance, _surface, nullptr);
    }
}

// ----------------------------------------------------------------------------
//
//

VkResult Swapchain::acquireNextImage(VkSemaphore presentCompleteSemaphore, uint32_t* imageIndex)
{
    return vkAcquireNextImageKHR(
            _device->getLogicalDevice(),
            _swapchain,
            UINT64_MAX,
            presentCompleteSemaphore,
            VK_NULL_HANDLE,
            imageIndex);
}

// ----------------------------------------------------------------------------
//
//

VkResult Swapchain::queuePresent(VkQueue queue, uint32_t* imageIndex, VkSemaphore* waitSemaphore)
{
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = waitSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &_swapchain;
    presentInfo.pImageIndices = imageIndex;
    presentInfo.pResults = nullptr;

    return vkQueuePresentKHR(queue, &presentInfo);
}

// ----------------------------------------------------------------------------
//
//

void Swapchain::create(bool vsync)
{
    assert(_surface);

    // Update surfaceCapabilities after resize
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            _device->getPhysicalDevice(), _surface, &_surfaceCapabilities);

    uint32_t minImageCount = _surfaceCapabilities.minImageCount;
    _log->info("Swapchain minImageCount({})", minImageCount);

    // Search for preferred format, if not found, pick first one in the list.
    assert(_surfaceFormatList.size() > 0);
    const VkFormat preferredSurfaceFormat = VK_FORMAT_B8G8R8A8_UNORM;

    auto res = std::find_if(
            _surfaceFormatList.begin(),
            _surfaceFormatList.end(),
            [surfaceformat = preferredSurfaceFormat](auto const& format) {
                return format.format == surfaceformat;
            });

    if(res != _surfaceFormatList.end())
    {
        _surfaceFormat = res->format;
        _surfaceColorSpace = res->colorSpace;
    }
    else
    {
        _surfaceFormat = _surfaceFormatList[0].format;
        _surfaceColorSpace = _surfaceFormatList[0].colorSpace;
    }

    // FIFO is guaranteed by spec
    _presentMode = VK_PRESENT_MODE_FIFO_KHR;
    if(!vsync)
    {
        for(auto const& mode : _presentModeList)
        {
            if(mode == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                _presentMode = mode;
                minImageCount += 1;
                break;
            }
        }
    }

    auto extent = [this] {
        if(_surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        {
            _log->info(
                    "CurrentExtent({}, {})",
                    _surfaceCapabilities.currentExtent.width,
                    _surfaceCapabilities.currentExtent.height);
            return _surfaceCapabilities.currentExtent;
        }

        int width = 0, height = 0;
        glfwGetFramebufferSize(_device->getGLFWwindow(), &width, &height);

        VkExtent2D actualExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

        actualExtent.width = std::max(
                _surfaceCapabilities.minImageExtent.width,
                std::min(_surfaceCapabilities.maxImageExtent.width, actualExtent.width));

        actualExtent.height = std::max(
                _surfaceCapabilities.minImageExtent.height,
                std::min(_surfaceCapabilities.maxImageExtent.height, actualExtent.height));

        _log->info("ActualExtent({}, {})", actualExtent.width, actualExtent.height);

        return actualExtent;
    }();

    _extent = extent;

    // 0 is also valid, unlimited maximagecount
    assert(_surfaceCapabilities.maxImageCount == 0
           || _surfaceCapabilities.maxImageCount >= minImageCount);

    VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(
            _device->getPhysicalDevice(), _surfaceFormat, &formatProperties);

    if((formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT_KHR)
       || (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT))
    {
        imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    VkSwapchainKHR oldSwapchain = _swapchain;

    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.surface = _surface;
    createInfo.minImageCount = minImageCount;
    createInfo.imageFormat = _surfaceFormat;
    createInfo.imageColorSpace = _surfaceColorSpace;
    createInfo.imageExtent = _extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = imageUsage;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount = 1;
    createInfo.pQueueFamilyIndices = nullptr;
    createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = _presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = oldSwapchain;

    VK_CHECK(vkCreateSwapchainKHR(_device->getLogicalDevice(), &createInfo, nullptr, &_swapchain));

    // TODO: remove
    _imageCount = minImageCount;

    // Clean up old swapchain and associated views
    if(oldSwapchain)
    {
        for(auto& view : _imageViews)
        {
            vkDestroyImageView(_device->getLogicalDevice(), view, nullptr);
        }

        vkDestroySwapchainKHR(_device->getLogicalDevice(), oldSwapchain, nullptr);
    }

    // Get handles for new swapchain images
    vkGetSwapchainImagesKHR(_device->getLogicalDevice(), _swapchain, &_imageCount, nullptr);
    _images.resize(_imageCount);

    vkGetSwapchainImagesKHR(_device->getLogicalDevice(), _swapchain, &_imageCount, _images.data());

    // Create image views
    _imageViews.resize(_imageCount);
    for(uint32_t i = 0; i < _imageCount; ++i)
    {
        _device->createImageView(
                _images[i], _surfaceFormat, VK_IMAGE_ASPECT_COLOR_BIT, &_imageViews[i]);
    }

    // (re)create Depth buffer
    if(_depth.image)
    {
        vkDestroyImageView(_device->getLogicalDevice(), _depth.view, nullptr);
        vmaDestroyImage(_device->getAllocator(), _depth.image, _depth.memory);
    }

    _device->createImage(
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY,
            _depth.format,
            VK_IMAGE_TILING_OPTIMAL,
            _extent,
            &_depth.image,
            &_depth.memory,
            VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);

    _device->createImageView(_depth.image, _depth.format, VK_IMAGE_ASPECT_DEPTH_BIT, &_depth.view);

    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.pNext = nullptr;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = _depth.image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};

    VkCommandBuffer commandBuffer =
            _device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &barrier);

    _device->flushCommandBuffer(commandBuffer, _device->getGraphicsQueue(), true);
}

// ----------------------------------------------------------------------------
//
//

void Swapchain::createFrameBuffers(VkRenderPass renderpass)
{
    _frameBuffers.resize(_imageCount);

    VkFramebufferCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.renderPass = renderpass;
    createInfo.width = _extent.width;
    createInfo.height = _extent.height;
    createInfo.layers = 1;

    for(uint32_t i = 0; i < _imageCount; ++i)
    {
        std::array<VkImageView, 2> attachments = {_imageViews[i], _depth.view};
        createInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        createInfo.pAttachments = attachments.data();

        VK_CHECK(vkCreateFramebuffer(
                _device->getLogicalDevice(), &createInfo, nullptr, &_frameBuffers[i]));
    }
}

// ----------------------------------------------------------------------------
//
//

void Swapchain::destroyFrameBuffers()
{
    for(auto& fb : _frameBuffers)
    {
        vkDestroyFramebuffer(_device->getLogicalDevice(), fb, nullptr);
    }
}

} // namespace app::vk
