// Seal puzzle (docs/02 §3 stage 5, §5 rule 3). A mirror is a two-sided line in one of 4
// orientations (m × 45°); a beam in direction d (8 directions, 45° steps) leaves it in 2m − d. So
// a mirror turns a beam by 90° (or reverses it) and axis beams stay axis beams. The path is built
// backwards from the sigil: the last segment ends at the sigil, each mirror turns the beam by
// 90°, and the first segment starts at the crystal in the PUZZLE room. Then every mirror is set to
// a wrong orientation, and a BFS over orientation states checks that a solution needs at most 8
// single +45° rotations (DECISIONS #34: rotation is one-directional, 4 states per mirror).
#include "procgen/Internal.h"

#include <algorithm>

namespace lb::procgen {
namespace detail {
namespace {

constexpr i32 kMaxBounces = 4;  // GDD §6 (base kit)

struct MirrorRef {
    u32 tile;
    u8 orient;
};

u8 reflect(u8 d, u8 m) { return static_cast<u8>((2u * m + 8u - d) & 7u); }

/// The beam's step from (x, y) in direction d is blocked by a wall corner (diagonal through two walls).
bool cornerBlocked(const FloorData& f, i32 x, i32 y, u8 d) {
    if ((d & 1u) == 0) return false;
    return blocksLight(f.at(x + kDx8[d], y)) && blocksLight(f.at(x, y + kDy8[d]));
}

/// Bounces when the beam enters the sigil tile, -1 when it stops elsewhere.
i32 traceBeam(const FloorData& f, u32 source, u8 dir, u32 sigil, const std::vector<MirrorRef>& mirrors) {
    const i32 w = static_cast<i32>(f.w), h = static_cast<i32>(f.h);
    i32 x = static_cast<i32>(source) % w, y = static_cast<i32>(source) / w;
    u8 d = dir;
    i32 bounces = 0;
    for (i32 steps = 0; steps < 4 * (w + h); ++steps) {
        if (cornerBlocked(f, x, y, d)) return -1;
        const i32 nx = x + kDx8[d], ny = y + kDy8[d];
        if (nx < 0 || ny < 0 || nx >= w || ny >= h) return -1;
        const u32 nt = static_cast<u32>(ny * w + nx);
        if (nt == sigil) return bounces;
        if (blocksLight(f.tiles[nt])) return -1;
        x = nx;
        y = ny;
        for (const MirrorRef& m : mirrors) {
            if (m.tile != nt) continue;
            const u8 nd = reflect(d, m.orient);
            if (nd != d) {
                if (++bounces > kMaxBounces) return -1;
                d = nd;
            }
            break;
        }
    }
    return -1;
}

struct Builder {
    Gen& g;
    u16 puzzleRoom;
    std::vector<u8> onPath, isMirror;
    std::vector<MirrorRef> chain;  // source side first once complete (pushed while unwinding)
    u32 source = 0;
    u8 sourceDir = 0;
    i32 budget = 0;

    Builder(Gen& gen, u16 room) : g(gen), puzzleRoom(room) {
        onPath.assign(g.f.tiles.size(), 0);
        isMirror.assign(g.f.tiles.size(), 0);
    }

    bool mirrorNear(i32 x, i32 y) const {
        for (i32 oy = -1; oy <= 1; ++oy)
            for (i32 ox = -1; ox <= 1; ++ox)
                if (g.inside(x + ox, y + oy) && isMirror[g.idx(x + ox, y + oy)] != 0) return true;
        return false;
    }

