#pragma once
// Shared helpers of the floor generator stages. Layout decisions use integers only (docs/02 §1);
// the only floating-point values are px positions built from integers and sqrt of integer
// squared distances, so floors are bit-identical across compilers (lb_procgen builds with
// FP contraction off, DECISIONS #34).
#include "procgen/FloorData.h"

#include "core/Rng.h"

#include <vector>

namespace lb::procgen::detail {

constexpr i32 kDx8[8] = {1, 1, 0, -1, -1, -1, 0, 1};  // 45° steps, y down; dir 0 = +x
constexpr i32 kDy8[8] = {0, 1, 1, 1, 0, -1, -1, -1};
constexpr i32 kDx4[4] = {1, 0, -1, 0};
constexpr i32 kDy4[4] = {0, 1, 0, -1};

inline i64 sq(i64 v) { return v * v; }

inline float2 tileCenterPx(i32 x, i32 y) {
    constexpr i32 kHalf = kTilePx / 2;
    return {static_cast<f32>(x * kTilePx + kHalf), static_cast<f32>(y * kTilePx + kHalf)};
}

/// Squared px distance from point (px, py) to the square of tile (tx, ty).
inline i64 pointToTileD2(i32 px, i32 py, i32 tx, i32 ty) {
    const i32 x0 = tx * kTilePx, x1 = x0 + kTilePx, y0 = ty * kTilePx, y1 = y0 + kTilePx;
    const i64 dx = px < x0 ? x0 - px : (px > x1 ? px - x1 : 0);
    const i64 dy = py < y0 ? y0 - py : (py > y1 ? py - y1 : 0);
    return dx * dx + dy * dy;
}

/// Fisher–Yates with the given stream.
template <class T> void shuffle(std::vector<T>& v, Pcg32& rng) {
    for (usize i = v.size(); i > 1; --i) {
        const usize j = rng.nextBounded(static_cast<u32>(i));
        T tmp = v[i - 1];
        v[i - 1] = v[j];
        v[j] = tmp;
    }
}

/// Per-attempt generator state.
struct Gen {
    FloorData& f;
    u64 floorSeed = 0;
    i32 w = 0, h = 0;
    Pcg32 layout, caves, lights, props, spawns, deco;
    std::vector<i64> tileD2;      // open tiles: squared px distance from tile center to nearest wall square
    std::vector<i32> bfsDist;     // walkable 4-connected distance from START, -1 unreachable
    const char* failure = "";

    Gen(FloorData& out, u64 seed, i32 width, i32 height);

    bool inside(i32 x, i32 y) const { return x >= 0 && y >= 0 && x < w && y < h; }
    u32 idx(i32 x, i32 y) const { return static_cast<u32>(y * w + x); }
    i32 ix(u32 i) const { return static_cast<i32>(i) % w; }
    i32 iy(u32 i) const { return static_cast<i32>(i) / w; }
    Tile& t(i32 x, i32 y) { return f.tiles[idx(x, y)]; }
    Tile at(i32 x, i32 y) const { return f.at(x, y); }
    bool fail(const char* why) { failure = why; return false; }
};

// Stages (docs/02 §3). Each returns false (with g.failure set) when the attempt must be re-rolled.
bool stageLayout(Gen& g);         // 1: rooms, Delaunay → MST + 15 % loops, roles
bool stageCaves(Gen& g);          // 2: CA blobs, corridors, water, chasms, connectivity, regions
void stageDistanceField(Gen& g);  // 3: tile EDT → 4× samples → jump-flood refinement
bool stageLights(Gen& g);         // 4: braziers, crystals
bool stageProps(Gen& g);          // 5: exit, caches, oil, nests, seal puzzle, deco; light cap + pre-lit
void stageSpawns(Gen& g);         // 6: dark pockets, nest zones
bool validateFloor(Gen& g);       // §5 + metrics (§6)

/// Seal puzzle construction (stage 5). Adds sigil, mirrors, beam source prop and crystal light.
bool buildSealPuzzle(Gen& g);

/// Walkable BFS from tile `start` (4-connected); fills dist (-1 = unreachable), returns reached count.
u32 walkableBfs(const FloorData& f, u32 start, std::vector<i32>& dist);

} // namespace lb::procgen::detail
