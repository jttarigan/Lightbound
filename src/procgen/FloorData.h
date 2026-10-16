#pragma once
// Output of the floor generator (docs/02 §2). CPU-side data; the SDF is uploaded once per floor
// to a read-only GPU buffer later (M4/M5). Positions are in px (1 tile = 16 px, GDD §4).
#include "core/Math.h"
#include "core/Types.h"

#include <vector>

namespace lb::procgen {

inline constexpr i32 kTilePx = 16;
inline constexpr i32 kSdfPerTile = 4;               // SDF samples per tile and axis
inline constexpr i32 kSdfSamplePx = kTilePx / kSdfPerTile;
inline constexpr u32 kMaxStaticLights = 64;         // R5: fixed render workload
inline constexpr u16 kRegionNone = 0xFFFF;          // corridors and walls
inline constexpr u8 kNoBeam = 0xFF;

enum class Tile : u8 { Wall = 0, Floor = 1, Water = 2, Chasm = 3, CrystalWall = 4 };

constexpr bool blocksLight(Tile t) { return t == Tile::Wall || t == Tile::CrystalWall; }
constexpr bool isOpen(Tile t) { return !blocksLight(t); }
constexpr bool isWalkable(Tile t) { return t == Tile::Floor || t == Tile::Water; }

enum class Biome : u8 { MossyHollow, CrystalVein, CrystalVeinDeep, EmberDeep, Abyss };

enum class RoomRole : u8 { Cave, Start, Exit, Seal, ShrineAntechamber, Nest, Cache };

struct Room {
    i32 cx = 0, cy = 0;      // center tile
    i32 radius = 0;          // carve radius in tiles (6..12)
    RoomRole role = RoomRole::Cave;
    bool puzzle = false;     // seal floors: the antechamber also holds the mirror puzzle
    u8 degree = 0;           // in the room graph
    u8 hops = 0;             // graph distance from START
};

struct RoomEdge {
    u16 a = 0, b = 0;
    bool loop = false;       // added back after the MST (15 %)
};

enum class LightKind : u8 { Brazier, Crystal };

struct Light {
    float2 pos;              // px
    f32 radius = 0.f;        // px
    LightKind kind = LightKind::Brazier;
    bool lit = false;        // braziers: pre-lit; crystals: always lit
    u8 priority = 0;         // 0 essential, 1 room brazier, 2 crystal, 3 corridor brazier (cap drops 3 first)
    u8 beamDir = kNoBeam;    // seal floors: the puzzle crystal's fixed beam direction (0..7, 45° steps)
    u16 room = kRegionNone;
};

enum class PropKind : u8 { ExitWell, OilPool, Cache, Nest, Mirror, Sigil, BeamSource, Deco };

enum class CacheContent : u8 { Oil, Flare, Heart };

struct Prop {
    float2 pos;              // px (tile center, deco jittered)
    PropKind kind = PropKind::Deco;
    u8 variant = 0;          // cache content, deco kind
    u8 orientation = 0;      // mirrors: current line orientation 0..3 (× 45°)
    u8 solution = 0;         // mirrors: orientation that solves the puzzle (debug/tests only)
    u16 room = kRegionNone;
    u32 tile = 0;            // tile index (y * w + x)
};

enum class SpawnKind : u8 { DarkPocket, Nest };

struct SpawnZone {
    SpawnKind kind = SpawnKind::DarkPocket;
    u16 room = kRegionNone;
    u32 firstCell = 0;       // range in FloorData::spawnCells
    u32 cellCount = 0;
};

struct Metrics {
    u32 floorTiles = 0;          // open tiles (floor, water, chasm)
    u32 rooms = 0;
    u32 loops = 0;
    u32 criticalPathLen = 0;     // tiles on the shortest START → EXIT path
    f32 litAreaFraction = 0.f;   // walkable tiles within radius of a lit static light / walkable tiles
    u32 darkPocketArea = 0;      // tiles
    u32 nestCount = 0;
    u32 mirrorCount = 0;
    f64 genMs = 0.0;             // wall clock, all attempts (never hashed)
    u32 attempts = 0;
};

struct FloorRequest {
    u64 runSeed = 1;
    u32 depth = 1;               // 1..5
    u32 sizeOverride = 0;        // 0: GDD §7 size for the depth; bench uses 200 (docs/06 §3)
};

struct FloorData {
    u64 runSeed = 0;
    u32 depth = 0;
    u64 floorSeed = 0;           // seed of the accepted attempt
    u32 w = 0, h = 0;
    Biome biome = Biome::MossyHollow;
    bool seal = false;
    std::vector<Tile> tiles;     // w * h
    std::vector<u16> regionId;   // w * h, room index or kRegionNone
    std::vector<f32> sdf;        // (w * 4) * (h * 4), px; > 0 in open space, < 0 inside walls
    std::vector<Room> rooms;
    std::vector<RoomEdge> edges;
    std::vector<Light> staticLights;
    std::vector<Prop> props;
    float2 playerSpawn;
    u32 startRoom = 0, exitRoom = 0;
    std::vector<SpawnZone> spawnZones;
    std::vector<u32> spawnCells; // tile indices
    std::vector<u32> criticalPath;
    Metrics metrics;

    u32 sdfWidth() const { return w * static_cast<u32>(kSdfPerTile); }
    u32 sdfHeight() const { return h * static_cast<u32>(kSdfPerTile); }
    Tile at(i32 x, i32 y) const {
        if (x < 0 || y < 0 || x >= static_cast<i32>(w) || y >= static_cast<i32>(h)) return Tile::Wall;
        return tiles[static_cast<usize>(y) * w + static_cast<usize>(x)];
    }
};

enum class GenStatus : u8 { Ok, Failed };

struct GenResult {
    GenStatus status = GenStatus::Failed;
    const char* lastFailure = "";  // validation rule of the last failed attempt
};

/// Map size in tiles for depth d (GDD §7), or the override.
u32 floorSizeFor(const FloorRequest& req);
Biome biomeFor(u32 depth);
const char* biomeName(Biome b);
const char* roomRoleName(RoomRole r);

/// Generates and validates a floor (docs/02 §3–§6). Re-rolls up to 16 attempts
/// (floor_seed + 0x9E37 * attempt). Deterministic: same request + kGeneratorVersion = same floor.
GenResult generateFloor(const FloorRequest& req, FloorData& out);

/// 64-bit hash of everything the generator decides (docs/02 §7): tiles, regions, SDF quantized
/// to 1/16 px, rooms, lights, props, spawns, critical path. Excludes gen_ms.
u64 floorHash(const FloorData& f);

/// Seal puzzle: fewest single +45° mirror rotations that make the crystal beam hit the sigil, or -1
/// if none within maxDepth. Uses the current mirror orientations in `f.props`.
i32 solveSealPuzzle(const FloorData& f, i32 maxDepth);

} // namespace lb::procgen
