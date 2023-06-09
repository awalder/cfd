#include "core/app.h"
#include <cstdlib>
#include <exception>

#include "logs/log.h"

auto main() -> int
{

    auto log = logs::getLogger("main");
    log->info("Starting application");

    auto app = app::Application{};
        app.init();
        app.run();
    // try
    // {
    // }
    // catch(std::exception const& e)
    // {
    //     log->error("Exception: {}", e.what());
    //     return EXIT_FAILURE;
    // }
    // catch(...)
    // {
    //     log->error("Unknown exception");
    //     return EXIT_FAILURE;
    // }
    // app.run();
    return EXIT_SUCCESS;
}
