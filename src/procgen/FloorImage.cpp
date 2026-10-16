#include "procgen/FloorImage.h"

namespace lb::procgen {
namespace {

struct Rgb {
    u8 r, g, b;
};

Rgb floorColor(Biome b) {
    switch (b) {
    case Biome::MossyHollow: return {58, 78, 60};
    case Biome::CrystalVein: return {64, 58, 92};
    case Biome::CrystalVeinDeep: return {50, 44, 74};
    case Biome::EmberDeep: return {84, 56, 44};
    case Biome::Abyss: return {38, 42, 60};
    }
    return {60, 60, 60};
}

Rgb mix(Rgb a, Rgb b, u32 bWeight256) {
    auto m = [bWeight256](u8 x, u8 y) {
        return static_cast<u8>((static_cast<u32>(x) * (256u - bWeight256) + static_cast<u32>(y) * bWeight256) / 256u);
    };
    return {m(a.r, b.r), m(a.g, b.g), m(a.b, b.b)};
}

struct Canvas {
    FloorImage& img;
    void put(i32 x, i32 y, Rgb c) {
        if (x < 0 || y < 0 || x >= static_cast<i32>(img.width) || y >= static_cast<i32>(img.height)) return;
        u8* p = &img.rgba[(static_cast<usize>(y) * img.width + static_cast<usize>(x)) * 4u];
        p[0] = c.r;
        p[1] = c.g;
        p[2] = c.b;
        p[3] = 255;
    }
    Rgb get(i32 x, i32 y) const {
        const u8* p = &img.rgba[(static_cast<usize>(y) * img.width + static_cast<usize>(x)) * 4u];
        return {p[0], p[1], p[2]};
    }
    void box(i32 cx, i32 cy, i32 r, Rgb c) {
        for (i32 y = cy - r; y <= cy + r; ++y)
            for (i32 x = cx - r; x <= cx + r; ++x) put(x, y, c);
    }
    void ring(i32 cx, i32 cy, i32 r, Rgb c) {
        for (i32 y = cy - r; y <= cy + r; ++y)
            for (i32 x = cx - r; x <= cx + r; ++x) {
                const i32 d2 = (x - cx) * (x - cx) + (y - cy) * (y - cy);
                if (d2 <= r * r && d2 > (r - 1) * (r - 1)) put(x, y, c);
            }
    }
};

} // namespace

FloorImage renderFloorImage(const FloorData& f, u32 pxPerTile) {
    FloorImage img;
    const i32 s = static_cast<i32>(pxPerTile);
    img.width = f.w * pxPerTile;
    img.height = f.h * pxPerTile;
    img.rgba.assign(static_cast<usize>(img.width) * img.height * 4u, 255);
    Canvas cv{img};
    const Rgb floor = floorColor(f.biome);
    const Rgb wall{22, 22, 30}, water{46, 96, 124}, chasm{6, 6, 10}, crystal{120, 210, 232};

    for (u32 y = 0; y < f.h; ++y)
        for (u32 x = 0; x < f.w; ++x) {
            const usize i = static_cast<usize>(y) * f.w + x;
            Rgb c = wall;
            switch (f.tiles[i]) {
            case Tile::Wall: c = wall; break;
            case Tile::Floor: c = f.regionId[i] == kRegionNone ? mix(floor, wall, 80) : floor; break;
            case Tile::Water: c = water; break;
            case Tile::Chasm: c = chasm; break;
            case Tile::CrystalWall: c = crystal; break;
            }
            for (i32 py = 0; py < s; ++py)
                for (i32 px = 0; px < s; ++px) cv.put(static_cast<i32>(x) * s + px, static_cast<i32>(y) * s + py, c);
        }

    // Spawn zones: dark pockets violet, nest zones red (tinted).
    for (const SpawnZone& z : f.spawnZones) {
        const Rgb tint = z.kind == SpawnKind::DarkPocket ? Rgb{120, 60, 160} : Rgb{200, 40, 40};
        for (u32 k = 0; k < z.cellCount; ++k) {
            const u32 c = f.spawnCells[z.firstCell + k];
            const i32 x = static_cast<i32>(c % f.w) * s, y = static_cast<i32>(c / f.w) * s;
            for (i32 py = 0; py < s; ++py)
                for (i32 px = 0; px < s; ++px) cv.put(x + px, y + py, mix(cv.get(x + px, y + py), tint, 90));
        }
    }

    // Light radii (rings) and sources.
    const f32 scale = static_cast<f32>(s) / static_cast<f32>(kTilePx);
    for (const Light& l : f.staticLights) {
        const i32 cx = static_cast<i32>(l.pos.x * scale), cy = static_cast<i32>(l.pos.y * scale);
        const i32 r = static_cast<i32>(l.radius * scale);
        const Rgb c = l.kind == LightKind::Crystal ? crystal : (l.lit ? Rgb{255, 170, 60} : Rgb{130, 110, 90});
        cv.ring(cx, cy, r, c);
        cv.box(cx, cy, 1, c);
    }

    // Critical path.
    for (u32 c : f.criticalPath) cv.put(static_cast<i32>(c % f.w) * s + s / 2, static_cast<i32>(c / f.w) * s + s / 2, {250, 230, 90});

    // Props.
    for (const Prop& p : f.props) {
        const i32 cx = static_cast<i32>(p.pos.x * scale), cy = static_cast<i32>(p.pos.y * scale);
        switch (p.kind) {
        case PropKind::ExitWell: cv.box(cx, cy, 3, {255, 255, 255}); break;
        case PropKind::Cache: cv.box(cx, cy, 2, {230, 190, 40}); break;
        case PropKind::OilPool: cv.box(cx, cy, 2, {20, 20, 20}); break;
        case PropKind::Nest: cv.box(cx, cy, 3, {220, 30, 30}); break;
        case PropKind::Sigil: cv.box(cx, cy, 2, {255, 240, 120}); break;
        case PropKind::BeamSource: cv.box(cx, cy, 2, {140, 255, 255}); break;
        case PropKind::Mirror: {
            static constexpr i32 kLx[4] = {1, 1, 0, -1}, kLy[4] = {0, 1, 1, 1};
            for (i32 t = -3; t <= 3; ++t) cv.put(cx + t * kLx[p.orientation & 3u], cy + t * kLy[p.orientation & 3u], {255, 255, 255});
            break;
        }
        case PropKind::Deco: cv.put(cx, cy, mix(floor, {200, 200, 200}, 128)); break;
        }
    }
    // Player spawn.
    cv.ring(static_cast<i32>(f.playerSpawn.x * scale), static_cast<i32>(f.playerSpawn.y * scale), 4, {90, 255, 120});
    return img;
}

} // namespace lb::procgen
