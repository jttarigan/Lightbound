#pragma once
// Background GPU load for the microbench's hot-GPU cells (DECISIONS #32). A second queue runs a
// small calibrated kernel back to back while the measured round trips run on the main queue,
// the way render work keeps the GPU busy in a real frame (docs/05 §3 "GPU may run independent
// work"). The pacing loop and the calibration live here, once, for both backends (R1); the
// backends only provide Gfx::loadDispatch / loadWait.
#include "gfx/Gfx.h"

#include <array>
#include <atomic>
#include <thread>

namespace lb::bench {

constexpr u32 kLoadInFlight = 2;        // dispatches queued ahead on the second queue
constexpr f64 kLoadTargetUs = 200.0;    // calibrated length of one load dispatch

struct LoadKernel {
    gfx::PipelineHandle pipeline;   // micro_burn
    gfx::BindingSetHandle set;
    u32 groups = 1;
    u32 steps = 0;                  // push constant v[0]: steps of the dependent multiply-add chain
};

/// Runs `count` load dispatches with kLoadInFlight queued ahead (after a short warm-up) and
/// returns the wall time per dispatch in µs, or a negative value on error.
f64 timeLoadDispatches(gfx::Gfx& g, const LoadKernel& k, u32 count);

/// Finds the step count that makes one dispatch take about kLoadTargetUs and measures it.
/// Returns false if the second queue does not work.
bool calibrateLoad(gfx::Gfx& g, LoadKernel& k, f64& dispatchUs);

/// Keeps the second queue busy from start() until stop(), kLoadInFlight dispatches ahead.
class BackgroundLoad {
public:
    BackgroundLoad(gfx::Gfx& g, const LoadKernel& k) : m_g(g), m_k(k) {}
    ~BackgroundLoad() { stop(); }
    BackgroundLoad(const BackgroundLoad&) = delete;
    BackgroundLoad& operator=(const BackgroundLoad&) = delete;

    void start();
    /// Joins the load thread; every dispatch has completed when this returns.
    void stop();
    bool running() const { return m_thread.joinable(); }
    u64 dispatches() const { return m_dispatches.load(std::memory_order_relaxed); }
    bool failed() const { return m_failed.load(std::memory_order_relaxed); }
    /// Wall time from start() to the last completed dispatch (µs); valid after stop().
    f64 activeUs() const { return static_cast<f64>(m_endNs - m_startNs) * 1e-3; }

private:
    void run();

    gfx::Gfx& m_g;
    LoadKernel m_k;
    std::thread m_thread;
    std::atomic<bool> m_stop{false};
    std::atomic<bool> m_failed{false};
    std::atomic<u64> m_dispatches{0};
    u64 m_startNs = 0, m_endNs = 0;
};

/// CPU companion load for the microbench (--micro-cpuload=N, the M5 spin-wait shift, DECISIONS #36):
/// N threads each run a dependent integer multiply-add chain (no memory traffic) from start() until
/// stop(), so the bench thread shares the SoC with a defined load instead of whatever else runs.
/// Plain std::thread, the same on every platform (R1). The iteration rate per thread is reported as
/// a rough clock proxy for the loaded cores.
constexpr u32 kMaxCpuLoadThreads = 16;

class CpuLoad {
public:
    explicit CpuLoad(u32 threads) : m_count(threads < kMaxCpuLoadThreads ? threads : kMaxCpuLoadThreads) {}
    ~CpuLoad() { stop(); }
    CpuLoad(const CpuLoad&) = delete;
    CpuLoad& operator=(const CpuLoad&) = delete;

    void start();
    /// Joins every load thread.
    void stop();
    u32 threads() const { return m_count; }
    /// Mean chain iterations per µs and thread from start() to stop(); valid after stop().
    f64 itersPerUs() const;
    f64 activeUs() const { return static_cast<f64>(m_endNs - m_startNs) * 1e-3; }

private:
    void run(u32 index);

    u32 m_count;
    std::array<std::thread, kMaxCpuLoadThreads> m_threads;
    std::array<u64, kMaxCpuLoadThreads> m_iters{};
    std::array<u64, kMaxCpuLoadThreads> m_sink{};   // chain results, kept so the loop is not optimized away
    std::atomic<bool> m_stop{false};
    bool m_running = false;
    u64 m_startNs = 0, m_endNs = 0;
};

} // namespace lb::bench
