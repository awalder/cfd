#pragma once

#include "common/appcontext.h"
#include "confighandler.h"
#include "core/vulkan/context.h"
#include "core/window/windowmanager.h"
#include "logs/log.h"

#include <chrono>

namespace app
{

class Application final
{
public:
    Application();

    auto init() -> void;
    auto run() -> void;

private:
    // appcontext needs to be first to be destroyed last
    std::shared_ptr<AppContext> _appContext;

    // std::unique_ptr<ConfigHandler> _configHandler;
    std::unique_ptr<WindowManager> _windowManager;
    std::unique_ptr<vk::Context> _vkContext;
    logs::Log _log;

    // Total runtime of application
    std::chrono::duration<double> _apprunTime{0};
    // Total time taken by one cycle of mainloop
    std::chrono::duration<double> _frameTime{0};

    uint64_t _frameCounter = 0;
};

} // namespace app
