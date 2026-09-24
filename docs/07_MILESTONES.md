# 07 — Milestones

Work strictly in order. Each milestone ends with a **report** (what was built, how verified, numbers, deviations) and all acceptance criteria green. Items marked **[Mac]** are verified on the M5 (Metal), **[PC]** on the Windows PC (Vulkan). When you are running on one machine, finish and verify that platform, then list the exact commands the human must run on the other machine and **stop** until they paste back results.

---

## M0 — Scaffold

**Build:** repo layout (CLAUDE.md), CMake with `LB_BACKEND=metal|vulkan`, FetchContent deps (SDL3, metal-cpp, Vulkan headers, VMA, Slang binary release, miniaudio, doctest), Slang → SPIR-V/MSL build step, `core/` (math, PCG32 + splitmix64, arena, logger, steady-clock timer, job system with fixed W), CLI parser with all flags from protocol §11 (unimplemented modes print "not implemented" and exit 2), `Gfx.h` interface skeleton, a window that clears to `#0b0b10`, `docs/DECISIONS.md`.

**Accept:**
- Builds warning-free in Release on [Mac] (Metal) and [PC] (Vulkan); `ctest` passes (PCG32 known-answer test, job system test, arena test).
- `lightbound --mode=play` opens a window and clears; closes cleanly; no validation-layer errors (Vulkan) and no Metal API validation errors.
- `layouts.gen.h` generation works and a test compares one sample struct to Slang reflection.

---

## M1 — Round-trip microbenchmark (GO / NO-GO GATE)

**Build:** `--mode=microbench` exactly as protocol §6: timeline sync abstraction (`gpuSignal/gpuWait/cpuWait/cpuSignal`) on both backends, memory classes of architecture §4.2 including Vulkan variants (`hostcached`, `rebar`, `coherent`), S1 copy path, both submit modes, both CPU-wait modes, GPU timestamps + clock calibration, `micro.csv` writer with header. Plus `tools/analysis/micro_report.py` → figure 1 + summary table (p50/p99 per path × payload).

**Accept:**
- Runs to completion on [Mac] and [PC with 3060 Ti] and [PC with 4060 Ti]; sysinfo header includes measured PCIe gen/width and ReBAR status.
- Sanity: bytes read back on CPU equal what the GPU wrote (checksum) for every iteration; CPU→GPU round trip verified by GPU checksum.
- Run-to-run variation of p50 across 3 process restarts < 10%.
- **Report the table and figure, apply the go criterion (protocol §6), and STOP for human decision.**

---

## M2 — Procedural floors

**Build:** `procgen/` per `02_PROCGEN.md` (all stages, biomes, validation with re-roll, seal-puzzle backward construction + solver), `FloorData`, SDF, `--mode=procgen-dump` PNG writer (stb_image_write).

**Accept:**
- Tests of procgen §7 pass: determinism (seeds 1..200), validity (seeds 1..1000), golden hashes identical on [Mac] and [PC], 200×200 generation < 150 ms.
- Dump PNGs for seeds in `bench/seeds.txt` committed under `docs/img/floors/` for review.

---

## M3 — Headless simulation core

**Build:** `sim/`: agent SoA, `decide()` utility scoring with hysteresis (GDD §5.2), light damage, state timers, alarm BFS over a neighbor graph built from CellRange/SortedIds, ambiguity test → RefineRequests, refined re-decision, sim events, audio binning counts. A CPU reference implementation of perception (slow, scalar: light falloff + SDF soft occlusion + grid-based neighbor count) used **only** for tests and as the audit oracle's reference.

**Accept:**
- Pure-function test: `decide()` called twice with identical inputs returns identical Intents; fuzz 1e6 random inputs, no NaN/inf.
- Scenario tests with synthetic perception: agents in bright light pick FLEE; Stalker behind lantern picks FLANK; Wailer lit triggers alarm reaching exactly the expected agents within 4 hops with correct decay.
- Alarm BFS on 100k agents < 2 ms on one core [Mac] (report PC number too).
- `sim` links without any graphics backend.

