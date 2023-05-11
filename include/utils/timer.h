#pragma once

#include <chrono>

namespace utils
{

template<typename Resolution = std::chrono::microseconds>
class Timer
{
public:
    using ClockType = std::chrono::steady_clock;

    auto elapsed() const -> Resolution
    {
        return std::chrono::duration_cast<Resolution>(ClockType::now() - _start);
    }

    template<typename ReturnType = float>
    auto elapsedInSeconds() const -> ReturnType
    {
        return std::chrono::duration<ReturnType>(ClockType::now() - _start);
    }

private:
    ClockType::time_point _start = ClockType::now();
};

} // namespace utils
