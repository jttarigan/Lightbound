#pragma once
// Deterministic RNG (R4): PCG32 (O'Neill, "pcg32" XSH RR 64/32), splitmix64 mixing,
// 32-bit integer and string hashes. No rand(), no wall clock anywhere in the sim.
#include "core/Types.h"

namespace lb {

/// PCG32 XSH-RR 64/32. Bit-identical to the reference `pcg32_random_r`.
struct Pcg32 {
    u64 state = 0x853c49e6748fea9bULL;
    u64 inc   = 0xda3e39cb94b95bdbULL;

    /// Equivalent to pcg32_srandom_r(initstate, initseq).
    static constexpr Pcg32 seeded(u64 initstate, u64 initseq) {
        Pcg32 r;
        r.state = 0u;
        r.inc = (initseq << 1u) | 1u;
        r.next();
        r.state += initstate;
        r.next();
        return r;
    }

    constexpr u32 next() {
        const u64 old = state;
        state = old * 6364136223846793005ULL + inc;
        const u32 xorshifted = static_cast<u32>(((old >> 18u) ^ old) >> 27u);
        const u32 rot = static_cast<u32>(old >> 59u);
        return (xorshifted >> rot) | (xorshifted << ((0u - rot) & 31u));
    }

    /// Uniform in [0, bound). Reference pcg32_boundedrand_r (unbiased, rejection).
    constexpr u32 nextBounded(u32 bound) {
        if (bound <= 1u) return 0u;
        const u32 threshold = (0u - bound) % bound;
        for (;;) {
            const u32 r = next();
            if (r >= threshold) return r % bound;
        }
    }

    /// Uniform integer in [lo, hi] inclusive.
    constexpr i32 nextRange(i32 lo, i32 hi) {
        return lo + static_cast<i32>(nextBounded(static_cast<u32>(hi - lo + 1)));
    }

    /// Uniform float in [0, 1) with 24 bits of mantissa.
    constexpr f32 nextFloat01() {
        return static_cast<f32>(next() >> 8u) * (1.0f / 16777216.0f);
    }

    /// Uniform float in [lo, hi).
    constexpr f32 nextFloat(f32 lo, f32 hi) { return lo + (hi - lo) * nextFloat01(); }

    constexpr bool nextBool() { return (next() & 1u) != 0u; }
};

/// splitmix64 output function applied to (x + golden gamma). Stateless mix used for
/// floor_seed_d = splitmix64(run_seed + d).
constexpr u64 splitmix64(u64 x) {
    u64 z = x + 0x9e3779b97f4a7c15ULL;
    z = (z ^ (z >> 30u)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27u)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31u);
}

/// lowbias32 integer hash (Wellons).
constexpr u32 hash32(u32 x) {
    x ^= x >> 16u;
    x *= 0x7feb352dU;
    x ^= x >> 15u;
    x *= 0x846ca68bU;
    x ^= x >> 16u;
    return x;
}

/// FNV-1a over a NUL-terminated string; used for named RNG streams ("layout", ...).
constexpr u32 hash32(const char* s) {
    u32 h = 0x811c9dc5U;
    for (; *s != '\0'; ++s) {
        h ^= static_cast<u32>(static_cast<unsigned char>(*s));
        h *= 0x01000193U;
    }
    return h;
}

/// Stateless per-agent random value: pcg32(seed, sequence).next(). Matches GDD §5.2
/// `pcg32(floor_seed ^ hash(agent_id), frame_index)`.
constexpr u32 pcgHash(u64 seed, u64 sequence) {
    return Pcg32::seeded(seed, sequence).next();
}

/// Named stream for a procgen stage: pcg32(floor_seed_d, hash32(name)).
constexpr Pcg32 namedStream(u64 floorSeed, const char* name) {
    return Pcg32::seeded(floorSeed, hash32(name));
}

} // namespace lb
