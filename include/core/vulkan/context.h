#pragma once

#include "GLFW/glfw3.h"
#include "common/appcontext.h"
#include "core/confighandler.h"
#include "core/vulkan/device.h"
#include "core/vulkan/vktypes.h"
#include "entt/signal/dispatcher.hpp"
#include "event/commonevents.h"
#include "event/sub.h"
#include "logs/log.h"
#include "model/vertex.h"
#include "rocket/simu.h"
#include "vkmemalloc.h"
#include "vulkan/vulkan.h"

namespace app::vk
{

class DebugUtils;
class Swapchain;
class DescriptorSetGenerator;

class Context final
{
public:
    Context(AppContextPtr const& appContext, GLFWwindow* window);
    ~Context();

    Context(Context const&) = delete;
    auto operator=(Context const&) -> Context& = delete;
    Context(Context&&) = delete;
    auto operator=(Context&&) -> Context& = delete;

    auto init(VkExtent2D swapchainExtent) -> void;
    auto renderFrame(float dt, float elapsed) -> void;
    auto deviceWaitIdle() -> void;

    auto onEvent(event::FrameBufferResizeEvent const& event) -> void;

private:
    auto generatePipelines() -> void;
    auto createInstance() -> void;
    [[nodiscard]] auto selectPhysicalDevice() -> VkPhysicalDevice;
    auto createSynchronizationPrimitives() -> void;
    auto createRenderPass() -> void;
    auto createGraphicsPipeline() -> void;
    auto allocateCommandBuffers() -> void;
    auto recordCommandBuffers(uint32_t nextImageIndex) -> void;
    auto createUniformBuffers() -> void;
    auto update(float dt, uint32_t imageIndex) -> void;
    auto cleanupSwapchain() -> void;
    auto recreateSwapchain() -> void;
    auto createVertexIndexBuffers() -> void;
    void setupDescriptors();

private:
    event::Subs<Context> _conn;
    AppContextCPtr _appContext;
    params::Params::VulkanConfig _config;

    logs::Log _log;

    VkInstance _instance = VK_NULL_HANDLE;
    VkExtent2D _swapchainExtent = {0, 0};
    uint32_t _frameIndex = 0;
    bool _frameBufferResized = false;

    std::unique_ptr<simu::Simu> _simu;

    GLFWwindow* _window = nullptr;
    std::unique_ptr<Device> _device;
    std::shared_ptr<DebugUtils const> _debugUtils;
    std::shared_ptr<Swapchain> _swapchain;

    std::vector<model::VertexPT> _vertices = {
            {{-1.0f, -1.0f, 0.0}, {0.0f, 0.0f}},
            {{-1.0f, 1.0f, 0.0}, {0.0f, 1.0f}},
            {{1.0f, 1.0f, 0.0}, {1.0f, 1.0f}},
            {{1.0f, -1.0f, 0.0}, {1.0f, 0.0f}}};

    std::vector<uint32_t> _indices{0, 1, 2, 0, 2, 3};

    struct
    {
        vk::Buffer vertexBuffer;
        vk::Buffer indexBuffer;
    } quad;

    struct
    {
        VkSemaphore graphics = VK_NULL_HANDLE;
        VkSemaphore compute = VK_NULL_HANDLE;
        VkFence fence = VK_NULL_HANDLE;
    } _sync;

    struct UniformBufferObject
    {
        float time;
    };

    VkRenderPass _renderpass = VK_NULL_HANDLE;
    VkPipelineLayout _pipelineLayout = VK_NULL_HANDLE;
    VkPipeline _pipeline = VK_NULL_HANDLE;

    std::vector<VkSemaphore> _presentCompleteSemaphores;
    std::vector<VkSemaphore> _renderingCompleteSemaphores;
    std::vector<VkFence> _fences;
    std::vector<VkFence> _imagesInFlight;
    std::vector<VkCommandBuffer> _renderingCommandBuffers;

    std::vector<vk::Buffer> _uniformBuffer;

    VkDescriptorPool _descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout _descSetLayout = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> _descriptorSet;

    std::shared_ptr<DescriptorSetGenerator> _descriptorSetGenerator;
};

} // namespace app::vk
