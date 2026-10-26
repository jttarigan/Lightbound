#pragma once
// M1 round-trip microbenchmark (docs/06_RESEARCH_PROTOCOL.md §6).
#include "gfx/Gfx.h"

#include <string>
#include <vector>

namespace lb::bench {

/// GPU state during the CPU step (DECISIONS #32). Idle = protocol §6 as written (the GPU has
/// nothing else to do). Hot = a second queue keeps the GPU busy, as render work does in a real
/// frame; such cells are written with the path name suffixed "_hot". Both = idle cells, then hot.
enum class GpuLoad : u8 { Idle, Hot, Both };
const char* gpuLoadName(GpuLoad l);

/// Scheduling requested for the bench thread (DECISIONS #36). Default = whatever the process started
/// with; Interactive = QOS_CLASS_USER_INTERACTIVE (macOS caps it at priority 31 for a command-line
/// process); High = that plus base priority 47 through THREAD_PRECEDENCE_POLICY, the priority a
/// user-interactive thread has in an app. No-ops elsewhere. A diagnostic for the M5 spin-wait shift;
/// never one of the protocol's conditions. The header records `qos`, `qos.note` and
/// `sysinfo.thread_qos` / `thread_priority` so a CSV always says what the thread ran as.
enum class ThreadQos : u8 { Default, Interactive, High };
const char* threadQosName(ThreadQos q);

struct MicrobenchConfig {
    u32 iterations = 2000;                 // measured iterations per cell
    u32 warmup = 200;                      // discarded iterations per cell
    std::vector<bool> chainModes;          // true = chain, false = perpass
    std::vector<gfx::CpuWaitMode> waitModes;
    std::vector<std::string> paths;        // empty = every path this backend supports
    std::vector<u64> payloads;             // bytes (multiples of 16); P = 0 is the "empty" path
    GpuLoad gpuLoad = GpuLoad::Idle;
    ThreadQos qos = ThreadQos::Default;
    u32 cpuLoad = 0;                       // CPU companion load: busy threads beside the bench thread (DECISIONS #36)
    std::string outPath;
    std::string commandLine;               // full CLI, for the CSV header
    std::string options;                   // normalized options, for the CSV header
    std::string tag;
    bool resume = false;                   // outPath holds an interrupted run: keep its complete cells
};

/// Protocol payload set: 4 KiB .. 16 MiB.
std::vector<u64> defaultMicroPayloads();

/// Name of a path's hot-GPU cells in micro.csv ("s2_direct" -> "s2_direct_hot") and its inverse.
std::string hotPathName(const std::string& path);
bool isHotPath(const std::string& path);

/// Parses "4K,64K,1M,16M" (K/M/G binary suffixes; plain bytes allowed). Sizes must be
/// non-zero multiples of 16.
bool parsePayloadList(const std::string& text, std::vector<u64>& out, std::string& error);

/// Runs every configured cell and writes micro.csv. Returns a process exit code:
/// 0 = all iterations verified, 1 = verification/R7/API failures, 2 = setup error.
int runMicrobench(gfx::Gfx& gfx, const MicrobenchConfig& cfg);

} // namespace lb::bench
