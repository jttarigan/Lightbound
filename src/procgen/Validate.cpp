// Validation (docs/02 §5) and floor metrics (§6). A failed rule re-rolls the floor.
#include "procgen/Internal.h"

#include <algorithm>

namespace lb::procgen::detail {
namespace {

/// Open tiles per map tile that rule 6 compares against (±15 %): the median of seeds 1..200 per
/// GDD map size (DECISIONS #34), linear in between, in per-mille so the check is integer-only.
u32 targetOpenPerMille(u32 size) {
    constexpr u32 kSize[5] = {120, 140, 160, 180, 200};
    constexpr u32 kPerMille[5] = {214, 222, 228, 234, 235};
    if (size <= kSize[0]) return kPerMille[0];
    for (usize i = 1; i < 5; ++i)
        if (size <= kSize[i])
            return kPerMille[i - 1] + (kPerMille[i] - kPerMille[i - 1]) * (size - kSize[i - 1]) / (kSize[i] - kSize[i - 1]);
    return kPerMille[4];
}

} // namespace

bool validateFloor(Gen& g) {
    FloorData& f = g.f;
    const Room& sr = f.rooms[f.startRoom];
    const Room& er = f.rooms[f.exitRoom];
    const u32 start = g.idx(sr.cx, sr.cy), exit = g.idx(er.cx, er.cy);
    std::vector<i32>& dist = g.bfsDist;
    walkableBfs(f, start, dist);

    // 1. START → EXIT walkable (4-connected tiles; the player's 5 px radius fits a 16 px tile).
    if (dist[exit] < 0) return g.fail("rule 1: exit unreachable");
    f.criticalPath.assign(static_cast<usize>(dist[exit]) + 1u, 0);
    for (u32 c = exit;;) {
        f.criticalPath[static_cast<usize>(dist[c])] = c;
        if (c == start) break;
        const i32 x = g.ix(c), y = g.iy(c);
        for (i32 k = 0; k < 4; ++k) {
            const i32 nx = x + kDx4[k], ny = y + kDy4[k];
            if (g.inside(nx, ny) && dist[g.idx(nx, ny)] == dist[c] - 1) { c = g.idx(nx, ny); break; }
        }
    }

    // 2. Every cache, nest and oil pool reachable. 4. Nearest nest ≥ 40 tiles of path from START.
    i32 nearestNest = INT32_MAX;
    for (const Prop& p : f.props) {
        if (p.kind != PropKind::Cache && p.kind != PropKind::Nest && p.kind != PropKind::OilPool) continue;
        if (dist[p.tile] < 0) return g.fail("rule 2: cache/nest/oil unreachable");
        if (p.kind == PropKind::Nest) nearestNest = std::min(nearestNest, dist[p.tile]);
    }
    if (nearestNest < 40) return g.fail("rule 4: nest closer than 40 tiles");
    for (u32 c : f.spawnCells)
        if (f.regionId[c] == f.startRoom) return g.fail("rule 4: spawn zone in START");

    // 3. Seal floors: solvable within 8 rotations (and not pre-solved).
    if (f.seal) {
        const i32 moves = solveSealPuzzle(f, 8);
        if (moves < 1) return g.fail("rule 3: puzzle");
    }

    // 5. d ≤ 3: a lit brazier within 30 tiles of each 60-tile stretch of the critical path.
    if (f.depth <= 3) {
        for (usize s = 0; s < f.criticalPath.size(); s += 60) {
            const usize e = std::min(f.criticalPath.size(), s + 60);
            bool covered = false;
            for (const Light& l : f.staticLights) {
                if (l.kind != LightKind::Brazier || !l.lit) continue;
                const i32 lx = static_cast<i32>(l.pos.x) / kTilePx, ly = static_cast<i32>(l.pos.y) / kTilePx;
                for (usize k = s; k < e && !covered; ++k)
                    covered = sq(g.ix(f.criticalPath[k]) - lx) + sq(g.iy(f.criticalPath[k]) - ly) <= 900;
                if (covered) break;
            }
            if (!covered) return g.fail("rule 5: dark stretch on the critical path");
        }
    }

    // 6. Light cap, open-tile count within ±15 % of the target.
    if (f.staticLights.size() > kMaxStaticLights) return g.fail("rule 6: too many static lights");
    u32 open = 0, walkable = 0;
    for (Tile t : f.tiles) {
        open += isOpen(t) ? 1u : 0u;
        walkable += isWalkable(t) ? 1u : 0u;
    }
    const u64 target = static_cast<u64>(f.w) * f.h * targetOpenPerMille(f.w) / 1000u;
    if (static_cast<u64>(open) * 100u < target * 85u || static_cast<u64>(open) * 100u > target * 115u)
        return g.fail("rule 6: open-tile count off target");

    // Metrics (§6).
    Metrics& m = f.metrics;
    m.floorTiles = open;
    m.rooms = static_cast<u32>(f.rooms.size());
    m.loops = 0;
    for (const RoomEdge& e : f.edges) m.loops += e.loop ? 1u : 0u;
    m.criticalPathLen = static_cast<u32>(f.criticalPath.size());
    u32 lit = 0;
    for (usize i = 0; i < f.tiles.size(); ++i) {
        if (!isWalkable(f.tiles[i])) continue;
        const i64 px = g.ix(static_cast<u32>(i)) * kTilePx + kTilePx / 2, py = g.iy(static_cast<u32>(i)) * kTilePx + kTilePx / 2;
        for (const Light& l : f.staticLights) {
            if (!l.lit) continue;
            const i64 r = static_cast<i64>(l.radius);
            if (sq(static_cast<i64>(l.pos.x) - px) + sq(static_cast<i64>(l.pos.y) - py) <= r * r) { ++lit; break; }
        }
    }
    m.litAreaFraction = walkable == 0 ? 0.f : static_cast<f32>(static_cast<f64>(lit) / static_cast<f64>(walkable));
    m.darkPocketArea = 0;
    for (const SpawnZone& z : f.spawnZones) m.darkPocketArea += z.kind == SpawnKind::DarkPocket ? z.cellCount : 0u;
    m.nestCount = 0;
    m.mirrorCount = 0;
    for (const Prop& p : f.props) {
        m.nestCount += p.kind == PropKind::Nest ? 1u : 0u;
        m.mirrorCount += p.kind == PropKind::Mirror ? 1u : 0u;
    }
    return true;
}

} // namespace lb::procgen::detail
