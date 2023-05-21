
#include "core/vulkan/context.h"
#include "GLFW/glfw3.h"
#include "core/vulkan/descriptorgen.h"
#include "debugutils.h"
#include "fmt/ranges.h"
#include "swapchain.h"
#include "utils/vkutils.h"

#include <stdexcept>
#include <vector>

namespace app::vk
{

Context::Context(AppContextPtr const& appContext, GLFWwindow* window)
    : _conn(appContext->getDispatcher(), this)
    , _appContext(appContext)
    , _config(appContext->getParamsStruct()->vulkanConfig)
    , _log(logs::getLogger("VkContext"))
    , _window(window)
{
    _conn.attach<event::FrameBufferResizeEvent>();
}

Context::~Context()
{
    vkDeviceWaitIdle(_device->getLogicalDevice());

    for(auto& buffer : _uniformBuffer)
    {
        buffer.clean();
    }

    if(_descSetLayout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(_device->getLogicalDevice(), _descSetLayout, nullptr);
    }

    if(_descriptorPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(_device->getLogicalDevice(), _descriptorPool, nullptr);
    }

    {
        if(_sync.graphics != VK_NULL_HANDLE)
        {
            vkDestroySemaphore(_device->getLogicalDevice(), _sync.graphics, nullptr);
        }
        if(_sync.compute != VK_NULL_HANDLE)
        {
            vkDestroySemaphore(_device->getLogicalDevice(), _sync.compute, nullptr);
        }
        if(_sync.fence != VK_NULL_HANDLE)
        {
            vkDestroyFence(_device->getLogicalDevice(), _sync.fence, nullptr);
        }

        quad.vertexBuffer.clean();
        quad.indexBuffer.clean();
    }

    if(_pipelineLayout)
    {
        vkDestroyPipelineLayout(_device->getLogicalDevice(), _pipelineLayout, nullptr);
    }
    if(_pipeline)
    {
        vkDestroyPipeline(_device->getLogicalDevice(), _pipeline, nullptr);
    }

    if(_renderpass)
    {
        vkDestroyRenderPass(_device->getLogicalDevice(), _renderpass, nullptr);
    }

    for(auto& fence : _fences)
    {
        vkDestroyFence(_device->getLogicalDevice(), fence, nullptr);
    }

    for(auto& semaphore : _renderingCompleteSemaphores)
    {
        vkDestroySemaphore(_device->getLogicalDevice(), semaphore, nullptr);
    }

    for(auto& semaphore : _presentCompleteSemaphores)
    {
        vkDestroySemaphore(_device->getLogicalDevice(), semaphore, nullptr);
    }

    _swapchain.reset();
    _descriptorSetGenerator.reset();
    _simu.reset();

    _device.reset();
    _debugUtils.reset();

    if(_instance)
    {
        vkDestroyInstance(_instance, nullptr);
    }
}

auto Context::onEvent(event::FrameBufferResizeEvent const& event) -> void
{
    _log->info("onEvent::FrameBufferResizeEvent ({}x{})", event.width, event.height);
    _frameBufferResized = true;
}

void Context::renderFrame(float dt, float elapsed)
{
    // Compute stage

    // vkWaitForFences(_device->getLogicalDevice(), 1, &_sync.fence, VK_TRUE, UINT64_MAX);
    // vkResetFences(_device->getLogicalDevice(), 1, &_sync.fence);
    vkWaitForFences(_device->getLogicalDevice(), 1, &_fences[_frameIndex], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex = 0;
    VkResult result =
            _swapchain->acquireNextImage(_presentCompleteSemaphores[_frameIndex], &imageIndex);
    if(result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        recreateSwapchain();
        return;
    }
    if(result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        _log->warn("renderFrame::acquireNextImage({}): ", imageIndex, utils::errorString(result));
        assert(false);
    }

    _simu->update(dt, elapsed, imageIndex);

    // TODO test if I can use _frameindex to work with commandbuffers
    //
    auto* computeCmdBuf = _simu->recordCommandBuffer(imageIndex);
    VkPipelineStageFlags computeWaitStages[] = {VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT};

    auto submitInfos = std::array<VkSubmitInfo, 2>{};
    submitInfos[0].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfos[0].waitSemaphoreCount = 1;
    submitInfos[0].pWaitSemaphores = &_presentCompleteSemaphores[_frameIndex];
    submitInfos[0].pWaitDstStageMask = computeWaitStages;
    submitInfos[0].commandBufferCount = 1;
    submitInfos[0].pCommandBuffers = &computeCmdBuf;
    submitInfos[0].signalSemaphoreCount = 1;
    submitInfos[0].pSignalSemaphores = &_sync.compute;

    // Now the rendering and presentation operations
    if(_imagesInFlight[imageIndex] != VK_NULL_HANDLE)
    {
        vkWaitForFences(
                _device->getLogicalDevice(), 1, &_imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
    }

    _imagesInFlight[imageIndex] = _fences[_frameIndex];

    update(dt, imageIndex);
    recordCommandBuffers(imageIndex);

    const std::vector<VkCommandBuffer> cmdBuffers = {_renderingCommandBuffers[imageIndex]};

    const std::vector<VkPipelineStageFlags> graphicsWaitStages = {
            VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    const std::vector<VkSemaphore> graphicsWaitSemaphores = {_sync.compute};

    const std::vector<VkSemaphore> graphicsSignalSemaphores = {
            _renderingCompleteSemaphores[_frameIndex]};

    submitInfos[1].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfos[1].pNext = nullptr;
    submitInfos[1].waitSemaphoreCount = static_cast<uint32_t>(graphicsWaitSemaphores.size());
    submitInfos[1].pWaitSemaphores = graphicsWaitSemaphores.data();
    submitInfos[1].pWaitDstStageMask = graphicsWaitStages.data();
    submitInfos[1].commandBufferCount = static_cast<uint32_t>(cmdBuffers.size());
    submitInfos[1].pCommandBuffers = cmdBuffers.data();
    submitInfos[1].signalSemaphoreCount = static_cast<uint32_t>(graphicsSignalSemaphores.size());
    submitInfos[1].pSignalSemaphores = graphicsSignalSemaphores.data();

    vkResetFences(_device->getLogicalDevice(), 1, &_fences[_frameIndex]);

    result = vkQueueSubmit(
            _device->getGraphicsQueue(),
            static_cast<uint32_t>(submitInfos.size()),
            submitInfos.data(),
            _fences[_frameIndex]);

    if(result != VK_SUCCESS)
    {
        _log->warn("renderFrame::vkQueueSubmit {}", utils::errorString(result));
        assert(false);
    }

    result = _swapchain->queuePresent(
            _device->getGraphicsQueue(), imageIndex, &_renderingCompleteSemaphores[_frameIndex]);

    if(result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR || _frameBufferResized)
    {
        _frameBufferResized = false;
        recreateSwapchain();
    }
    else if(result != VK_SUCCESS)
    {
        assert(false);
    }

    _frameIndex = (_frameIndex + 1) % _swapchain->getImageCount();
}

auto Context::deviceWaitIdle() -> void
{
    vkDeviceWaitIdle(_device->getLogicalDevice());
}

auto Context::init(VkExtent2D swapchainExtent) -> void
{
    if(swapchainExtent.width == 0 && swapchainExtent.height == 0)
    {
        throw std::runtime_error("Swapchain extent is not valid");
    }

    _swapchainExtent = swapchainExtent;

    createInstance();

    if(_config.debugUtils)
    {
        VkDebugUtilsMessageSeverityFlagsEXT messageSeverity =
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
                | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
        // | VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;

        VkDebugUtilsMessageTypeFlagsEXT messageType =
                VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

        if(_config.validationLayers)
            messageType |= VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;

        _debugUtils = std::make_shared<DebugUtils const>(_instance, messageSeverity, messageType);
    }

    // Look for GPU's
    VkPhysicalDevice gpu = selectPhysicalDevice();
    _device = std::make_unique<Device>(gpu, _window);

    {
        std::vector<char const*> requestedExtensions = {};

        VkQueueFlags queueFlags =
                VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;

        _device->createLogicalDevice(_instance, requestedExtensions, queueFlags);
    }

    // Create surface
    _swapchain = std::make_shared<Swapchain>(_instance, _device.get());
    _descriptorSetGenerator = std::make_shared<DescriptorSetGenerator>(_device->getLogicalDevice());

    // Check present support
    VkBool32 supportPresent = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(
            _device->getPhysicalDevice(),
            _device->getGraphicsQueueFamily(),
            _swapchain->getSurface(),
            &supportPresent);

    if(!supportPresent)
    {
        _log->critical("Window surface does not support presenting!");
        assert(false);
    }

    // m_Scene->loadModels(m_Device.get());
    _swapchain->create(_config.vsync);

    _simu = std::make_unique<simu::Simu>(_device.get(), _swapchain->getImageCount());

    createUniformBuffers();
    createSynchronizationPrimitives();
    createRenderPass();
    createVertexIndexBuffers();
    setupDescriptors();
    // setupDescriptorsAntSim();

    generatePipelines();
}

void Context::generatePipelines()
{
    createGraphicsPipeline();
    _swapchain->createFrameBuffers(_renderpass);
    allocateCommandBuffers();
    // initImGui();
}

auto Context::createInstance() -> void
{
    std::vector<char const*> instanceExtensions = {};
    std::vector<char const*> instanceLayers = {};

    { // GLFW should ask for VK_KHR_SURFACE and platform depended surface
        uint32_t count = 0;
        char const** glfwExtensions = nullptr;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&count);
        assert(count > 0);
        instanceExtensions = std::vector<char const*>(glfwExtensions, glfwExtensions + count);
    }

    _log->debug(
            "Instance extensions required({}) : {}", instanceExtensions.size(), instanceExtensions);

    if(_config.debugUtils)
    {
        _log->debug("DebugUtils enabled");
        instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    if(_config.validationLayers)
    {
        _log->debug("ValidationLayers enabled");
        // instanceLayers.push_back("VK_LAYER_LUNARG_standard_validation");
        instanceLayers.push_back("VK_LAYER_KHRONOS_validation");
    }

    { // Print instance layers
        uint32_t count = 0;
        vkEnumerateInstanceLayerProperties(&count, nullptr);
        auto layers = std::vector<VkLayerProperties>(count);
        vkEnumerateInstanceLayerProperties(&count, layers.data());

        _log->debug("Instance layers found({})", count);
        for(auto l : layers)
        {
            _log->debug("\t{}", l.layerName);
        }
    }
    { // Print instance extensions
        uint32_t count = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
        std::vector<VkExtensionProperties> extensions(count);
        vkEnumerateInstanceExtensionProperties(nullptr, &count, extensions.data());

        _log->debug("Instance Extensions found({})", count);
        for(auto v : extensions)
        {
            _log->debug("\t{}", v.extensionName);
        }
    }

    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pNext = nullptr;
    appInfo.pApplicationName = "RockerDynamics";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
    appInfo.pEngineName = "Light Wood Laminate";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 0, 1);
    appInfo.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledLayerCount = static_cast<uint32_t>(instanceLayers.size());
    createInfo.ppEnabledLayerNames = instanceLayers.data();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
    createInfo.ppEnabledExtensionNames = instanceExtensions.data();

    VK_CHECK(vkCreateInstance(&createInfo, nullptr, &_instance));
}

auto Context::selectPhysicalDevice() -> VkPhysicalDevice
{
    uint32_t physicalDeviceCount = 0;
    vkEnumeratePhysicalDevices(_instance, &physicalDeviceCount, nullptr);
    std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
    vkEnumeratePhysicalDevices(_instance, &physicalDeviceCount, physicalDevices.data());

    assert(physicalDevices.size() > 0);

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

    for(auto* const gpu : physicalDevices)
    {
        VkPhysicalDeviceProperties gpuProperties;
        vkGetPhysicalDeviceProperties(gpu, &gpuProperties);

        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(gpu, &memProperties);

        uint32_t queueFamilyPropertyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyPropertyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyPropertyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(
                gpu, &queueFamilyPropertyCount, queueFamilies.data());

        // Pick first available discrete GPU
        if(gpuProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            physicalDevice = gpu;
            break;
        }
    }

    if(physicalDevice == VK_NULL_HANDLE)
    {
        physicalDevice = physicalDevices[0];
    }

    _log->debug("**** GPU's and queues found in system  ****");
    _log->debug("GPU's found: {}", physicalDeviceCount);
    for(auto const& device : physicalDevices)
    {
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);
        _log->info("Device name: {}", deviceProperties.deviceName);

        VkPhysicalDeviceMemoryProperties memoryProperties;
        vkGetPhysicalDeviceMemoryProperties(device, &memoryProperties);

        uint32_t queueFamilyPropertyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyPropertyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyPropertyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(
                device, &queueFamilyPropertyCount, queueFamilyProperties.data());

        _log->debug("Queue families found: {}", queueFamilyPropertyCount);
        for(uint32_t i = 0; i < queueFamilyPropertyCount; ++i)
        {
            _log->debug("\tQueue family [{}] supports following operations:", i);
            if(queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                _log->debug("\t\tVK_QUEUE_GRAPHICS_BIT");
            }
            if(queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
            {
                _log->debug("\t\tVK_QUEUE_COMPUTE_BIT");
            }
            if(queueFamilyProperties[i].queueFlags & VK_QUEUE_TRANSFER_BIT)
            {
                _log->debug("\t\tVK_QUEUE_TRANSFER_BIT");
            }
            if(queueFamilyProperties[i].queueFlags & VK_QUEUE_SPARSE_BINDING_BIT)
            {
                _log->debug("\t\tVK_QUEUE_SPARSE_BINDING_BIT");
            }
        }
    }
    return physicalDevice;
}

auto Context::createSynchronizationPrimitives() -> void
{
    const uint32_t imageCount = _swapchain->getImageCount();
    _presentCompleteSemaphores.resize(imageCount);
    _renderingCompleteSemaphores.resize(imageCount);
    _fences.resize(imageCount);
    _imagesInFlight.resize(imageCount, VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreInfo.flags = 0;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    _log->debug("Creating {} semaphores and fences", imageCount);

    for(uint32_t i = 0; i < imageCount; ++i)
    {
        VK_CHECK(vkCreateSemaphore(
                _device->getLogicalDevice(),
                &semaphoreInfo,
                nullptr,
                &_presentCompleteSemaphores[i]));
        VK_CHECK(vkCreateSemaphore(
                _device->getLogicalDevice(),
                &semaphoreInfo,
                nullptr,
                &_renderingCompleteSemaphores[i]));
        VK_CHECK(vkCreateFence(_device->getLogicalDevice(), &fenceInfo, nullptr, &_fences[i]));
    }

    VK_CHECK(vkCreateSemaphore(
            _device->getLogicalDevice(), &semaphoreInfo, nullptr, &_sync.graphics));
    VK_CHECK(vkCreateSemaphore(
            _device->getLogicalDevice(), &semaphoreInfo, nullptr, &_sync.compute));

    VkFenceCreateInfo compfenceInfo = {};
    compfenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    compfenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VK_CHECK(vkCreateFence(_device->getLogicalDevice(), &compfenceInfo, nullptr, &_sync.fence));

    // need to signal the first semaphore in order to do the compute work
    // VkSubmitInfo submitInfo = {};
    // submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    // submitInfo.signalSemaphoreCount = 1;
    // submitInfo.pSignalSemaphores = &_renderingCompleteSemaphores[0];
    // VK_CHECK(vkQueueSubmit(_device->getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE));
    // VK_CHECK(vkQueueWaitIdle(_device->getGraphicsQueue()));
}

auto Context::createRenderPass() -> void
{
    std::array<VkAttachmentDescription, 2> attachments = {};
    attachments[0].flags = 0;
    attachments[0].format = _swapchain->getImageFormat();
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    attachments[1].flags = 0;
    attachments[1].format = VK_FORMAT_D32_SFLOAT;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    std::array<VkAttachmentReference, 2> attachmentReferences = {};
    attachmentReferences[0].attachment = 0;
    attachmentReferences[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachmentReferences[1].attachment = 1;
    attachmentReferences[1].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    std::array<VkSubpassDescription, 1> subpassDescriptions = {};
    subpassDescriptions[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescriptions[0].colorAttachmentCount = 1;
    subpassDescriptions[0].pColorAttachments = &attachmentReferences[0];
    subpassDescriptions[0].pDepthStencilAttachment = &attachmentReferences[1];

    std::array<VkSubpassDependency, 2> subpassDependencies = {};

    subpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    subpassDependencies[0].dstSubpass = 0;
    subpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    subpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    subpassDependencies[0].dstAccessMask =
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    subpassDependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    subpassDependencies[1].srcSubpass = 0;
    subpassDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    subpassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    subpassDependencies[1].srcAccessMask =
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    subpassDependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    subpassDependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo renderpassInfo = {};
    renderpassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderpassInfo.pNext = nullptr;
    renderpassInfo.flags = 0;
    renderpassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderpassInfo.pAttachments = attachments.data();
    renderpassInfo.subpassCount = static_cast<uint32_t>(subpassDescriptions.size());
    renderpassInfo.pSubpasses = subpassDescriptions.data();
    renderpassInfo.dependencyCount = static_cast<uint32_t>(subpassDependencies.size());
    renderpassInfo.pDependencies = subpassDependencies.data();

    VK_CHECK(vkCreateRenderPass(
            _device->getLogicalDevice(), &renderpassInfo, nullptr, &_renderpass));
}

auto Context::createGraphicsPipeline() -> void
{
    auto const bindingDescription = model::VertexPT::getBindingDescription();
    auto const attributeDescription = model::VertexPT::getAttributeDescription();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.pNext = nullptr;
    vertexInputInfo.flags = 0;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount =
            static_cast<uint32_t>(attributeDescription.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescription.data();

    // --

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.pNext = nullptr;
    inputAssembly.flags = 0;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // --

    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = static_cast<float>(_swapchainExtent.width);
    viewport.height = static_cast<float>(_swapchainExtent.height);
    ;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    // --

    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = _swapchainExtent;

    // --

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.pNext = nullptr;
    viewportState.flags = 0;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    // --

    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.pNext = nullptr;
    rasterizer.flags = 0;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f;
    rasterizer.depthBiasClamp = 0.0f;
    rasterizer.depthBiasSlopeFactor = 0.0f;
    rasterizer.lineWidth = 1.0f;

    // --

    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.pNext = nullptr;
    multisampling.flags = 0;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.minSampleShading = 1.0f;
    multisampling.pSampleMask = nullptr;
    multisampling.alphaToCoverageEnable = VK_FALSE;
    multisampling.alphaToOneEnable = VK_FALSE;

    // --

    VkPipelineDepthStencilStateCreateInfo depthInfo = {};
    depthInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthInfo.pNext = nullptr;
    depthInfo.flags = 0;
    depthInfo.depthTestEnable = VK_TRUE;
    depthInfo.depthWriteEnable = VK_TRUE;
    depthInfo.depthCompareOp = VK_COMPARE_OP_LESS;
    depthInfo.depthBoundsTestEnable = VK_FALSE;
    depthInfo.stencilTestEnable = VK_FALSE;
    // depthInfo.front;
    // depthInfo.back;
    depthInfo.minDepthBounds = 0.0f;
    depthInfo.maxDepthBounds = 1.0f;

    // --

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.blendEnable = VK_FALSE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.colorWriteMask = 0xF;

    // --

    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.pNext = nullptr;
    colorBlending.flags = 0;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    // colorBlending.blendConstants;

    // --

    std::array<VkDynamicState, 2> dynStates{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

    VkPipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.pNext = nullptr;
    dynamicState.flags = 0;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynStates.size());
    dynamicState.pDynamicStates = dynStates.data();

    // --

    VkPipelineLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.pNext = nullptr;
    layoutInfo.flags = 0;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &_descSetLayout;
    layoutInfo.pushConstantRangeCount = 0;
    layoutInfo.pPushConstantRanges = nullptr;

    VK_CHECK(vkCreatePipelineLayout(
            _device->getLogicalDevice(), &layoutInfo, nullptr, &_pipelineLayout));
    // --

    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {};

    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = nullptr;
    pipelineInfo.flags = 0;
    pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pTessellationState = nullptr;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthInfo;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = _pipelineLayout;
    pipelineInfo.renderPass = _renderpass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;

    {
        shaderStages[0] = _device->loadShaderFromFile(
                "data/shaders/simple.vert.spv", VK_SHADER_STAGE_VERTEX_BIT),
        shaderStages[1] = _device->loadShaderFromFile(
                "data/shaders/simple.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    }

    VK_CHECK(vkCreateGraphicsPipelines(
            _device->getLogicalDevice(), nullptr, 1, &pipelineInfo, nullptr, &_pipeline));

    for(auto& shader : shaderStages)
    {
        vkDestroyShaderModule(_device->getLogicalDevice(), shader.module, nullptr);
    }
}

auto Context::allocateCommandBuffers() -> void
{
    _renderingCommandBuffers.resize(_swapchain->getImageCount());

    VkCommandBufferAllocateInfo allocateInfo = {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.pNext = nullptr;
    allocateInfo.commandPool = _device->getGraphicsCommandPool();
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = _swapchain->getImageCount();

    VK_CHECK(vkAllocateCommandBuffers(
            _device->getLogicalDevice(), &allocateInfo, _renderingCommandBuffers.data()));
}

auto Context::recordCommandBuffers(uint32_t nextImageIndex) -> void
{
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.pNext = nullptr;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{0.1f, 0.1f, 0.1f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo renderPassBeginInfo = {};
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.pNext = nullptr;
    renderPassBeginInfo.renderPass = _renderpass;
    renderPassBeginInfo.framebuffer = _swapchain->getFrameBuffer(nextImageIndex);
    renderPassBeginInfo.renderArea.offset = {0, 0};
    renderPassBeginInfo.renderArea.extent = _swapchainExtent;
    renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassBeginInfo.pClearValues = clearValues.data();

    // Use opengl like viewport with y-axis pointing up
    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = static_cast<float>(_swapchainExtent.height);
    // viewport.y = 0;
    viewport.width = static_cast<float>(_swapchainExtent.width);
    viewport.height = -static_cast<float>(_swapchainExtent.height);
    // viewport.height = static_cast<float>(m_SwapchainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = _swapchainExtent;
    VkDeviceSize offsets[] = {0};

    {
        VkCommandBuffer cmdBuf = _renderingCommandBuffers[nextImageIndex];
        vkResetCommandBuffer(cmdBuf, 0);
        vkBeginCommandBuffer(cmdBuf, &beginInfo);

        vkCmdBeginRenderPass(cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline);

        vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
        vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

        vkCmdBindDescriptorSets(
                cmdBuf,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                _pipelineLayout,
                0,
                1,
                &_descriptorSet[nextImageIndex],
                0,
                nullptr);

        vkCmdBindVertexBuffers(cmdBuf, 0, 1, &quad.vertexBuffer.buffer, offsets);
        vkCmdBindIndexBuffer(cmdBuf, quad.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmdBuf, static_cast<uint32_t>(_indices.size()), 1, 0, 0, 0);

        viewport.y = 0;
        viewport.height = static_cast<float>(_swapchainExtent.height);

        vkCmdSetViewport(cmdBuf, 0, 1, &viewport);

        vkCmdEndRenderPass(cmdBuf);

        VK_CHECK(vkEndCommandBuffer(cmdBuf));
    }
}

void Context::createUniformBuffers()
{
    _uniformBuffer.resize(_swapchain->getImageCount());

    VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    for(auto& buffer : _uniformBuffer)
    {
        buffer = _device->createBuffer(
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VMA_MEMORY_USAGE_CPU_TO_GPU,
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                bufferSize);
    }
}

void Context::update(float dt, uint32_t imageIndex)
{
    auto ubo = UniformBufferObject{};

    ubo.time = dt;

    auto& buffer = _uniformBuffer[_frameIndex];
    buffer.copyTo(ubo);

    // auto i = _frameIndex;
    // auto readBufferInfo = _simu->getGridBufferInfo();
    // auto descriptorWrites = std::array<VkWriteDescriptorSet, 1>{};
    //
    // descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    // descriptorWrites[0].dstSet = _descriptorSet.at(imageIndex);
    // descriptorWrites[0].dstBinding = 1;
    // descriptorWrites[0].dstArrayElement = 0;
    // descriptorWrites[0].descriptorCount = 1;
    // descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    // descriptorWrites[0].pBufferInfo = &readBufferInfo;
    //
    // vkUpdateDescriptorSets(
    //         _device->getLogicalDevice(),
    //         static_cast<uint32_t>(descriptorWrites.size()),
    //         descriptorWrites.data(),
    //         0,
    //         nullptr);
}

auto Context::cleanupSwapchain() -> void
{
    _swapchain->destroyFrameBuffers();
    vkFreeCommandBuffers(
            _device->getLogicalDevice(),
            _device->getGraphicsCommandPool(),
            static_cast<uint32_t>(_renderingCommandBuffers.size()),
            _renderingCommandBuffers.data());

    vkDestroyRenderPass(_device->getLogicalDevice(), _renderpass, nullptr);
}

auto Context::recreateSwapchain() -> void
{

    int width = 0, height = 0;
    glfwGetFramebufferSize(_window, &width, &height);
    while(width == 0 || height == 0)
    {
        glfwGetFramebufferSize(_window, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(_device->getLogicalDevice());
    cleanupSwapchain();

    _swapchainExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

    _swapchain->create(_config.vsync);
    createRenderPass();
    _swapchain->createFrameBuffers(_renderpass);
    allocateCommandBuffers();
}

void Context::createVertexIndexBuffers()
{
    {
        VkDeviceSize size = sizeof(_vertices[0]) * _vertices.size();
        quad.vertexBuffer = _device->createBufferOnGPU(
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, size, _vertices.data());
    }
    {
        VkDeviceSize size = sizeof(_indices[0]) * _indices.size();
        quad.indexBuffer =
                _device->createBufferOnGPU(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, size, _indices.data());
    }
}

void Context::setupDescriptors()
{
    _descriptorSetGenerator->addBinding(
            0, // binding
            1, // count
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    _descriptorSetGenerator->addBinding(
            1, // binding
            1, // count
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            VK_SHADER_STAGE_FRAGMENT_BIT);

    _descriptorPool = _descriptorSetGenerator->generatePool(100);
    _descSetLayout = _descriptorSetGenerator->generateLayout();

    _descriptorSet.resize(_swapchain->getImageCount());

    for(auto& set : _descriptorSet)
    {
        set = _descriptorSetGenerator->generateSet(_descriptorPool, _descSetLayout);
    }

    for(std::size_t i = 0; i < _descriptorSet.size(); ++i)
    {
        auto& uniformBuffer = _uniformBuffer[i];
        auto imageInfo = _simu->getRenderImageInfo();

        _descriptorSetGenerator->bind(_descriptorSet[i], 0, {uniformBuffer.info});
        _descriptorSetGenerator->bind(_descriptorSet[i], 1, {imageInfo});
    }
    _descriptorSetGenerator->updateSetContents();
}
} // namespace app::vk
