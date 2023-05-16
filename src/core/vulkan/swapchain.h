#pragma once

#include "core/vulkan/device.h"
#include "vulkan/vulkan.h"

#include <vector>

namespace app::vk
{

class Swapchain final
{
public:
    Swapchain(VkInstance instance, Device* device);
    ~Swapchain();

    Swapchain(Swapchain const&) = delete;
    auto operator=(Swapchain const&) -> Swapchain& = delete;
    Swapchain(Swapchain&&) = delete;
    auto operator=(Swapchain&&) -> Swapchain& = delete;

    [[nodiscard]] auto getSurface() const -> VkSurfaceKHR { return _surface; }
    [[nodiscard]] auto getImageCount() const -> uint32_t { return _imageCount; }
    [[nodiscard]] auto getImageFormat() const -> VkFormat { return _surfaceFormat; }
    [[nodiscard]] auto getFrameBuffer(size_t i) const -> VkFramebuffer { return _frameBuffers[i]; }
    [[nodiscard]] auto getExtent() const -> VkExtent2D { return _extent; }

    [[nodiscard]] auto acquireNextImage(VkSemaphore presentCompleteSemaphore, uint32_t* imageIndex)
            -> VkResult;
    [[nodiscard]] auto queuePresent(VkQueue queue, uint32_t imageIndex, VkSemaphore* waitSemaphore)
            -> VkResult;
    void create(bool vsync);
    void createFrameBuffers(VkRenderPass renderpass);
    void destroyFrameBuffers();

    [[nodiscard]] auto getSurfaceCapabilities() const { return _surfaceCapabilities; }

private:
    logs::Log _log;
    Device* _device;
    VkInstance _instance;
    VkSwapchainKHR _swapchain = VK_NULL_HANDLE;
    VkSurfaceKHR _surface = VK_NULL_HANDLE;
    VkSurfaceCapabilitiesKHR _surfaceCapabilities = {};
    VkPresentModeKHR _presentMode = VK_PRESENT_MODE_FIFO_KHR;
    VkExtent2D _extent = {0, 0};
    VkFormat _surfaceFormat = VK_FORMAT_B8G8R8A8_SRGB;
    VkColorSpaceKHR _surfaceColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    uint32_t _imageCount = 0;

    struct
    {
        VkImage image = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VmaAllocation memory = VK_NULL_HANDLE;
        VkFormat format = VK_FORMAT_D32_SFLOAT;
    } _depth;

    std::vector<VkFramebuffer> _frameBuffers;
    std::vector<VkImage> _images;
    std::vector<VkImageView> _imageViews;
    std::vector<VkPresentModeKHR> _presentModeList;
    std::vector<VkSurfaceFormatKHR> _surfaceFormatList;
};

} // namespace app::vk
