#pragma once

#include "common/appcontext.h"
#include "fmt/core.h"
#include "logs/log.h"

#include <optional>
#include <string>

namespace app
{
class ConfigHandler
{
public:
    ConfigHandler(std::string configFilePath);
    [[nodiscard]] auto loadVulkanConfig() -> std::optional<params::Params::VulkanConfig>;
    [[nodiscard]] auto loadScreenConfig() -> std::optional<params::Params::ScreenConfig>;

private:
    logs::Log _log;
    std::string _configFile;
};
} // namespace app
