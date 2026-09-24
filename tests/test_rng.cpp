#include "core/Rng.h"

#include <doctest/doctest.h>

using namespace lb;

TEST_CASE("pcg32 known-answer test (reference pcg32_srandom_r(42, 54))") {
    Pcg32 rng = Pcg32::seeded(42u, 54u);
    // First six outputs of the reference implementation's demo program.
    const u32 expected[] = {0xa15c02b7u, 0x7b47f409u, 0xba1d3330u, 0x83d2f293u, 0xbfa4784bu, 0xcbed606eu};
    for (u32 e : expected) CHECK(rng.next() == e);
}

TEST_CASE("pcg32 is a pure function of its seed") {
    Pcg32 a = Pcg32::seeded(0xDEADBEEFu, 7u);
    Pcg32 b = Pcg32::seeded(0xDEADBEEFu, 7u);
    for (int i = 0; i < 1000; ++i) CHECK(a.next() == b.next());
    Pcg32 c = Pcg32::seeded(0xDEADBEEFu, 8u);
    bool differs = false;
    for (int i = 0; i < 16; ++i) differs |= (a.next() != c.next());
    CHECK(differs);
}

TEST_CASE("pcg32 bounded and float outputs stay in range") {
    Pcg32 rng = Pcg32::seeded(1u, 1u);
    u32 hist[7] = {};
    for (int i = 0; i < 70000; ++i) {
        const u32 v = rng.nextBounded(7u);
        REQUIRE(v < 7u);
        ++hist[v];
    }
    for (u32 h : hist) CHECK(h > 9000u); // roughly uniform (expected 10000)
    for (int i = 0; i < 10000; ++i) {
        const f32 f = rng.nextFloat01();
        REQUIRE(f >= 0.f);
        REQUIRE(f < 1.f);
    }
    for (int i = 0; i < 1000; ++i) {
        const i32 r = rng.nextRange(-3, 3);
        REQUIRE(r >= -3);
        REQUIRE(r <= 3);
    }
    CHECK(rng.nextBounded(0u) == 0u);
    CHECK(rng.nextBounded(1u) == 0u);
}

TEST_CASE("splitmix64 known answer (SplitMix64 seed 0, first output)") {
    CHECK(splitmix64(0u) == 0xe220a8397b1dcdafULL);
    CHECK(splitmix64(1u) != splitmix64(2u));
}

TEST_CASE("hash32 FNV-1a known answers") {
    CHECK(hash32("") == 0x811c9dc5u);
    CHECK(hash32("a") == 0xe40c292cu);
    CHECK(hash32("layout") != hash32("caves"));
}

TEST_CASE("hash32 integer hash is a bijection on a sample and pcgHash is stateless") {
    CHECK(hash32(0u) != hash32(1u));
    CHECK(pcgHash(12345u, 1u) == pcgHash(12345u, 1u));
    CHECK(pcgHash(12345u, 1u) != pcgHash(12345u, 2u));
    static_assert(pcgHash(7u, 3u) == pcgHash(7u, 3u)); // constexpr-evaluable
}

TEST_CASE("named streams differ per stage") {
    Pcg32 a = namedStream(99u, "layout");
    Pcg32 b = namedStream(99u, "caves");
    CHECK(a.next() != b.next());
}
