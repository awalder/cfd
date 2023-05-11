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

    _windowManager = std::make_unique<WindowManager>();
    _windowManager->setup();
}

auto Application::run() -> void
{
    _log->info("Entering mainloop");
    while(!_windowManager->shouldClose())
    {
        auto timer = utils::Timer<std::chrono::duration<double>>{};
        _windowManager->pollEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

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
}

} // namespace app
