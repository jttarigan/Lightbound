#include "core/FixedVector.h"

#include <doctest/doctest.h>

#include <string>

using namespace lb;

TEST_CASE("FixedVector respects capacity and destroys elements") {
    static int live = 0;
    struct Tracked {
        Tracked() { ++live; }
        Tracked(const Tracked&) { ++live; }
        ~Tracked() { --live; }
    };
    {
        FixedVector<Tracked, 3> v;
        CHECK(v.push_back(Tracked{}));
        CHECK(v.push_back(Tracked{}));
        CHECK(v.push_back(Tracked{}));
        CHECK_FALSE(v.push_back(Tracked{}));
        CHECK(v.size() == 3);
        CHECK(v.full());
        CHECK(live == 3);
        v.pop_back();
        CHECK(live == 2);
    }
    CHECK(live == 0);
}

TEST_CASE("FixedVector basic access") {
    FixedVector<std::string, 4> v;
    v.emplace_back("a");
    v.emplace_back("bb");
    CHECK(v[1] == "bb");
    CHECK(v.back() == "bb");
    int n = 0;
    for (const auto& s : v) n += static_cast<int>(s.size());
    CHECK(n == 3);
    CHECK(v.resize(4));
    CHECK(v[3].empty());
    CHECK_FALSE(v.resize(5));
    v.clear();
    CHECK(v.empty());
}