    /// The beam travels in direction d and ends at tile `target`; find what feeds it.
    bool back(u32 target, u8 d, i32 remaining) {
        const FloorData& f = g.f;
        std::vector<u32> ray;  // ray[j - 1] = target − j·dir
        i32 x = g.ix(target), y = g.iy(target);
        for (;;) {
            const i32 px = x - kDx8[d], py = y - kDy8[d];
            if (!g.inside(px, py) || blocksLight(f.at(px, py)) || cornerBlocked(f, px, py, d)) break;
            const u32 pi = g.idx(px, py);
            if (isMirror[pi] != 0) break;
            ray.push_back(pi);
            x = px;
            y = py;
        }
        std::vector<u32> picks;
        for (usize j = 1; j < ray.size(); ++j) {  // segment length >= 2
            const u32 c = ray[j];
            if (f.tiles[c] != Tile::Floor || onPath[c] != 0) continue;
            if (remaining == 0 ? f.regionId[c] != puzzleRoom : mirrorNear(g.ix(c), g.iy(c))) continue;
            picks.push_back(static_cast<u32>(j));
        }
        shuffle(picks, g.props);
        for (u32 j : picks) {
            if (--budget < 0) return false;
            const u32 c = ray[j];
            if (remaining == 0) {
                source = c;
                sourceDir = d;
                return true;
            }
            for (u32 k = 0; k < j; ++k) ++onPath[ray[k]];
            isMirror[c] = 1;
            const bool flip = g.props.nextBool();
            for (u8 turn : {flip ? u8{6} : u8{2}, flip ? u8{2} : u8{6}}) {
                const u8 din = static_cast<u8>((d + turn) & 7u);
                if (back(c, din, remaining - 1)) {
                    chain.push_back({c, static_cast<u8>(((d + din) & 7u) >> 1u)});
                    return true;
                }
                if (budget < 0) break;
            }
            isMirror[c] = 0;
            for (u32 k = 0; k < j; ++k) --onPath[ray[k]];
            if (budget < 0) return false;
        }
        return false;
    }
};

} // namespace

bool buildSealPuzzle(Gen& g) {
    FloorData& f = g.f;
    u16 puzzleRoom = kRegionNone;
    for (usize i = 0; i < f.rooms.size(); ++i)
        if (f.rooms[i].puzzle) puzzleRoom = static_cast<u16>(i);
    if (puzzleRoom == kRegionNone) return g.fail("puzzle: no puzzle room");
    const Room& pr = f.rooms[puzzleRoom];
    const Room& er = f.rooms[f.exitRoom];
    const i32 mirrorsWanted = f.depth >= 5 ? 3 : 2;

    // Sigil candidates: walls touching the EXIT room's floor, farthest from the PUZZLE room first.
    struct Cand { i64 d2; u32 tile; };
    std::vector<Cand> sigils;
    const i32 R2 = er.radius + 2;
    for (i32 y = std::max(0, er.cy - R2); y <= std::min(g.h - 1, er.cy + R2); ++y)
        for (i32 x = std::max(0, er.cx - R2); x <= std::min(g.w - 1, er.cx + R2); ++x) {
            if (f.tiles[g.idx(x, y)] != Tile::Wall) continue;
            bool touches = false;
            for (i32 k = 0; k < 4 && !touches; ++k) {
                const i32 nx = x + kDx4[k], ny = y + kDy4[k];
                touches = g.inside(nx, ny) && f.tiles[g.idx(nx, ny)] == Tile::Floor && f.regionId[g.idx(nx, ny)] == f.exitRoom;
            }
            if (touches) sigils.push_back({sq(x - pr.cx) + sq(y - pr.cy), g.idx(x, y)});
        }
    std::sort(sigils.begin(), sigils.end(), [](const Cand& a, const Cand& b) {
        return a.d2 != b.d2 ? a.d2 > b.d2 : a.tile < b.tile;
    });

    Builder b(g, puzzleRoom);
    b.budget = 40000;
    u32 sigil = UINT32_MAX;
    for (usize s = 0; s < sigils.size() && s < 24 && sigil == UINT32_MAX && b.budget > 0; ++s) {
        std::vector<u8> dirs{0, 1, 2, 3, 4, 5, 6, 7};
        shuffle(dirs, g.props);
        for (u8 d : dirs) {
            b.chain.clear();
            if (b.back(sigils[s].tile, d, mirrorsWanted)) {
                sigil = sigils[s].tile;
                break;
            }
            if (b.budget <= 0) break;
        }
    }
    if (sigil == UINT32_MAX) return g.fail("puzzle: no beam path");

    // chain was pushed while unwinding: source side first. Check the solved state forward.
    std::vector<MirrorRef> solved(b.chain.begin(), b.chain.end());
    if (traceBeam(f, b.source, b.sourceDir, sigil, solved) != mirrorsWanted) return g.fail("puzzle: forward check");

    Prop sp;
    sp.kind = PropKind::Sigil;
    sp.tile = sigil;
    sp.pos = tileCenterPx(g.ix(sigil), g.iy(sigil));
    sp.room = static_cast<u16>(f.exitRoom);
    f.props.push_back(sp);
    for (const MirrorRef& m : solved) {
        Prop p;
        p.kind = PropKind::Mirror;
        p.tile = m.tile;
        p.pos = tileCenterPx(g.ix(m.tile), g.iy(m.tile));
        p.room = f.regionId[m.tile];
        p.solution = m.orient;
        p.orientation = static_cast<u8>((m.orient + 1u + g.props.nextBounded(3)) & 3u);
        f.props.push_back(p);
    }
    Prop src;
    src.kind = PropKind::BeamSource;
    src.tile = b.source;
    src.pos = tileCenterPx(g.ix(b.source), g.iy(b.source));
    src.room = puzzleRoom;
    src.variant = b.sourceDir;
    f.props.push_back(src);
    Light l;
    l.pos = src.pos;
    l.radius = 40.f;
    l.kind = LightKind::Crystal;
    l.lit = true;
    l.priority = 0;
    l.beamDir = b.sourceDir;
    l.room = puzzleRoom;
    f.staticLights.push_back(l);

    if (solveSealPuzzle(f, 8) < 1) return g.fail("puzzle: not solvable in 1..8 rotations");
    return true;
}

} // namespace detail

i32 solveSealPuzzle(const FloorData& f, i32 maxDepth) {
    using namespace detail;
    u32 source = UINT32_MAX, sigil = UINT32_MAX;
    u8 dir = 0;
    std::vector<MirrorRef> mirrors;
    for (const Prop& p : f.props) {
        if (p.kind == PropKind::BeamSource) { source = p.tile; dir = p.variant; }
        if (p.kind == PropKind::Sigil) sigil = p.tile;
        if (p.kind == PropKind::Mirror) mirrors.push_back({p.tile, p.orientation});
    }
    if (source == UINT32_MAX || sigil == UINT32_MAX || mirrors.empty() || mirrors.size() > 8) return -1;
    const usize k = mirrors.size();
    const u32 states = 1u << (2u * k);
    auto encode = [&](const std::vector<MirrorRef>& m) {
        u32 s = 0;
        for (usize i = 0; i < k; ++i) s |= static_cast<u32>(m[i].orient) << (2u * i);
        return s;
    };
    std::vector<i32> dist(states, -1);
    std::vector<u32> queue{encode(mirrors)};
    dist[queue[0]] = 0;
    std::vector<MirrorRef> cur = mirrors;
    for (usize head = 0; head < queue.size(); ++head) {
        const u32 s = queue[head];
        if (dist[s] > maxDepth) break;
        for (usize i = 0; i < k; ++i) cur[i].orient = static_cast<u8>((s >> (2u * i)) & 3u);
        if (traceBeam(f, source, dir, sigil, cur) >= 0) return dist[s];
        for (usize i = 0; i < k; ++i) {
            const u32 o = (s >> (2u * i)) & 3u;
            const u32 n = (s & ~(3u << (2u * i))) | (((o + 1u) & 3u) << (2u * i));
            if (dist[n] < 0) {
                dist[n] = dist[s] + 1;
                queue.push_back(n);
            }
        }
    }
    return -1;
}

} // namespace lb::procgen
