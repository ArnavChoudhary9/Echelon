#pragma once

#include <chrono>

namespace Echelon {
    using Clock = std::chrono::high_resolution_clock;
    using milliseconds = std::chrono::milliseconds;

    auto GetTimePoint() { return Clock::now(); }
    float GetTime() { return std::chrono::duration<float, std::chrono::seconds::period>(Clock::now().time_since_epoch()).count(); }
    float GetDeltaTime(const Clock::time_point& start, const Clock::time_point& end) {
        return std::chrono::duration<float, std::chrono::seconds::period>(end - start).count();
    }
} // namespace Echelon
