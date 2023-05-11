#pragma once

#include "logs/log.h"
#include "vulkan/vulkan.h"

#include <string>

namespace app::vk
{

class DebugUtils final
{
public:
    DebugUtils(
            VkInstance const& instance,
            VkDebugUtilsMessageSeverityFlagsEXT severity,
            VkDebugUtilsMessageTypeFlagsEXT type);
    ~DebugUtils();

    DebugUtils(DebugUtils const&) = delete;
    DebugUtils(DebugUtils&&) = delete;
    auto operator=(DebugUtils const&) -> DebugUtils& = delete;
    auto operator=(DebugUtils&&) -> DebugUtils& = delete;

private:
    [[nodiscard]] static VKAPI_ATTR auto VKAPI_CALL debug_messenger_callback(
            VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
            VkDebugUtilsMessageTypeFlagsEXT messageType,
            VkDebugUtilsMessengerCallbackDataEXT const* callbackData,
            void* userData) -> VkBool32;

    [[nodiscard]] auto createDebugUtilsMessengerEXT(
            VkInstance m_instance,
            VkDebugUtilsMessengerCreateInfoEXT const* pCreateInfo,
            VkAllocationCallbacks const* pAllocator,
            VkDebugUtilsMessengerEXT* pMessenger) -> VkResult;

    auto destroyDebugUtilsMessengerEXT(
            VkInstance m_instance,
            VkDebugUtilsMessengerEXT messenger,
            VkAllocationCallbacks const* pAllocator) -> void;

    [[nodiscard]] static auto debugAnnotateObjectToString(const VkObjectType object_type)
            -> char const*;

private:
    VkDebugUtilsMessengerEXT _messenger = VK_NULL_HANDLE;
    VkInstance const& _instance;
    logs::Log _log;
};

} // namespace app::vk
