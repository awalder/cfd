#include "logs/log.h"

#include "spdlog/sinks/stdout_color_sinks.h"

namespace logs
{

auto getLogger(std::string const& name) -> std::shared_ptr<spdlog::logger>
{
    auto logger = spdlog::get(name);
    if(!logger)
    {
        logger = spdlog::stdout_color_mt(name);
        logger->set_level(spdlog::level::debug);
        logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] %v");
    }

    return logger;
}

} // namespace logs
