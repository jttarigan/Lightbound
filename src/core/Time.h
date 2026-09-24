#pragma once
// Monotonic timing. All CPU timestamps in the engine come from steady_clock in ns.
#include "core/Types.h"

#include <chrono>

namespace lb {

inline u64 nowNs() {
    return static_cast<u64>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
}

constexpr f64 nsToMs(u64 ns) { return static_cast<f64>(ns) * 1e-6; }
constexpr f64 nsToUs(u64 ns) { return static_cast<f64>(ns) * 1e-3; }
constexpr f64 nsToSec(u64 ns) { return static_cast<f64>(ns) * 1e-9; }

struct Stopwatch {
    u64 start = nowNs();
    void restart() { start = nowNs(); }
    u64 elapsedNs() const { return nowNs() - start; }
    f64 elapsedMs() const { return nsToMs(elapsedNs()); }
    f64 elapsedSec() const { return nsToSec(elapsedNs()); }
};

} // namespace lb
