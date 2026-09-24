#include "core/Math.h"

#include <doctest/doctest.h>

using namespace lb;

TEST_CASE("vector types carry GPU alignment") {
    CHECK(sizeof(float2) == 8);
    CHECK(alignof(float2) == 8);
    CHECK(sizeof(float4) == 16);
    CHECK(alignof(float4) == 16);
    CHECK(sizeof(uint4) == 16);
}

TEST_CASE("basic vector ops") {
    const float2 a{3.f, 4.f};
    CHECK(length(a) == doctest::Approx(5.f));
    CHECK(dot(a, {1.f, 0.f}) == doctest::Approx(3.f));
    const float2 n = normalize(a);
    CHECK(length(n) == doctest::Approx(1.f));
    CHECK(normalize(float2{0.f, 0.f}) == float2{0.f, 0.f});
    CHECK(clamp(5, 0, 3) == 3);
    CHECK(alignUp(13u, 8u) == 16u);
    CHECK(divCeil(65u, 64u) == 2u);
    CHECK(smoothstep(0.f, 1.f, 0.5f) == doctest::Approx(0.5f));
}