---

## M4 — GPU research pipeline (headless bench mode)

**Build:** passes A (light field incl. world light grid, spatial hash radix sort, perception), C/C'/C'' (refine + LOS), E (steering/integration), `decide.slang` for S3; frame orchestrator implementing S0–S3, K = 1..4, chain/perpass, spin/block; bench bot + stimulus schedule; full instrumentation; `frames.csv`, `roundtrips.csv`, `runs.csv` writers. Render only a debug view (agents as dots over the light field) — full art comes in M5.

**Accept:**
- `--mode=bench` runs every strategy × K at N = 1k and 100k on [Mac] and [PC] without validation errors.
- **R7 check:** on Metal S2, a debug assertion counts blit encoders on research buffers per frame = 0; on S1 it equals the expected copy count. Same counter on Vulkan for `vkCmdCopyBuffer` on research buffers.
- GPU perception vs CPU reference perception (M3) on 1k agents: mean |Δlit| < 0.01, LOS agreement > 99%.
- Determinism test (architecture §7) passes for S2 K=2 on each backend.
- Equivalence check (protocol §10) passes between [Mac] and [PC].
- Report a first `--preset=quick` table.

---

## M5 — Rendering & art

**Build:** procedural sprite/tile atlas generator (art §4), albedo pass with instanced agents read from GPU buffers and GPU y-sort, composite, particles, post stack (bloom, vignette, blue-noise dither, chromatic kick, LUTs), camera, debug views F3/F4, pixel UI font.

**Accept:**
- Screenshots per biome committed in `docs/img/biomes/` [Mac] and [PC]; visuals match between backends (side-by-side).
- Creature eyes visible in darkness; no visible banding in dark gradients.
- Render workload identical across strategies (same pass list, caps) — asserted in bench header.
- Frame time budget table (art §8) reported at 20k agents.

---

## M6 — Gameplay

**Build:** player controller, all tools (GDD §4), creatures' contact damage, brazier/mirror/seal/cache/nest/oil pool, beam tracing with mirror bounces + sigil charging (CPU reads beam endpoints same-frame), floor flow, shrine upgrades, run summary, pause menu, settings, gamepad support, tuning hot-reload.

**Accept:**
- A full 5-floor run is completable on seed 1234 by the human; seal puzzles solvable.
- Bench mode unaffected: `--preset=quick` numbers within 5% of M4/M5 numbers (gameplay code must not leak cost into the research pipeline).
- Playtest notes: human confirms the swarm visibly reacts on the flare's ignition frame under S2 and visibly lags under S0.

---

## M7 — Audio

**Build:** mixer, buses, reverb, darkness filter, procedural SFX set (audio §3), granular swarm voices (audio §4), adaptive music (audio §5), spatialization with occlusion.

**Accept:**
- All sounds generated at startup in < 300 ms; generation hash stable.
- Audio thread never blocks the main thread (measured: max main-thread stall attributable to audio = 0).
- `--audio=null` vs `off` CPU difference reported; bench uses `null`.

---

## M8 — Experiment harness & analysis

**Build:** `--mode=audit` + oracle, `stimuli.csv`, power capture integration, `tools/analysis/run_matrix.py` (resumable, randomized, cooldowns), `make_figures.py` (all figures of protocol §8), cost-model fit + validation, statistics, `docs/REPRODUCE.md`.

**Accept:**
- `--preset=quick` runs end-to-end and produces all figures on each platform.
- Cost model fit on quick data runs and reports MAPE.
- Human launches the core matrix on each platform; the harness resumes after an interrupted run.

---

## M9 — Results package

**Build:** run full matrices (human-operated), final figures/tables, results summary `docs/RESULTS.md` (numbers only, no paper prose), archive bundle (commit tag, binaries, CSVs, scripts).

**Accept:**
- Every figure regenerates from archived CSVs with one command.
- `RESULTS.md` states for each hypothesis H1–H6: supported / not supported / inconclusive, with the numbers.
