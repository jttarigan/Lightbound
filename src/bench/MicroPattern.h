#pragma once
// CPU mirror of shaders/microbench.slang data patterns (docs/06 §6). Every microbenchmark
// iteration verifies that the CPU read exactly what the GPU wrote and that the GPU read
// exactly what the CPU wrote back.
#include "core/Types.h"

namespace lb::bench {

constexpr u32 kMicroGroupSize = 64;       // LB_AGENT_GROUP_SIZE
constexpr u32 kMicroMaxReadGroups = 256;  // partial sums written by micro_read

/// Element i (16 B) of the pattern micro_write produces in iteration `it`.
inline void gpuWriteElement(u32 i, u32 it, u32 out[4]) {
    out[0] = i ^ it;
    out[1] = i * 2654435761u + it;
    out[2] = i + it * 40503u;
    out[3] = ~i;
}

/// Element i of the pattern the CPU writes back in iteration `it`.
inline void cpuWriteElement(u32 i, u32 it, u32 out[4]) {
    out[0] = i * 3u + it;
    out[1] = i ^ 0xA5A5A5A5u ^ it;
    out[2] = it * 7u + i;
    out[3] = i + 1u;
}

/// Sum (mod 2^32) of all 32-bit words; this is the CPU's "read every byte" step.
inline u32 sumWords(const u32* words, usize count) {
    u32 a = 0, b = 0, c = 0, d = 0;
    usize i = 0;
    for (; i + 4 <= count; i += 4) {
        a += words[i];
        b += words[i + 1];
        c += words[i + 2];
        d += words[i + 3];
    }
    for (; i < count; ++i) a += words[i];
    return a + b + c + d;
}

/// Writes `elements` elements of the CPU pattern ("write P bytes back").
inline void writeCpuPattern(u32* dst, u32 elements, u32 it) {
    for (u32 i = 0; i < elements; ++i) cpuWriteElement(i, it, dst + 4ull * i);
}

/// What sumWords() must return after micro_write(elements, it).
inline u32 expectedCpuReadSum(u32 elements, u32 it) {
    u32 s = 0;
    u32 e[4];
    for (u32 i = 0; i < elements; ++i) {
        gpuWriteElement(i, it, e);
        s += e[0] + e[1] + e[2] + e[3];
    }
    return s;
}

/// What the sum of micro_read's partials must be after the CPU wrote writeCpuPattern(elements, it).
inline u32 expectedGpuChecksum(u32 elements, u32 it) {
    u32 s = 0;
    u32 e[4];
    for (u32 i = 0; i < elements; ++i) {
        cpuWriteElement(i, it, e);
        s += e[0] ^ e[1] ^ e[2] ^ e[3];
    }
    return s;
}

/// micro_write dispatch size (one thread per element; ≥ 1 group).
inline u32 writeGroups(u32 elements) {
    const u32 g = (elements + kMicroGroupSize - 1) / kMicroGroupSize;
    return g == 0 ? 1u : g;
}

/// micro_read dispatch size (grid-stride; ≥ 1 group, ≤ kMicroMaxReadGroups).
inline u32 readGroups(u32 elements) {
    const u32 g = writeGroups(elements);
    return g > kMicroMaxReadGroups ? kMicroMaxReadGroups : g;
}

} // namespace lb::bench
