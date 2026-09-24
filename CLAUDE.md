# CLAUDE.md — Lightbound

You are building **Lightbound**, a top-down survival-stealth roguelite where the player fights a swarm of thousands of light-sensitive shadow creatures through procedurally generated cave floors.

Lightbound is **both a game and a research instrument**. It exists to measure how CPU–GPU cooperation patterns behave on **unified memory (Apple M5, Metal)** versus **discrete GPUs over PCIe (RTX 3060 Ti x16, RTX 4060 Ti x8, Vulkan)**. The research requirements in `docs/06_RESEARCH_PROTOCOL.md` are **not optional polish**. They constrain the architecture from day one.

## Document map (read in this order before writing code)

| File | What it defines |
|---|---|
| `docs/01_GDD.md` | Game design: loop, player kit, creatures, progression, tuning |
| `docs/02_PROCGEN.md` | Procedural floor generation pipeline, validation, seeding |
| `docs/03_ART_RENDERING.md` | Visual style, palette, render pipeline, lighting, post-processing |
| `docs/04_AUDIO.md` | Audio style, procedural SFX, swarm audio, adaptive music |
| `docs/05_ARCHITECTURE.md` | Engine layout, backends, per-frame pipeline, buffers, sync, determinism |
| `docs/06_RESEARCH_PROTOCOL.md` | Experimental conditions, metrics, instrumentation, CSV schemas |
| `docs/07_MILESTONES.md` | Ordered milestones with acceptance criteria. **Work from this.** |

## How to work

1. **One milestone at a time**, in order from `docs/07_MILESTONES.md`. Do not start milestone N+1 until milestone N's acceptance criteria pass.
2. At the end of each milestone, **stop and report**: what was built, how it was verified, measured numbers where relevant, and any deviation from spec.
3. **M1 is a go/no-go gate.** After M1, report the microbenchmark results and wait for the human before continuing.
4. If a spec is ambiguous, pick the simplest option consistent with the research invariants, record it in `docs/DECISIONS.md` (create it) with date and reason, and continue.
5. If a spec is **wrong or impossible** on a platform, stop and ask. Don't silently work around it.

## Research invariants (never violate without asking)

- **R1 — Backend parity.** Metal and Vulkan backends run the *same algorithms* with the *same buffer layouts* (`docs/05_ARCHITECTURE.md` §Buffers). Shaders come from one source (Slang). No backend-specific "optimizations" of the research pipeline.
- **R2 — Strategy switchability.** Transfer strategy (S0–S3) and round-trip count K are runtime flags. The game logic must not know which strategy is active.
- **R3 — Pure decision function.** Creature decision logic is a pure function `decide(AgentState, Perception, WorldCtx) -> Intent` with no hidden state or RNG outside explicit per-agent seeded streams. The staleness oracle depends on this.
- **R4 — Determinism.** Fixed timestep simulation (60 Hz), PCG32 RNG with explicit seeds, no `rand()`, no wall-clock-dependent logic, no unordered iteration affecting results.
- **R5 — Fixed render workload.** Internal resolution, light count caps, and post-processing are identical across conditions. Measurements run with vsync off.
- **R6 — Instrumentation is first-class.** GPU timestamps per pass, CPU timings per stage, and sync-wait timings per round trip are always compiled in (cheap), and written out in bench mode.
- **R7 — No hidden copies.** In strategy S2 on Metal, research buffers use `MTLStorageModeShared` and are never blitted. Any accidental staging copy invalidates the experiment.

## Tech stack

- **Language:** C++20. CMake ≥ 3.25. Warnings as errors on project code.
- **Platform layer:** SDL3 (window, input, gamepad, Metal view, Vulkan surface).
- **Metal backend:** metal-cpp (macOS 15+, Apple silicon). Uses `MTLSharedEvent` for GPU↔CPU signaling.
- **Vulkan backend:** Vulkan 1.3, timeline semaphores, VMA (Vulkan Memory Allocator). Windows 11 primary; builds on macOS through MoltenVK as an optional API-control condition.
- **Shaders:** Slang, compiled offline to SPIR-V and MSL by a CMake step. One `.slang` source per pass.
- **Audio:** miniaudio (bundled). All SFX are synthesized procedurally at startup (no audio asset files required).
- **Math:** own small header (`float2`, `float3`, `float4`) plus glm only if needed; SoA arrays for agents.
- **Tests:** doctest. Analysis scripts: Python 3.11 with pandas, matplotlib, scipy.
- Third-party code goes through CMake FetchContent, pinned to tags.

## Code rules

- Agents are **SoA** (structure of arrays). No per-agent heap objects, no virtual calls in hot loops.
- No exceptions or allocations in the frame loop. Use preallocated arenas and fixed-capacity containers.
- Every GPU-visible struct has a `static_assert` on size and alignment, plus a matching Slang struct. A unit test checks the two stay in sync (generated layout header or reflection).
- Keep the platform/backend code under `src/gfx/metal` and `src/gfx/vulkan`. Game code only talks to `src/gfx/Gfx.h`.
- Log with a tiny logger (`LB_LOG_INFO` etc.), not raw `printf`, except in bench CSV writers.

## Build & run (target commands — create these)

```
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DLB_BACKEND=metal    # macOS
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DLB_BACKEND=vulkan   # Windows (or macOS via MoltenVK)
cmake --build build
./build/lightbound --mode=play --seed=1234
./build/lightbound --mode=microbench --out=results/micro.csv
./build/lightbound --mode=bench --strategy=S2 --k=2 --agents=20000 --seed=101 --frames=3600 --out=results/run.csv
ctest --test-dir build
```

Full CLI flags: `docs/06_RESEARCH_PROTOCOL.md` §CLI.

## Repository layout (target)

```
lightbound/
  CLAUDE.md
  docs/                     spec documents + DECISIONS.md
  CMakeLists.txt
  src/
    app/                    main, CLI parsing, mode dispatch (play/bench/microbench/audit)
    core/                   math, PCG32, arena, job system, timing, logger
    procgen/                floor generation (docs/02)
    sim/                    agent SoA, player, behavior (decide()), events, combat
    frame/                  per-frame orchestrator implementing S0–S3 and K round trips
    gfx/                    Gfx.h interface; metal/; vulkan/
    render/                 lighting, sprites, particles, post, procedural sprite gen
    audio/                  miniaudio wrapper, sfx synth, swarm mixer, adaptive music
    bench/                  microbench, scripted input replay, CSV writers, audit oracle
  shaders/                  *.slang
  tests/
  tools/analysis/           Python analysis & plotting, cost-model fit
  bench/seeds.txt           fixed benchmark seeds
  bench/replays/            recorded input scripts
  results/                  (gitignored)
```
