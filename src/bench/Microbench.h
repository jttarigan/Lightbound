#pragma once
// M1 round-trip microbenchmark (docs/06_RESEARCH_PROTOCOL.md §6).
#include "gfx/Gfx.h"

#include <string>
#include <vector>

namespace lb::bench {

struct MicrobenchConfig {
    u32 iterations = 2000;                 // measured iterations per cell
    u32 warmup = 200;                      // discarded iterations per cell
    std::vector<bool> chainModes;          // true = chain, false = perpass
    std::vector<gfx::CpuWaitMode> waitModes;
    std::vector<std::string> paths;        // empty = every path this backend supports
    std::vector<u64> payloads;             // bytes (multiples of 16); P = 0 is the "empty" path
    std::string outPath;
    std::string commandLine;               // full CLI, for the CSV header
    std::string options;                   // normalized options, for the CSV header
    std::string tag;
};

/// Protocol payload set: 4 KiB .. 16 MiB.
std::vector<u64> defaultMicroPayloads();

/// Parses "4K,64K,1M,16M" (K/M/G binary suffixes; plain bytes allowed). Sizes must be
/// non-zero multiples of 16.
bool parsePayloadList(const std::string& text, std::vector<u64>& out, std::string& error);

/// Runs every configured cell and writes micro.csv. Returns a process exit code:
/// 0 = all iterations verified, 1 = verification/R7/API failures, 2 = setup error.
int runMicrobench(gfx::Gfx& gfx, const MicrobenchConfig& cfg);

} // namespace lb::bench
