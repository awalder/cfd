#pragma once

#include "spdlog/spdlog.h"
#include <memory>
#include <string>

namespace logs
{

using Log = std::shared_ptr<spdlog::logger>;
auto getLogger(std::string const& name = "") -> Log;

} // namespace logs
