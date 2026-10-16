#include "procgen/FloorData.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using namespace lb;
using namespace lb::procgen;

namespace {

std::vector<u64> benchSeeds() {
    std::ifstream in(std::string(LB_SOURCE_DIR) + "/bench/seeds.txt");
    std::vector<u64> seeds;
    std::string line;
    while (std::getline(in, line))
        if (!line.empty() && line[0] != '#') seeds.push_back(std::strtoull(line.c_str(), nullptr, 10));
    return seeds;
}

FloorData gen(u64 seed, u32 depth, u32 size = 0) {
    FloorData f;
    const GenResult r = generateFloor({seed, depth, size}, f);
    INFO("seed " << seed << " depth " << depth << " last failure: " << std::string(r.lastFailure));
    REQUIRE(r.status == GenStatus::Ok);
    return f;
}

} // namespace

TEST_CASE("procgen determinism: seeds 1..200 generate identical floors twice") {
    for (u64 seed = 1; seed <= 200; ++seed) {
        const u32 depth = 1u + static_cast<u32>(seed % 5u);
        const FloorData a = gen(seed, depth), b = gen(seed, depth);
        INFO("seed " << seed);
        CHECK(floorHash(a) == floorHash(b));
    }
}

TEST_CASE("procgen validity: seeds 1..1000 pass every rule within 16 attempts") {
    u32 worst = 0;
    for (u64 seed = 1; seed <= 1000; ++seed) {
        const FloorData f = gen(seed, 1u + static_cast<u32>(seed % 5u));
        worst = std::max(worst, f.metrics.attempts);
        CHECK(f.staticLights.size() <= kMaxStaticLights);
        CHECK(f.metrics.criticalPathLen > 0);
    }
    MESSAGE("most attempts needed: " << worst);
}

TEST_CASE("procgen golden hashes (bench seeds, depths 1..5) match tests/golden/floors.txt") {
    // Regenerate after an intended generator change (and bump kGeneratorVersion):
    //   LB_UPDATE_GOLDEN=1 ./build/lb_tests -tc="procgen golden*"
    const std::string path = std::string(LB_SOURCE_DIR) + "/tests/golden/floors.txt";
    std::ostringstream now;
    for (u64 seed : benchSeeds())
        for (u32 depth = 1; depth <= 5; ++depth) {
            char line[64];
            std::snprintf(line, sizeof line, "%" PRIu64 " %u %016" PRIx64 "\n", seed, depth, floorHash(gen(seed, depth)));
            now << line;
        }
    if (std::getenv("LB_UPDATE_GOLDEN") != nullptr) {
        std::ofstream out(path);
        out << "# seed depth floorHash (docs/02 §7): must match on macOS and Windows\n" << now.str();
        MESSAGE("rewrote " << path);
        return;
    }
    std::ifstream in(path);
    REQUIRE(in.good());
    std::ostringstream want;
    std::string line;
    while (std::getline(in, line))
        if (!line.empty() && line[0] != '#') want << line << "\n";
    CHECK(now.str() == want.str());
}

TEST_CASE("procgen performance: a 200x200 floor in < 150 ms (Release)") {
#ifdef NDEBUG
    for (u64 seed : benchSeeds()) {
        f64 best = 1e9;
        for (int rep = 0; rep < 3; ++rep) best = std::min(best, gen(seed, 5).metrics.genMs);
        INFO("seed " << seed);
        CHECK(best < 150.0);
    }
#else
    MESSAGE("skipped: not a Release build");
#endif
}

TEST_CASE("procgen bench size override gives a 200x200 floor at any depth") {
    const FloorData f = gen(101, 1, 200);
    CHECK(f.w == 200);
    CHECK(f.h == 200);
    CHECK(f.biome == Biome::MossyHollow);
}

TEST_CASE("procgen seal puzzle: scrambled, solvable within 8, solved by the stored solution") {
    for (u64 seed = 1; seed <= 40; ++seed)
        for (u32 depth : {3u, 5u}) {
            FloorData f = gen(seed, depth);
            INFO("seed " << seed << " depth " << depth);
            CHECK(f.metrics.mirrorCount == (depth == 5 ? 3u : 2u));
            const i32 moves = solveSealPuzzle(f, 8);
            CHECK(moves >= 1);
            CHECK(moves <= 8);
            for (Prop& p : f.props)
                if (p.kind == PropKind::Mirror) p.orientation = p.solution;
            CHECK(solveSealPuzzle(f, 8) == 0);
        }
    const FloorData plain = gen(7, 2);
    CHECK(plain.metrics.mirrorCount == 0);
}

