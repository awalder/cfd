#include "core/app.h"
#include "utils/timer.h"

#include <chrono>
#include <thread>

#include "fmt/chrono.h"

namespace app
{

Application::Application() : _log(logs::getLogger("Application"))
{
}

auto Application::init() -> void
{
    _log->info("Initializing application");

    params::Params parameters;

    {
        auto configHandler = std::make_unique<ConfigHandler>("data/config.ini");
        auto screenConfig = configHandler->loadScreenConfig();
        auto vulkanConfig = configHandler->loadVulkanConfig();
        if(!screenConfig || !vulkanConfig)
        {
            throw std::runtime_error("Failed to load config");
        }

        parameters.screenConfig = *screenConfig;
        parameters.vulkanConfig = *vulkanConfig;
    }

    _appContext = std::make_shared<AppContext>(parameters);
    _windowManager = std::make_unique<WindowManager>(_appContext);
    _windowManager->setup();

    _vkContext = std::make_unique<vk::Context>(
            _appContext, _windowManager->getWindow());
    _vkContext->init(_windowManager->getSwapchainExtent());
}

auto Application::run() -> void
{
    _log->info("Entering mainloop");
    while(!_windowManager->shouldClose())
    {
        auto timer = utils::Timer<std::chrono::duration<double>>{};
        _windowManager->pollEvents();
        _vkContext->renderFrame(static_cast<float>(_frameTime.count()), static_cast<float>(_apprunTime.count()));

        _frameTime = timer.elapsed();
        _apprunTime += _frameTime;
        _frameCounter += 1;

        // if(_frameCounter % 60 == 0)
        // {
        //     _log->info(
        //             "Frame time: {}, Runtime: {}, Counter {}",
        //             std::chrono::duration_cast<std::chrono::microseconds>(
        //                     _frameTime),
        //             std::chrono::duration_cast<std::chrono::milliseconds>(
        //                     _apprunTime),
        //             _frameCounter);
        // }
    }

    _vkContext->deviceWaitIdle();
}

} // namespace app
