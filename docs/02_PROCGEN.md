# 02 — Procedural Generation: The Hollows

Every floor is generated from a single 64-bit seed. Same seed + same generator version = bit-identical floor on every platform. Benchmark seeds are fixed in `bench/seeds.txt`, so procgen is also the **workload generator** for the experiments: it must be deterministic and the difficulty must be measurable.

## 1. Seeding

```
run_seed     : from --seed or random (shown on pause/summary screens)
floor_seed_d = splitmix64(run_seed + d)            // d = 1..5
stream(name) = pcg32(floor_seed_d, hash32(name))    // one named stream per stage
```

Each pipeline stage draws from its own named stream (`"layout"`, `"caves"`, `"lights"`, `"props"`, `"spawns"`, `"deco"`) so changing one stage doesn't reshuffle the others. No floating-point RNG in layout decisions; use integer ranges. `GENERATOR_VERSION` constant is written into bench CSV headers.

## 2. Output: `FloorData`

```
struct FloorData {
  uint32 w, h;                 // tiles
  uint8  tiles[w*h];           // WALL, FLOOR, WATER, CHASM, CRYSTAL_WALL
  uint8  biome;
  uint16 region_id[w*h];       // room/cave id, 0xFFFF for corridors/walls
  float  sdf[(w*4)*(h*4)];     // distance to nearest wall, 4 samples per tile (px units)
  Light  static_lights[];      // braziers (lit/unlit), crystals
  Prop   props[];              // mirrors, caches, nests, oil pools, sigil, exit, deco
  float2 player_spawn;
  SpawnZone spawn_zones[];     // nests + dark pockets for initial swarm
  Metrics metrics;             // see §6
}
```

The SDF is uploaded once per floor to a GPU buffer (read-only) and used for wall avoidance, LOS/DDA acceleration and soft shadows.

## 3. Pipeline

### Stage 1 — Macro layout (stream `layout`)
- Poisson-disc sample **room centers** in the map (min distance 22 tiles, count ≈ `(w*h)/900`).
- Build a **Delaunay triangulation** of centers → **minimum spanning tree** → add back 15% of remaining edges (loops make flanking and escape routes possible).
- Assign room roles: `START` (the lowest-degree room among those nearest the map edge; ties broken by seeded RNG), `EXIT` (maximize graph distance from START), `SHRINE_ANTECHAMBER` (adjacent to EXIT), `NEST` rooms (2 + d, prefer high-degree rooms far from START), `CACHE` rooms (3–5), remaining `CAVE`.
- On seal floors, `EXIT` gets role `SEAL` and one neighbor is `PUZZLE`.

### Stage 2 — Cave carving (stream `caves`)
- Each room: carve a blob of radius 6–12 tiles with **cellular automata** (initial fill 45%, 5 iterations, rule B5678/S45678 on walls), masked by a radial falloff so rooms stay roughly round.
- Each graph edge: carve a **corridor** by a random walk biased toward the target (bias 0.7), width 2–4 tiles, then 1 CA smoothing pass along the corridor.
- **Water** (biome-dependent): shallow pools in 20% of caves (walkable, slows player 30%, reflects light visually only).
- **Chasms** (Abyss biome): non-walkable, not light-blocking.
- Remove floor regions not connected to START (flood fill); fill tiny enclosed islands (< 6 tiles) with walls.

### Stage 3 — Distance field
- Compute tile-level exact EDT (Felzenszwalb) to walls, then bilinearly upsample to 4×/tile and refine with a jump-flood pass on CPU. Store in px units.

### Stage 4 — Lights (stream `lights`)
- **Braziers:** placed on floor cells with `sdf ∈ [1.5, 3]` tiles (near walls), one per room plus one per 20 corridor tiles. Pre-lit fraction per depth (GDD §7). START room brazier is always lit.
- **Crystals:** Crystal biome places clusters on walls; other biomes place 0–3.
- **Light budget cap:** max 64 static lights per floor (R5: fixed render workload). If exceeded, drop lowest-priority (corridor braziers first).

