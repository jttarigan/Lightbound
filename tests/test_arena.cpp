#include "core/Arena.h"

#include <doctest/doctest.h>

#include <cstdint>

using namespace lb;

TEST_CASE("arena hands out aligned, non-overlapping blocks") {
    Arena arena(4096);
    REQUIRE(arena.capacity() == 4096);
    void* a = arena.alloc(10, 16);
    void* b = arena.alloc(100, 64);
    void* c = arena.alloc(1, 1);
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    REQUIRE(c != nullptr);
    CHECK(reinterpret_cast<std::uintptr_t>(a) % 16 == 0);
    CHECK(reinterpret_cast<std::uintptr_t>(b) % 64 == 0);
    CHECK(static_cast<char*>(b) >= static_cast<char*>(a) + 10);
    CHECK(static_cast<char*>(c) >= static_cast<char*>(b) + 100);
    CHECK(arena.owns(a));
    CHECK(arena.owns(c));
}

TEST_CASE("arena returns nullptr when exhausted and never throws") {
    Arena arena(256);
    CHECK(arena.alloc(200) != nullptr);
    CHECK(arena.alloc(100) == nullptr);
    CHECK(arena.used() <= 256);
    CHECK(arena.alloc(0) != nullptr);
    CHECK(arena.alloc(8, 3) == nullptr); // non power-of-two alignment
}

TEST_CASE("arena markers and reset") {
    Arena arena(1024);
    arena.allocArray<int>(16);
    const Arena::Marker m = arena.mark();
    arena.allocArray<double>(32);
    CHECK(arena.used() >= 64 + 256);
    arena.resetTo(m);
    CHECK(arena.used() == m.offset);
    CHECK(arena.highWater() >= 64 + 256);
    arena.reset();
    CHECK(arena.used() == 0);
    int* z = arena.allocArrayZeroed<int>(8);
    REQUIRE(z != nullptr);
    for (int i = 0; i < 8; ++i) CHECK(z[i] == 0);
}

TEST_CASE("arena move semantics") {
    Arena a(512);
    void* p = a.alloc(8);
    Arena b(std::move(a));
    CHECK(b.owns(p));
    CHECK(a.capacity() == 0); // NOLINT(bugprone-use-after-move)
}