TEST_CASE("procgen SDF: exact distance to the nearest wall (or open) tile square, up to jump-flood error") {
    for (u64 seed : {3ull, 11ull})
        for (u32 depth : {1u, 4u}) {
            const FloorData f = gen(seed, depth);
            const i32 sw = static_cast<i32>(f.sdfWidth()), w = static_cast<i32>(f.w), h = static_cast<i32>(f.h);
            u32 checked = 0, inexact = 0;
            f64 worst = 0.0;
            for (i32 s = 0; s < sw * static_cast<i32>(f.sdfHeight()); s += 7) {
                const f32 v = f.sdf[static_cast<usize>(s)];
                if (v < -32.f) continue;  // deep inside rock: not refined (DECISIONS #34)
                const i32 px = (s % sw) * kSdfSamplePx + kSdfSamplePx / 2, py = (s / sw) * kSdfSamplePx + kSdfSamplePx / 2;
                const bool open = v > 0.f;
                const i32 reach = static_cast<i32>(std::fabs(v)) / kTilePx + 2;
                const i32 tx = px / kTilePx, ty = py / kTilePx;
                i64 best = INT64_MAX;
                for (i32 y = ty - reach; y <= ty + reach; ++y)
                    for (i32 x = tx - reach; x <= tx + reach; ++x) {
                        const bool wall = x < 0 || y < 0 || x >= w || y >= h || blocksLight(f.at(x, y));
                        if (wall != open) continue;
                        if (x < 0 || y < 0 || x >= w || y >= h) continue;
                        const i64 dx = px < x * kTilePx ? x * kTilePx - px : (px > x * kTilePx + kTilePx ? px - x * kTilePx - kTilePx : 0);
                        const i64 dy = py < y * kTilePx ? y * kTilePx - py : (py > y * kTilePx + kTilePx ? py - y * kTilePx - kTilePx : 0);
                        best = std::min(best, dx * dx + dy * dy);
                    }
                const f64 err = std::fabs(std::fabs(static_cast<f64>(v)) - std::sqrt(static_cast<f64>(best)));
                worst = std::max(worst, err);
                inexact += err > 1e-3 ? 1u : 0u;
                ++checked;
            }
            INFO("seed " << seed << " depth " << depth << " checked " << checked << " inexact " << inexact
                         << " worst " << worst << " px");
            CHECK(worst < 1.0);                 // jump flood is approximate far from walls
            CHECK(inexact * 1000u < checked);   // < 0.1 % of samples
        }
}

TEST_CASE("procgen calibration statistics" * doctest::skip()) {
    // Run with: lb_tests -tc="procgen calibration*" --no-skip
    std::map<std::string, u32> failures;
    for (u32 depth = 1; depth <= 5; ++depth) {
        std::vector<f64> openFrac, ms;
        u32 attemptsMax = 0, failed = 0;
        for (u64 seed = 1; seed <= 200; ++seed) {
            FloorData f;
            const GenResult r = generateFloor({seed, depth, 0}, f);
            if (r.status != GenStatus::Ok) { ++failed; ++failures[r.lastFailure]; continue; }
            if (f.metrics.attempts > 1) ++failures[std::string("retry: ") + r.lastFailure];
            openFrac.push_back(static_cast<f64>(f.metrics.floorTiles) / (f.w * f.h));
            ms.push_back(f.metrics.genMs);
            attemptsMax = std::max(attemptsMax, f.metrics.attempts);
        }
        std::sort(openFrac.begin(), openFrac.end());
        std::sort(ms.begin(), ms.end());
        if (openFrac.empty()) { std::printf("d%u: all failed\n", depth); continue; }
        std::printf("d%u: ok %zu failed %u | open frac min %.3f p50 %.3f max %.3f | gen ms p50 %.1f max %.1f | attempts max %u\n",
                    depth, openFrac.size(), failed, openFrac.front(), openFrac[openFrac.size() / 2], openFrac.back(),
                    ms[ms.size() / 2], ms.back(), attemptsMax);
    }
    for (const auto& [why, n] : failures) std::printf("  %4u  %s\n", n, why.c_str());
}


