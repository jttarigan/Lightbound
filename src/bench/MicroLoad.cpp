#include "bench/MicroLoad.h"

#include "bench/MicroPattern.h"
#include "core/Log.h"
#include "core/Time.h"

#include <algorithm>

namespace lb::bench {

using namespace lb::gfx;

namespace {

// Issues load dispatches and keeps kLoadInFlight of them pending: before each new dispatch it
// waits for the one issued kLoadInFlight steps earlier.
struct Pacer {
    Gfx& g;
    const LoadKernel& k;
    u64 tickets[kLoadInFlight] = {};
    u64 issued = 0;

    bool step() {
        u64& t = tickets[issued % kLoadInFlight];
        if (t != 0 && !g.loadWait(t)) return false;
        const MicroParams params{{k.steps, 0, 0, 0}};
        t = g.loadDispatch(k.pipeline, k.set, k.groups, &params, sizeof(params));
        if (t == 0) return false;
        ++issued;
        return true;
    }
    bool drain() {
        bool ok = true;
        for (u64& t : tickets) {
            if (t != 0) ok = g.loadWait(t) && ok;
            t = 0;
        }
        return ok;
    }
};

} // namespace

f64 timeLoadDispatches(Gfx& g, const LoadKernel& k, u32 count) {
    Pacer p{g, k};
    for (u32 i = 0; i < 2 * kLoadInFlight; ++i) {
        if (!p.step()) return -1.0;
    }
    if (!p.drain()) return -1.0;
    const u64 t0 = nowNs();
    for (u32 i = 0; i < count; ++i) {
        if (!p.step()) return -1.0;
    }
    if (!p.drain()) return -1.0;
    return static_cast<f64>(nowNs() - t0) * 1e-3 / static_cast<f64>(count);
}

bool calibrateLoad(Gfx& g, LoadKernel& k, f64& dispatchUs) {
    dispatchUs = -1.0;
    if (!g.caps().secondQueue || !k.pipeline.valid() || !k.set.valid()) return false;
    k.steps = 20000;
    for (int round = 0; round < 6; ++round) {
        const f64 us = timeLoadDispatches(g, k, 16);
        if (us < 0.0) return false;
        if (us > kLoadTargetUs * 0.9 && us < kLoadTargetUs * 1.1) break;
        const f64 scaled = static_cast<f64>(k.steps) * kLoadTargetUs / std::max(us, 1.0);
        k.steps = static_cast<u32>(std::clamp(scaled, 1000.0, 2.0e8));
    }
    dispatchUs = timeLoadDispatches(g, k, 32);
    return dispatchUs > 0.0;
}

void BackgroundLoad::start() {
    if (m_thread.joinable()) return;
    m_stop.store(false, std::memory_order_relaxed);
    m_failed.store(false, std::memory_order_relaxed);
    m_dispatches.store(0, std::memory_order_relaxed);
    m_startNs = nowNs();
    m_endNs = m_startNs;
    m_thread = std::thread(&BackgroundLoad::run, this);
}

void BackgroundLoad::stop() {
    if (!m_thread.joinable()) return;
    m_stop.store(true, std::memory_order_relaxed);
    m_thread.join();
}

void BackgroundLoad::run() {
    Pacer p{m_g, m_k};
    while (!m_stop.load(std::memory_order_relaxed)) {
        if (!p.step()) {
            m_failed.store(true, std::memory_order_relaxed);
            break;
        }
        m_dispatches.store(p.issued, std::memory_order_relaxed);
    }
    if (!p.drain()) m_failed.store(true, std::memory_order_relaxed);
    m_endNs = nowNs();
}

} // namespace lb::bench
