#pragma once

#include "spdlog/spdlog.h"
#include <memory>
#include <string>

namespace logs
{

using Log = std::shared_ptr<spdlog::logger>;
auto getLogger(const std::string& name = "") -> Log;

} // namespace logs
