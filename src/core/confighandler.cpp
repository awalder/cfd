#include "core/confighandler.h"

#include "mini/ini.h"

namespace app
{

ConfigHandler::ConfigHandler(std::string configFilePath)
    : _log(logs::getLogger("ConfigHandler")), _configFile(std::move(configFilePath))
{
}

auto ConfigHandler::loadVulkanConfig() -> std::optional<params::Params::VulkanConfig>
{
    auto file = mINI::INIFile{_configFile};
    auto ini = mINI::INIStructure{};

    if(!file.read(ini))
    {
        _log->error("Failed to read config file: {}", _configFile);
        return std::nullopt;
    }

    auto vulkanConfig = params::Params::VulkanConfig{};
    vulkanConfig.vsync = ini["vulkan"]["vsync"] == "true";
    vulkanConfig.validationLayers = ini["vulkan"]["validationLayers"] == "true";
    vulkanConfig.debugUtils = ini["vulkan"]["debugUtils"] == "true";

    return vulkanConfig;
}

auto ConfigHandler::loadScreenConfig() -> std::optional<params::Params::ScreenConfig>
{
    auto file = mINI::INIFile{_configFile};
    auto ini = mINI::INIStructure{};

    if(!file.read(ini))
    {
        _log->error("Failed to read config file: {}", _configFile);
        return std::nullopt;
    }

    auto screenConfig = params::Params::ScreenConfig{};

    auto width = ini["screen"]["width"];
    auto height = ini["screen"]["height"];

    screenConfig.width = std::stoi(width);
    screenConfig.height = std::stoi(height);

    return screenConfig;
}
} // namespace app
