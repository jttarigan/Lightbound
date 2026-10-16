// floorHash (docs/02 §7): FNV-1a 64 over every generator decision. Floats are quantized to
// 1/16 px first (all generator floats are exact multiples of that or sqrt of an integer).
#include "procgen/FloorData.h"

#include <cmath>

namespace lb::procgen {
namespace {

struct Fnv {
    u64 h = 0xcbf29ce484222325ULL;
    void byte(u8 b) {
        h ^= b;
        h *= 0x100000001b3ULL;
    }
    void u(u64 v, int bytes) {
        for (int i = 0; i < bytes; ++i) byte(static_cast<u8>(v >> (8 * i)));
    }
    void q(f32 v) { u(static_cast<u64>(static_cast<i64>(std::floor(v * 16.f + 0.5f))), 8); }
};

} // namespace

u64 floorHash(const FloorData& f) {
    Fnv h;
    h.u(f.w, 4);
    h.u(f.h, 4);
    h.u(f.depth, 4);
    h.u(f.floorSeed, 8);
    h.u(static_cast<u64>(f.biome), 1);
    for (Tile t : f.tiles) h.u(static_cast<u64>(t), 1);
    for (u16 r : f.regionId) h.u(r, 2);
    for (f32 s : f.sdf) h.q(s);
    for (const Room& r : f.rooms) {
        h.u(static_cast<u64>(r.cx), 4);
        h.u(static_cast<u64>(r.cy), 4);
        h.u(static_cast<u64>(r.radius), 4);
        h.u(static_cast<u64>(r.role), 1);
        h.u(r.puzzle ? 1u : 0u, 1);
    }
    for (const RoomEdge& e : f.edges) {
        h.u(e.a, 2);
        h.u(e.b, 2);
        h.u(e.loop ? 1u : 0u, 1);
    }
    for (const Light& l : f.staticLights) {
        h.q(l.pos.x);
        h.q(l.pos.y);
        h.q(l.radius);
        h.u(static_cast<u64>(l.kind), 1);
        h.u(l.lit ? 1u : 0u, 1);
        h.u(l.beamDir, 1);
    }
    for (const Prop& p : f.props) {
        h.q(p.pos.x);
        h.q(p.pos.y);
        h.u(static_cast<u64>(p.kind), 1);
        h.u(p.variant, 1);
        h.u(p.orientation, 1);
        h.u(p.tile, 4);
    }
    h.q(f.playerSpawn.x);
    h.q(f.playerSpawn.y);
    for (const SpawnZone& z : f.spawnZones) {
        h.u(static_cast<u64>(z.kind), 1);
        h.u(z.room, 2);
        h.u(z.firstCell, 4);
        h.u(z.cellCount, 4);
    }
    for (u32 c : f.spawnCells) h.u(c, 4);
    for (u32 c : f.criticalPath) h.u(c, 4);
    return h.h;
}

} // namespace lb::procgen
