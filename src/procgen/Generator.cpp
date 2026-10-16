// Floor generator: orchestration and stages 1, 2, 4, 5, 6 (docs/02 §3). Stage 3 is in Sdf.cpp,
// the seal puzzle in Puzzle.cpp, validation and metrics in Validate.cpp.
#include "procgen/Internal.h"

#include "core/Time.h"

#include <algorithm>
#include <cstdlib>

namespace lb::procgen {

using namespace detail;

u32 floorSizeFor(const FloorRequest& req) {
    if (req.sizeOverride != 0) return req.sizeOverride;
    const u32 d = std::clamp(req.depth, 1u, 5u);
    return 120u + 20u * (d - 1u);
}

Biome biomeFor(u32 depth) {
    switch (depth) {
    case 1: return Biome::MossyHollow;
    case 2: return Biome::CrystalVein;
    case 3: return Biome::CrystalVeinDeep;
    case 4: return Biome::EmberDeep;
    default: return Biome::Abyss;
    }
}

const char* biomeName(Biome b) {
    switch (b) {
    case Biome::MossyHollow: return "mossy_hollow";
    case Biome::CrystalVein: return "crystal_vein";
    case Biome::CrystalVeinDeep: return "crystal_vein_deep";
    case Biome::EmberDeep: return "ember_deep";
    case Biome::Abyss: return "abyss";
    }
    return "?";
}

const char* roomRoleName(RoomRole r) {
    switch (r) {
    case RoomRole::Cave: return "cave";
    case RoomRole::Start: return "start";
    case RoomRole::Exit: return "exit";
    case RoomRole::Seal: return "seal";
    case RoomRole::ShrineAntechamber: return "antechamber";
    case RoomRole::Nest: return "nest";
    case RoomRole::Cache: return "cache";
    }
    return "?";
}

namespace detail {

Gen::Gen(FloorData& out, u64 seed, i32 width, i32 height)
    : f(out), floorSeed(seed), w(width), h(height),
      layout(namedStream(seed, "layout")), caves(namedStream(seed, "caves")), lights(namedStream(seed, "lights")),
      props(namedStream(seed, "props")), spawns(namedStream(seed, "spawns")), deco(namedStream(seed, "deco")) {}

u32 walkableBfs(const FloorData& f, u32 start, std::vector<i32>& dist) {
    const i32 w = static_cast<i32>(f.w), h = static_cast<i32>(f.h);
    dist.assign(static_cast<usize>(w) * static_cast<usize>(h), -1);
    if (!isWalkable(f.tiles[start])) return 0;
    std::vector<u32> queue;
    queue.reserve(dist.size());
    queue.push_back(start);
    dist[start] = 0;
    for (usize head = 0; head < queue.size(); ++head) {
        const u32 c = queue[head];
        const i32 x = static_cast<i32>(c) % w, y = static_cast<i32>(c) / w;
        for (i32 k = 0; k < 4; ++k) {
            const i32 nx = x + kDx4[k], ny = y + kDy4[k];
            if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
            const u32 n = static_cast<u32>(ny * w + nx);
            if (dist[n] >= 0 || !isWalkable(f.tiles[n])) continue;
            dist[n] = dist[c] + 1;
            queue.push_back(n);
        }
    }
    return static_cast<u32>(queue.size());
}

// ---------------------------------------------------------------------------------------- stage 1
namespace {

i64 orient(const Room& a, const Room& b, const Room& c) {
    return static_cast<i64>(b.cx - a.cx) * (c.cy - a.cy) - static_cast<i64>(b.cy - a.cy) * (c.cx - a.cx);
}

/// > 0 when d lies strictly inside the circumcircle of (a, b, c), which must have orient > 0.
i64 incircle(const Room& a, const Room& b, const Room& c, const Room& d) {
    const i64 adx = a.cx - d.cx, ady = a.cy - d.cy;
    const i64 bdx = b.cx - d.cx, bdy = b.cy - d.cy;
    const i64 cdx = c.cx - d.cx, cdy = c.cy - d.cy;
    return (adx * adx + ady * ady) * (bdx * cdy - cdx * bdy) + (bdx * bdx + bdy * bdy) * (cdx * ady - adx * cdy) +
           (cdx * cdx + cdy * cdy) * (adx * bdy - bdx * ady);
}

struct UnionFind {
    std::vector<u32> parent;
    explicit UnionFind(usize n) : parent(n) {
        for (usize i = 0; i < n; ++i) parent[i] = static_cast<u32>(i);
    }
    u32 find(u32 x) {
        while (parent[x] != x) x = parent[x] = parent[parent[x]];
        return x;
    }
    bool unite(u32 a, u32 b) {
        a = find(a);
        b = find(b);
        if (a == b) return false;
        if (a < b) parent[b] = a; else parent[a] = b;
        return true;
    }
};

struct CandEdge {
    i64 d2;
    u16 a, b;
};

bool edgeLess(const CandEdge& x, const CandEdge& y) {
    if (x.d2 != y.d2) return x.d2 < y.d2;
    if (x.a != y.a) return x.a < y.a;
    return x.b < y.b;
}

} // namespace

bool stageLayout(Gen& g) {
    FloorData& f = g.f;
    std::vector<Room>& rooms = f.rooms;

    // Poisson-disc room centers (dart throwing on the integer grid), min distance 22 tiles.
    const u32 target = static_cast<u32>(g.w * g.h) / 900u;
    constexpr i32 kMargin = 14;  // max carve radius 12 + 2
    constexpr i64 kMinD2 = 22 * 22;
    const u32 spanX = static_cast<u32>(g.w - 2 * kMargin), spanY = static_cast<u32>(g.h - 2 * kMargin);
    for (u32 tries = target * 300u; rooms.size() < target && tries > 0; --tries) {
        Room r;
        r.cx = kMargin + static_cast<i32>(g.layout.nextBounded(spanX));
        r.cy = kMargin + static_cast<i32>(g.layout.nextBounded(spanY));
        bool ok = true;
        for (const Room& o : rooms) {
            if (sq(r.cx - o.cx) + sq(r.cy - o.cy) < kMinD2) { ok = false; break; }
        }
        if (ok) rooms.push_back(r);
    }
    const usize n = rooms.size();
    if (n < 8) return g.fail("layout: fewer than 8 rooms");

    // Delaunay triangulation by the empty-circumcircle test (exact integer predicates; n <= ~50).
    std::vector<u8> adj(n * n, 0);
    for (usize i = 0; i < n; ++i)
        for (usize j = i + 1; j < n; ++j)
            for (usize k = j + 1; k < n; ++k) {
                const i64 o = orient(rooms[i], rooms[j], rooms[k]);
                if (o == 0) continue;
                const Room& a = rooms[i];
                const Room& b = o > 0 ? rooms[j] : rooms[k];
                const Room& c = o > 0 ? rooms[k] : rooms[j];
                bool empty = true;
                for (usize l = 0; l < n && empty; ++l) {
                    if (l == i || l == j || l == k) continue;
                    if (incircle(a, b, c, rooms[l]) > 0) empty = false;
                }
                if (!empty) continue;
                adj[i * n + j] = adj[j * n + i] = 1;
                adj[i * n + k] = adj[k * n + i] = 1;
                adj[j * n + k] = adj[k * n + j] = 1;
            }

    std::vector<CandEdge> cands;
    for (usize i = 0; i < n; ++i)
        for (usize j = i + 1; j < n; ++j)
            if (adj[i * n + j] != 0)
                cands.push_back({sq(rooms[i].cx - rooms[j].cx) + sq(rooms[i].cy - rooms[j].cy), static_cast<u16>(i),
                                 static_cast<u16>(j)});
    std::sort(cands.begin(), cands.end(), edgeLess);

    // Minimum spanning tree (Kruskal), then 15 % of the remaining Delaunay edges back as loops.
    UnionFind uf(n);
    std::vector<CandEdge> rest;
    u32 treeEdges = 0;
    for (const CandEdge& e : cands) {
        if (uf.unite(e.a, e.b)) {
            f.edges.push_back({e.a, e.b, false});
            ++treeEdges;
        } else {
            rest.push_back(e);
        }
    }
    if (treeEdges + 1u < n) {  // degenerate triangulation: finish the tree over all pairs
        std::vector<CandEdge> all;
        for (usize i = 0; i < n; ++i)
            for (usize j = i + 1; j < n; ++j)
                all.push_back({sq(rooms[i].cx - rooms[j].cx) + sq(rooms[i].cy - rooms[j].cy), static_cast<u16>(i),
                               static_cast<u16>(j)});
        std::sort(all.begin(), all.end(), edgeLess);
        for (const CandEdge& e : all)
            if (uf.unite(e.a, e.b)) f.edges.push_back({e.a, e.b, false});
    }
    shuffle(rest, g.layout);
    const usize loops = (rest.size() * 15u + 50u) / 100u;
    for (usize i = 0; i < loops; ++i) f.edges.push_back({rest[i].a, rest[i].b, true});

    std::vector<std::vector<u16>> nbr(n);
    for (const RoomEdge& e : f.edges) {
        nbr[e.a].push_back(e.b);
        nbr[e.b].push_back(e.a);
    }
    for (usize i = 0; i < n; ++i) {
        std::sort(nbr[i].begin(), nbr[i].end());
        rooms[i].degree = static_cast<u8>(std::min<usize>(nbr[i].size(), 255u));
    }

    // START: lowest degree among the quarter of rooms nearest the map edge (ties: seeded).
    std::vector<u32> order(n);
    std::vector<i32> edgeDist(n);
    for (usize i = 0; i < n; ++i) {
        order[i] = static_cast<u32>(i);
        const Room& r = rooms[i];
        edgeDist[i] = std::min(std::min(r.cx, r.cy), std::min(g.w - 1 - r.cx, g.h - 1 - r.cy));
    }
    std::sort(order.begin(), order.end(), [&](u32 a, u32 b) {
        return edgeDist[a] != edgeDist[b] ? edgeDist[a] < edgeDist[b] : a < b;
    });
    const usize nearEdge = std::max<usize>(1u, n / 4u);
    u8 minDeg = 255;
    for (usize i = 0; i < nearEdge; ++i) minDeg = std::min(minDeg, rooms[order[i]].degree);
    std::vector<u32> ties;
    for (usize i = 0; i < nearEdge; ++i)
        if (rooms[order[i]].degree == minDeg) ties.push_back(order[i]);
    const u32 start = ties[g.layout.nextBounded(static_cast<u32>(ties.size()))];

    // Graph distance from START.
    std::vector<i32> hops(n, -1);
    std::vector<u32> q{start};
    hops[start] = 0;
    for (usize head = 0; head < q.size(); ++head)
        for (u16 m : nbr[q[head]])
            if (hops[m] < 0) {
                hops[m] = hops[q[head]] + 1;
                q.push_back(m);
            }
    for (usize i = 0; i < n; ++i) rooms[i].hops = static_cast<u8>(std::clamp(hops[i], 0, 255));

    // EXIT: farthest in hops (ties: farthest in space, then lowest index).
    u32 exit = start;
    for (usize i = 0; i < n; ++i) {
        const i64 di = sq(rooms[i].cx - rooms[start].cx) + sq(rooms[i].cy - rooms[start].cy);
        const i64 de = sq(rooms[exit].cx - rooms[start].cx) + sq(rooms[exit].cy - rooms[start].cy);
        if (hops[i] > hops[exit] || (hops[i] == hops[exit] && di > de)) exit = static_cast<u32>(i);
    }
    if (hops[exit] < 2) return g.fail("layout: exit adjacent to start");
    u32 ante = nbr[exit][0];
    for (u16 m : nbr[exit])
        if (hops[m] < hops[ante]) ante = m;

    f.seal = (f.depth == 3 || f.depth == 5);
    rooms[start].role = RoomRole::Start;
    rooms[exit].role = f.seal ? RoomRole::Seal : RoomRole::Exit;
    rooms[ante].role = RoomRole::ShrineAntechamber;
    rooms[ante].puzzle = f.seal;
    f.startRoom = start;
    f.exitRoom = exit;

    // NEST rooms (2 + d): prefer high degree, then far from START; seeded tie-break.
    std::vector<u32> key(n);
    for (usize i = 0; i < n; ++i) key[i] = g.layout.next();
    auto nestLess = [&](u32 a, u32 b) {
        if (rooms[a].degree != rooms[b].degree) return rooms[a].degree > rooms[b].degree;
        if (hops[a] != hops[b]) return hops[a] > hops[b];
        return key[a] < key[b];
    };
    const u32 nestTarget = 2u + f.depth;
    for (i32 minHops = 2; minHops >= 1; --minHops) {
        std::vector<u32> el;
        for (usize i = 0; i < n; ++i)
            if (rooms[i].role == RoomRole::Cave && hops[i] >= minHops) el.push_back(static_cast<u32>(i));
        std::sort(el.begin(), el.end(), nestLess);
        u32 have = 0;
        for (const Room& r : rooms) have += (r.role == RoomRole::Nest) ? 1u : 0u;
        for (usize i = 0; i < el.size() && have < nestTarget; ++i, ++have) rooms[el[i]].role = RoomRole::Nest;
        if (have >= nestTarget) break;
    }

    // CACHE rooms (3–5), random among the remaining caves.
    const u32 cacheTarget = static_cast<u32>(g.layout.nextRange(3, 5));
    std::vector<u32> caves;
    for (usize i = 0; i < n; ++i)
        if (rooms[i].role == RoomRole::Cave) caves.push_back(static_cast<u32>(i));
    shuffle(caves, g.layout);
    for (usize i = 0; i < caves.size() && i < cacheTarget; ++i) rooms[caves[i]].role = RoomRole::Cache;
    return true;
}

// ---------------------------------------------------------------------------------------- stage 2
namespace {

u32 waterPercent(Biome b) { return b == Biome::MossyHollow ? 40u : 20u; }  // DECISIONS #34

/// One cellular-automaton step of rule B5678/S45678 on walls: returns true when the cell is a wall.
bool caRule(bool wall, i32 wallNeighbors) { return wall ? wallNeighbors >= 4 : wallNeighbors >= 5; }

} // namespace

bool stageCaves(Gen& g) {
    FloorData& f = g.f;
    const usize cells = static_cast<usize>(g.w) * static_cast<usize>(g.h);
    f.tiles.assign(cells, Tile::Wall);
    f.regionId.assign(cells, kRegionNone);
    std::vector<u8> protect(cells, 0);  // room cores and corridor centerlines never turn into walls

    // Room blobs: CA with 45 % initial fill, radial falloff from 0.8 R to 70 % at the radius, 5 iterations.
    std::vector<u8> blob, next;
    for (Room& r : f.rooms) {
        const i32 R = g.caves.nextRange(6, 12);
        r.radius = R;
        const i32 side = 2 * R + 1;
        const i64 R2 = sq(R), core2 = (R2 * 16) / 25;  // falloff starts at 0.8 R
        blob.assign(static_cast<usize>(side * side), 1);
        for (i32 ly = 0; ly < side; ++ly)
            for (i32 lx = 0; lx < side; ++lx) {
                const i64 d2 = sq(lx - R) + sq(ly - R);
                if (d2 > R2) continue;
                const i64 fill = d2 <= core2 ? 45 : 45 + (25 * (d2 - core2)) / std::max<i64>(1, R2 - core2);
                blob[static_cast<usize>(ly * side + lx)] = g.caves.nextBounded(100) < static_cast<u32>(fill) ? 1 : 0;
            }
        for (i32 iter = 0; iter < 5; ++iter) {
            next = blob;
            for (i32 ly = 0; ly < side; ++ly)
                for (i32 lx = 0; lx < side; ++lx) {
                    if (sq(lx - R) + sq(ly - R) > R2) continue;
                    i32 walls = 0;
                    for (i32 k = 0; k < 8; ++k) {
                        const i32 nx = lx + kDx8[k], ny = ly + kDy8[k];
                        walls += (nx < 0 || ny < 0 || nx >= side || ny >= side || blob[static_cast<usize>(ny * side + nx)] != 0) ? 1 : 0;
                    }
                    const usize li = static_cast<usize>(ly * side + lx);
                    next[li] = caRule(blob[li] != 0, walls) ? 1 : 0;
                }
            blob.swap(next);
        }
        for (i32 ly = 0; ly < side; ++ly)
            for (i32 lx = 0; lx < side; ++lx) {
                const i64 d2 = sq(lx - R) + sq(ly - R);
                const i32 x = r.cx + lx - R, y = r.cy + ly - R;
                if (d2 <= 5) protect[g.idx(x, y)] = 1;
                if (d2 <= 5 || (d2 <= R2 && blob[static_cast<usize>(ly * side + lx)] == 0)) g.t(x, y) = Tile::Floor;
            }
    }

    // Corridors: biased random walk (0.7) along every graph edge, width 2–4, one CA pass along it.
    std::vector<u32> stamp(cells, 0);
    std::vector<Tile> snap;
    for (usize ci = 0; ci < f.edges.size(); ++ci) {
        const u32 id = static_cast<u32>(ci + 1);
        const Room& a = f.rooms[f.edges[ci].a];
        const Room& b = f.rooms[f.edges[ci].b];
        const i32 width = g.caves.nextRange(2, 4);
        const i32 lo = -(width - 1) / 2, hi = width / 2;
        i32 x = a.cx, y = a.cy;
        i32 minX = x, maxX = x, minY = y, maxY = y;
        auto carve = [&](i32 cx, i32 cy) {
            protect[g.idx(cx, cy)] = 1;
            for (i32 oy = lo; oy <= hi; ++oy)
                for (i32 ox = lo; ox <= hi; ++ox) {
                    const i32 px = std::clamp(cx + ox, 1, g.w - 2), py = std::clamp(cy + oy, 1, g.h - 2);
                    g.t(px, py) = Tile::Floor;
                    stamp[g.idx(px, py)] = id;
                    minX = std::min(minX, px); maxX = std::max(maxX, px);
                    minY = std::min(minY, py); maxY = std::max(maxY, py);
                }
        };
        carve(x, y);
        const i32 cap = 4 * (std::abs(b.cx - a.cx) + std::abs(b.cy - a.cy)) + 64;
        for (i32 step = 0; x != b.cx || y != b.cy; ++step) {
            const i32 dx = b.cx - x, dy = b.cy - y;
            if (step >= cap) {
                if (dx != 0) x += dx > 0 ? 1 : -1; else y += dy > 0 ? 1 : -1;
            } else if (g.caves.nextBounded(100) < 70) {
                const u32 adx = static_cast<u32>(std::abs(dx)), ady = static_cast<u32>(std::abs(dy));
                const bool alongX = ady == 0 || (adx != 0 && g.caves.nextBounded(adx + ady) < adx);
                if (alongX) x += dx > 0 ? 1 : -1; else y += dy > 0 ? 1 : -1;
            } else {
                const u32 d = g.caves.nextBounded(4);
                x = std::clamp(x + kDx4[d], 2, g.w - 3);
                y = std::clamp(y + kDy4[d], 2, g.h - 3);
            }
            carve(x, y);
        }
        // One smoothing pass over the corridor and its 8-neighborhood.
        minX = std::max(1, minX - 1); minY = std::max(1, minY - 1);
        maxX = std::min(g.w - 2, maxX + 1); maxY = std::min(g.h - 2, maxY + 1);
        const i32 bw = maxX - minX + 1;
        snap.resize(static_cast<usize>(bw * (maxY - minY + 1)));
        for (i32 yy = minY; yy <= maxY; ++yy)
            for (i32 xx = minX; xx <= maxX; ++xx) snap[static_cast<usize>((yy - minY) * bw + (xx - minX))] = g.at(xx, yy);
        auto snapAt = [&](i32 xx, i32 yy) {
            if (xx < minX || yy < minY || xx > maxX || yy > maxY) return g.at(xx, yy);
            return snap[static_cast<usize>((yy - minY) * bw + (xx - minX))];
        };
        for (i32 yy = minY; yy <= maxY; ++yy)
            for (i32 xx = minX; xx <= maxX; ++xx) {
                bool near = false;
                for (i32 k = 0; k < 8 && !near; ++k) near = stamp[g.idx(xx + kDx8[k], yy + kDy8[k])] == id;
                if (!near && stamp[g.idx(xx, yy)] != id) continue;
                i32 walls = 0;
                for (i32 k = 0; k < 8; ++k) walls += blocksLight(snapAt(xx + kDx8[k], yy + kDy8[k])) ? 1 : 0;
                const bool wall = caRule(blocksLight(snapAt(xx, yy)), walls);
                if (wall && protect[g.idx(xx, yy)] == 0) g.t(xx, yy) = Tile::Wall;
                else if (!wall) g.t(xx, yy) = Tile::Floor;
            }
    }

    // Shallow water pools in a biome-dependent share of the caves (walkable).
    for (Room& r : f.rooms) {
        if (r.role != RoomRole::Cave || g.caves.nextBounded(100) >= waterPercent(f.biome)) continue;
        const i32 reach = r.radius - 2;
        for (i32 attempt = 0; attempt < 16; ++attempt) {
            const i32 ox = g.caves.nextRange(-reach, reach), oy = g.caves.nextRange(-reach, reach);
            const i64 d2 = sq(ox) + sq(oy);
            if (d2 < 9 || d2 > sq(reach) || g.at(r.cx + ox, r.cy + oy) != Tile::Floor) continue;
            const i32 pr = g.caves.nextRange(2, 3);
            for (i32 yy = -pr; yy <= pr; ++yy)
                for (i32 xx = -pr; xx <= pr; ++xx) {
                    const i32 px = r.cx + ox + xx, py = r.cy + oy + yy;
                    if (sq(xx) + sq(yy) <= sq(pr) && sq(px - r.cx) + sq(py - r.cy) <= sq(r.radius) &&
                        g.at(px, py) == Tile::Floor)
                        g.t(px, py) = Tile::Water;
                }
            break;
        }
    }

    // Remove open space not connected to START.
    const Room& sr = f.rooms[f.startRoom];
    {
        std::vector<u8> seen(cells, 0);
        std::vector<u32> queue{g.idx(sr.cx, sr.cy)};
        seen[queue[0]] = 1;
        for (usize head = 0; head < queue.size(); ++head) {
            const i32 x = g.ix(queue[head]), y = g.iy(queue[head]);
            for (i32 k = 0; k < 4; ++k) {
                const i32 nx = x + kDx4[k], ny = y + kDy4[k];
                if (!g.inside(nx, ny)) continue;
                const u32 ni = g.idx(nx, ny);
                if (seen[ni] == 0 && isOpen(f.tiles[ni])) {
                    seen[ni] = 1;
                    queue.push_back(ni);
                }
            }
        }
        for (usize i = 0; i < cells; ++i)
            if (seen[i] == 0) f.tiles[i] = Tile::Wall;
    }

    // Abyss chasms (not walkable): kept only if every walkable tile stays reachable.
    if (f.biome == Biome::Abyss) {
        std::vector<i32> dist;
        u32 walkable = 0;
        for (Tile t : f.tiles) walkable += isWalkable(t) ? 1u : 0u;
        std::vector<u32> changed;
        for (Room& r : f.rooms) {
            if (r.role != RoomRole::Cave || g.caves.nextBounded(100) >= 35) continue;
            const i32 reach = r.radius - 2;
            const i32 ox = g.caves.nextRange(-reach, reach), oy = g.caves.nextRange(-reach, reach);
            const i32 pr = g.caves.nextRange(2, 3);
            if (sq(ox) + sq(oy) < 16 || sq(ox) + sq(oy) > sq(reach)) continue;
            changed.clear();
            for (i32 yy = -pr; yy <= pr; ++yy)
                for (i32 xx = -pr; xx <= pr; ++xx) {
                    const i32 px = r.cx + ox + xx, py = r.cy + oy + yy;
                    if (sq(xx) + sq(yy) > sq(pr) || !g.inside(px, py)) continue;
                    const u32 pi = g.idx(px, py);
                    if (protect[pi] == 0 && isWalkable(f.tiles[pi])) changed.push_back(pi);
                }
            if (changed.empty()) continue;
            std::vector<Tile> before;
            for (u32 pi : changed) {
                before.push_back(f.tiles[pi]);
                f.tiles[pi] = Tile::Chasm;
            }
            const u32 remaining = walkable - static_cast<u32>(changed.size());
            if (walkableBfs(f, g.idx(sr.cx, sr.cy), dist) == remaining) {
                walkable = remaining;
            } else {
                for (usize k = 0; k < changed.size(); ++k) f.tiles[changed[k]] = before[k];
            }
        }
    }

    // Regions: open tiles within radius + 1 of a room center belong to the nearest center.
    std::vector<i64> best(cells, INT64_MAX);
    for (usize ri = 0; ri < f.rooms.size(); ++ri) {
        const Room& r = f.rooms[ri];
        const i32 R1 = r.radius + 1;
        for (i32 y = std::max(0, r.cy - R1); y <= std::min(g.h - 1, r.cy + R1); ++y)
            for (i32 x = std::max(0, r.cx - R1); x <= std::min(g.w - 1, r.cx + R1); ++x) {
                const i64 d2 = sq(x - r.cx) + sq(y - r.cy);
                const u32 i = g.idx(x, y);
                if (d2 <= sq(R1) && isOpen(f.tiles[i]) && d2 < best[i]) {
                    best[i] = d2;
                    f.regionId[i] = static_cast<u16>(ri);
                }
            }
    }
    return true;
}

// ---------------------------------------------------------------------------------------- stage 4
namespace {

constexpr i64 kBrazierMinD2 = 24 * 24;  // sdf ∈ [1.5, 3] tiles at the tile center, in px²
constexpr i64 kBrazierMaxD2 = 48 * 48;

/// Floor tiles of a room, in tile-index order.
std::vector<u32> roomFloor(const Gen& g, u16 room) {
    std::vector<u32> out;
    const Room& r = g.f.rooms[room];
    const i32 R1 = r.radius + 1;
    for (i32 y = std::max(0, r.cy - R1); y <= std::min(g.h - 1, r.cy + R1); ++y)
        for (i32 x = std::max(0, r.cx - R1); x <= std::min(g.w - 1, r.cx + R1); ++x) {
            const u32 i = g.idx(x, y);
            if (g.f.regionId[i] == room && g.f.tiles[i] == Tile::Floor) out.push_back(i);
        }
    return out;
}

Light makeLight(const Gen& g, u32 tile, LightKind kind, u8 priority) {
    Light l;
    l.pos = tileCenterPx(g.ix(tile), g.iy(tile));
    l.kind = kind;
    l.radius = kind == LightKind::Brazier ? 80.f : 40.f;
    l.lit = kind == LightKind::Crystal;
    l.priority = priority;
    l.room = g.f.regionId[tile];
    return l;
}

void placeCrystalCluster(Gen& g, u16 room) {
    FloorData& f = g.f;
    const Room& r = f.rooms[room];
    auto touchesRoom = [&](i32 x, i32 y) {
        for (i32 k = 0; k < 4; ++k) {
            const i32 nx = x + kDx4[k], ny = y + kDy4[k];
            if (g.inside(nx, ny) && f.regionId[g.idx(nx, ny)] == room && isOpen(f.tiles[g.idx(nx, ny)])) return true;
        }
        return false;
    };
    std::vector<u32> seeds;
    const i32 R2 = r.radius + 2;
    for (i32 y = std::max(0, r.cy - R2); y <= std::min(g.h - 1, r.cy + R2); ++y)
        for (i32 x = std::max(0, r.cx - R2); x <= std::min(g.w - 1, r.cx + R2); ++x)
            if (f.tiles[g.idx(x, y)] == Tile::Wall && touchesRoom(x, y)) seeds.push_back(g.idx(x, y));
    if (seeds.empty()) return;
    const u32 seed = seeds[g.lights.nextBounded(static_cast<u32>(seeds.size()))];
    const i32 size = g.lights.nextRange(2, 4);
    std::vector<u32> cluster{seed};
    f.tiles[seed] = Tile::CrystalWall;
    for (usize head = 0; head < cluster.size() && static_cast<i32>(cluster.size()) < size; ++head)
        for (i32 k = 0; k < 4 && static_cast<i32>(cluster.size()) < size; ++k) {
            const i32 nx = g.ix(cluster[head]) + kDx4[k], ny = g.iy(cluster[head]) + kDy4[k];
            if (!g.inside(nx, ny) || f.tiles[g.idx(nx, ny)] != Tile::Wall || !touchesRoom(nx, ny)) continue;
            f.tiles[g.idx(nx, ny)] = Tile::CrystalWall;
            cluster.push_back(g.idx(nx, ny));
        }
    Light l = makeLight(g, seed, LightKind::Crystal, 2);
    l.room = room;
    f.staticLights.push_back(l);
}

} // namespace

bool stageLights(Gen& g) {
    FloorData& f = g.f;
    // One brazier per room near a wall; START's is essential and always lit.
    for (usize ri = 0; ri < f.rooms.size(); ++ri) {
        const std::vector<u32> floor = roomFloor(g, static_cast<u16>(ri));
        if (floor.empty()) return g.fail("lights: room without floor");
        std::vector<u32> cand;
        for (u32 i : floor)
            if (g.tileD2[i] >= kBrazierMinD2 && g.tileD2[i] <= kBrazierMaxD2) cand.push_back(i);
        const std::vector<u32>& pool = cand.empty() ? floor : cand;
        const u32 tile = pool[g.lights.nextBounded(static_cast<u32>(pool.size()))];
        const bool start = ri == f.startRoom;
        Light l = makeLight(g, tile, LightKind::Brazier, start ? 0 : 1);
        l.lit = start;
        f.staticLights.push_back(l);
    }

    // Crystals: clusters on walls in the Crystal biome, 0–3 elsewhere.
    const bool crystalBiome = f.biome == Biome::CrystalVein || f.biome == Biome::CrystalVeinDeep;
    if (crystalBiome) {
        for (usize ri = 0; ri < f.rooms.size(); ++ri)
            if (g.lights.nextBounded(100) < 50) placeCrystalCluster(g, static_cast<u16>(ri));
    } else {
        const i32 count = g.lights.nextRange(0, 3);
        for (i32 c = 0; c < count; ++c)
            placeCrystalCluster(g, static_cast<u16>(g.lights.nextBounded(static_cast<u32>(f.rooms.size()))));
    }

    // Corridor braziers: one per 20 corridor tiles, at least 10 tiles from every other brazier.
    u32 corridorTiles = 0;
    std::vector<u32> cand;
    for (usize i = 0; i < f.tiles.size(); ++i) {
        if (f.tiles[i] != Tile::Floor || f.regionId[i] != kRegionNone) continue;
        ++corridorTiles;
        if (g.tileD2[i] >= kBrazierMinD2 && g.tileD2[i] <= kBrazierMaxD2) cand.push_back(static_cast<u32>(i));
    }
    shuffle(cand, g.lights);
    u32 placed = 0;
    for (u32 tile : cand) {
        if (placed >= corridorTiles / 20u) break;
        const float2 p = tileCenterPx(g.ix(tile), g.iy(tile));
        bool spaced = true;
        for (const Light& l : f.staticLights) {
            if (l.kind != LightKind::Brazier) continue;
            const i64 dx = static_cast<i64>(l.pos.x - p.x) / kTilePx, dy = static_cast<i64>(l.pos.y - p.y) / kTilePx;
            if (dx * dx + dy * dy < 100) { spaced = false; break; }
        }
        if (!spaced) continue;
        f.staticLights.push_back(makeLight(g, tile, LightKind::Brazier, 3));
        ++placed;
    }
    return true;
}

// ---------------------------------------------------------------------------------------- stage 5
namespace {

Prop makeProp(const Gen& g, u32 tile, PropKind kind) {
    Prop p;
    p.pos = tileCenterPx(g.ix(tile), g.iy(tile));
    p.kind = kind;
    p.tile = tile;
    p.room = g.f.regionId[tile];
    return p;
}

bool nearProp(const Gen& g, u32 tile, i64 minD2Tiles) {
    for (const Prop& p : g.f.props)
        if (sq(g.ix(p.tile) - g.ix(tile)) + sq(g.iy(p.tile) - g.iy(tile)) < minD2Tiles) return true;
    return false;
}

/// Drops lowest-priority lights (3, then 2, then 1; the last generated first) down to the cap.
void capLights(FloorData& f) {
    for (u8 prio = 3; prio >= 1 && f.staticLights.size() > kMaxStaticLights; --prio)
        for (usize i = f.staticLights.size(); i-- > 0 && f.staticLights.size() > kMaxStaticLights;)
            if (f.staticLights[i].priority == prio) f.staticLights.erase(f.staticLights.begin() + static_cast<std::ptrdiff_t>(i));
}

} // namespace

bool stageProps(Gen& g) {
    FloorData& f = g.f;
    const Room& er = f.rooms[f.exitRoom];
    f.props.push_back(makeProp(g, g.idx(er.cx, er.cy), PropKind::ExitWell));

    // Caches (one per CACHE room) and 1–2 oil pools in CACHE rooms.
    std::vector<u16> cacheRooms;
    for (usize ri = 0; ri < f.rooms.size(); ++ri) {
        if (f.rooms[ri].role != RoomRole::Cache) continue;
        cacheRooms.push_back(static_cast<u16>(ri));
        const Room& r = f.rooms[ri];
        std::vector<u32> cand, any;
        for (u32 i : roomFloor(g, static_cast<u16>(ri))) {
            if (sq(g.ix(i) - r.cx) + sq(g.iy(i) - r.cy) < 4) continue;
            any.push_back(i);
            if (g.tileD2[i] >= kBrazierMinD2) cand.push_back(i);
        }
        const std::vector<u32>& pool = cand.empty() ? any : cand;
        if (pool.empty()) return g.fail("props: cache room without floor");
        Prop p = makeProp(g, pool[g.props.nextBounded(static_cast<u32>(pool.size()))], PropKind::Cache);
        const u32 roll = g.props.nextBounded(100);
        p.variant = static_cast<u8>(roll < 50 ? CacheContent::Oil : roll < 85 ? CacheContent::Flare : CacheContent::Heart);
        f.props.push_back(p);
    }
    const u32 oilTarget = static_cast<u32>(g.props.nextRange(1, 2));
    shuffle(cacheRooms, g.props);
    for (usize k = 0; k < cacheRooms.size() && k < oilTarget; ++k) {
        std::vector<u32> cand;
        for (u32 i : roomFloor(g, cacheRooms[k]))
            if (g.tileD2[i] >= kBrazierMinD2 && !nearProp(g, i, 4)) cand.push_back(i);
        if (cand.empty()) continue;
        f.props.push_back(makeProp(g, cand[g.props.nextBounded(static_cast<u32>(cand.size()))], PropKind::OilPool));
    }

    // Nests: nearest tile to the room center with sdf ≥ 3 tiles (else the most open tile).
    for (usize ri = 0; ri < f.rooms.size(); ++ri) {
        const Room& r = f.rooms[ri];
        if (r.role != RoomRole::Nest) continue;
        u32 bestOpen = UINT32_MAX, bestAny = UINT32_MAX;
        i64 bestOpenD2 = INT64_MAX, bestAnyClear = -1;
        for (u32 i : roomFloor(g, static_cast<u16>(ri))) {
            const i64 dc = sq(g.ix(i) - r.cx) + sq(g.iy(i) - r.cy);
            if (g.tileD2[i] >= kBrazierMaxD2 && dc < bestOpenD2) { bestOpenD2 = dc; bestOpen = i; }
            if (g.tileD2[i] > bestAnyClear) { bestAnyClear = g.tileD2[i]; bestAny = i; }
        }
        const u32 tile = bestOpen != UINT32_MAX ? bestOpen : bestAny;
        if (tile == UINT32_MAX) return g.fail("props: nest room without floor");
        f.props.push_back(makeProp(g, tile, PropKind::Nest));
    }

    if (f.seal && !buildSealPuzzle(g)) return false;

    // Light cap (R5), then pre-lit braziers: 0.6 − 0.12 (d − 1) of them; START's is always lit.
    capLights(f);
    std::vector<u32> braziers;
    u32 total = 0;
    for (usize i = 0; i < f.staticLights.size(); ++i) {
        if (f.staticLights[i].kind != LightKind::Brazier) continue;
        ++total;
        if (!f.staticLights[i].lit) braziers.push_back(static_cast<u32>(i));
    }
    const u32 pct = 60u - 12u * (f.depth - 1u);
    const u32 litTarget = std::max(1u, (total * pct + 50u) / 100u);
    shuffle(braziers, g.lights);
    for (usize k = 0; k + 1 < litTarget && k < braziers.size(); ++k) f.staticLights[braziers[k]].lit = true;

    // Decoration: blue noise (min 3 tiles apart) on floor tiles, visual only.
    u32 open = 0;
    for (Tile t : f.tiles) open += isOpen(t) ? 1u : 0u;
    const u32 decoTarget = open / 40u;
    std::vector<u32> decoTiles;
    for (u32 tries = decoTarget * 8u, placed = 0; tries > 0 && placed < decoTarget; --tries) {
        const u32 tile = g.deco.nextBounded(static_cast<u32>(f.tiles.size()));
        const u32 jx = g.deco.nextBounded(12), jy = g.deco.nextBounded(12), variant = g.deco.nextBounded(4);
        if (f.tiles[tile] != Tile::Floor) continue;
        bool ok = true;
        for (u32 d : decoTiles)
            if (sq(g.ix(d) - g.ix(tile)) + sq(g.iy(d) - g.iy(tile)) < 9) { ok = false; break; }
        if (!ok || nearProp(g, tile, 2)) continue;
        Prop p = makeProp(g, tile, PropKind::Deco);
        p.pos = {static_cast<f32>(g.ix(tile) * kTilePx + 2 + static_cast<i32>(jx)),
                 static_cast<f32>(g.iy(tile) * kTilePx + 2 + static_cast<i32>(jy))};
        p.variant = static_cast<u8>(variant);
        decoTiles.push_back(tile);
        f.props.push_back(p);
        ++placed;
    }

    const Room& sr = f.rooms[f.startRoom];
    f.playerSpawn = tileCenterPx(sr.cx, sr.cy);
    return true;
}

// ---------------------------------------------------------------------------------------- stage 6
void stageSpawns(Gen& g) {
    FloorData& f = g.f;
    constexpr i64 kDarkD2 = 120 * 120;  // no static light within 120 px
    for (usize ri = 0; ri < f.rooms.size(); ++ri) {
        const Room& r = f.rooms[ri];
        if (r.hops < 3) continue;
        SpawnZone z;
        z.kind = SpawnKind::DarkPocket;
        z.room = static_cast<u16>(ri);
        z.firstCell = static_cast<u32>(f.spawnCells.size());
        const i32 R1 = r.radius + 1;
        for (i32 y = std::max(0, r.cy - R1); y <= std::min(g.h - 1, r.cy + R1); ++y)
            for (i32 x = std::max(0, r.cx - R1); x <= std::min(g.w - 1, r.cx + R1); ++x) {
                const u32 i = g.idx(x, y);
                if (f.regionId[i] != ri || !isWalkable(f.tiles[i])) continue;
                const i64 px = x * kTilePx + kTilePx / 2, py = y * kTilePx + kTilePx / 2;
                bool dark = true;
                for (const Light& l : f.staticLights)
                    if (sq(static_cast<i64>(l.pos.x) - px) + sq(static_cast<i64>(l.pos.y) - py) <= kDarkD2) { dark = false; break; }
                if (dark) f.spawnCells.push_back(i);
            }
        z.cellCount = static_cast<u32>(f.spawnCells.size()) - z.firstCell;
        if (z.cellCount > 0) f.spawnZones.push_back(z);
    }
    for (const Prop& p : f.props) {
        if (p.kind != PropKind::Nest) continue;
        SpawnZone z;
        z.kind = SpawnKind::Nest;
        z.room = p.room;
        z.firstCell = static_cast<u32>(f.spawnCells.size());
        const i32 nx = g.ix(p.tile), ny = g.iy(p.tile);
        for (i32 y = ny - 3; y <= ny + 3; ++y)
            for (i32 x = nx - 3; x <= nx + 3; ++x)
                if (g.inside(x, y) && sq(x - nx) + sq(y - ny) <= 9 && isWalkable(f.tiles[g.idx(x, y)]))
                    f.spawnCells.push_back(g.idx(x, y));
        z.cellCount = static_cast<u32>(f.spawnCells.size()) - z.firstCell;
        f.spawnZones.push_back(z);
    }
}

} // namespace detail

// ---------------------------------------------------------------------------------------- driver
GenResult generateFloor(const FloorRequest& req, FloorData& out) {
    Stopwatch clock;
    const u32 depth = std::clamp(req.depth, 1u, 5u);
    const u32 size = floorSizeFor(req);
    const u64 base = splitmix64(req.runSeed + depth);
    GenResult result;
    for (u32 attempt = 0; attempt < 16; ++attempt) {
        out = FloorData{};
        out.runSeed = req.runSeed;
        out.depth = depth;
        out.floorSeed = base + 0x9E37ull * attempt;
        out.w = out.h = size;
        out.biome = biomeFor(depth);
        detail::Gen g(out, out.floorSeed, static_cast<i32>(size), static_cast<i32>(size));
        bool ok = detail::stageLayout(g) && detail::stageCaves(g);
        if (ok) {
            detail::stageDistanceField(g);
            ok = detail::stageLights(g) && detail::stageProps(g);
        }
        if (ok) {
            detail::stageSpawns(g);
            ok = detail::validateFloor(g);
        }
        if (ok) {
            out.metrics.attempts = attempt + 1;
            out.metrics.genMs = clock.elapsedMs();
            result.status = GenStatus::Ok;
            return result;
        }
        result.lastFailure = g.failure;
    }
    out.metrics.attempts = 16;
    out.metrics.genMs = clock.elapsedMs();
    return result;
}

} // namespace lb::procgen
