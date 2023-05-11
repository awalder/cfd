#pragma once

#include "fmt/format.h"
#include "vulkan/vulkan.h"

#include <string>

namespace app::vk::utils
{

auto errorString(VkResult result) -> std::string;
auto deviceType(VkPhysicalDeviceType deviceType) -> std::string;

inline constexpr void checkVkResult(VkResult res, char const* file, int line)
{
    if(res != VK_SUCCESS)
    {
        auto error = fmt::format(
                "Fatal : VkResult is \"{}\" in {} at line {}",
                app::vk::utils::errorString(res),
                file,
                line);
        throw std::runtime_error(error);
    }
}

} // namespace app::vk::utils

#define VK_CHECK(f) app::vk::utils::checkVkResult((f), __FILE__, __LINE__)
