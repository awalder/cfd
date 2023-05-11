#pragma once

#include "logs/log.h"
#include "core/window/windowmanager.h"

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
    std::unique_ptr<WindowManager> _windowManager;
    logs::Log _log;

    // Total runtime of application
    std::chrono::duration<double> _apprunTime{0};
    // Total time taken by one cycle of mainloop
    std::chrono::duration<double> _frameTime{0};

    uint64_t _frameCounter = 0;
};

} // namespace app