### Stage 5 — Props (stream `props`)
- Exit well in EXIT room center. Oil pools: 1–2 per floor, in CACHE rooms. Caches in CACHE rooms. Nests in NEST rooms, center-ish, sdf ≥ 3 tiles.
- **Seal puzzle (d = 3, 5):** place the sun-sigil on the EXIT room's far wall. Construct the puzzle *backwards* so it is always solvable:
  1. Choose a crystal beam source in the PUZZLE room.
  2. Trace a path of 2 (d3) or 3 (d5) reflection points from source to sigil through floor cells with clear LOS, using 45°-multiple directions only.
  3. Place mirrors at reflection points with the **correct** orientation, then randomize each to a wrong orientation (seeded).
  4. Verify by simulation (§5) that a solution exists within ≤ 8 rotations total.
- Decoration (stream `deco`): stalagmites, bones, mushrooms, cracks — visual only, non-colliding, placed with blue noise.

### Stage 6 — Spawns (stream `spawns`)
- Initial swarm = 40% of budget, spawned in **dark pockets**: floor cells with no static light within 120 px and graph distance ≥ 3 rooms from START.
- Rest arrive from nests over time.
- Type mix per GDD §5, assigned deterministically by `agent_id` hash.
- **Bench mode:** all N agents spawn at t=0 across dark pockets and nests (uniform by cell), nests disabled, so N is constant during a run.

## 4. Biomes

| d | Biome | Palette ramp (see art doc) | Features |
|---|---|---|---|
| 1 | Mossy Hollow | green-teal moss, warm stone | many water pools, many pre-lit braziers |
| 2 | Crystal Vein | violet-cyan | crystal clusters (cold ambient pockets) |
| 3 | Crystal Vein (deep) | violet-cyan, darker | first seal puzzle |
| 4 | Ember Deep | rust, charcoal, orange cracks | glowing floor cracks (very weak lights, decorative) |
| 5 | Abyss | near-black blue, bone white | chasms, few braziers, second seal puzzle |

Biome affects tile palette, deco set, ambient color, audio ambience bed — **not** light count caps or internal resolution (R5).

## 5. Validation (must pass; else re-roll with `floor_seed + 0x9E37 * attempt`, max 16 attempts)

1. START → EXIT walkable path exists (A* on tiles, player collision radius 5 px).
2. Every CACHE, NEST and OIL POOL reachable.
3. Seal floors: puzzle solvable (beam simulation with mirror rotations, BFS over orientation states, depth ≤ 8).
4. START room contains no spawn zone; nearest nest ≥ 40 tiles path distance.
5. At least one lit brazier within 30 tiles of each 60-tile stretch of the critical path (d ≤ 3 only).
6. Static lights ≤ 64; floor tile count within ±15% of target for (w, h).

Record the attempt count in metrics. Validation failures after 16 attempts = hard error with seed printed (should never happen; add a test).

## 6. Floor metrics (logged; used in bench CSV header and for difficulty sanity)

`floor_tiles, rooms, loops, critical_path_len, lit_area_fraction (static lights only), dark_pocket_area, nest_count, mirror_count, gen_ms, attempts`.

## 7. Tests (doctest)

- **Determinism:** generate seeds 1..200 twice → identical hash of `FloorData` (tiles, props, lights, sdf quantized to 1/16 px).
- **Cross-platform golden:** hashes for seeds in `bench/seeds.txt` stored in `tests/golden/floors.txt`; must match on macOS and Windows.
- **Validity:** seeds 1..1000 all pass §5.
- **Performance:** `gen_ms` < 150 ms for a 200×200 floor in Release.
- **Tool:** `--mode=procgen-dump --seed=N --out=floor.png` writes a debug PNG (tiles, lights, props, critical path, spawn zones) for eyeballing.
