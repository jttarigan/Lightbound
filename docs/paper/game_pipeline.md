# §7 Game workload and pipeline (design as specified; draft 2026-10-17)

<!-- Sources: docs/01_GDD.md (game), docs/02_PROCGEN.md + M2 (floors), docs/05 §1–§7 (pipeline, strategies,
     buffers, CPU passes, determinism), docs/06 §3–§4 (workload control, condition matrix).
     STATUS of the code: M2 (procedural floors) is built; the simulation (M3), the frame orchestrator (M4) and the
     render path are NOT built yet. This section describes the design the measurements will use, and must be
     re-checked against the code at M8. -->

## 7.1 The game as a workload

Lightbound is a top-down survival-stealth roguelite. The player carries a lantern through procedurally generated
cave floors swarming with thousands of light-sensitive shadow creatures. Light hurts the creatures, so every
creature's behaviour depends on the light field around it, and the light field depends on the player, static
lights and creatures being destroyed. That loop is what makes the workload useful for our question:
- the **GPU** computes what each creature perceives: light, its gradient, line of sight, and neighbours;
- the **CPU** decides what each creature does, with a pure decision function;
- the **GPU** moves and draws them.

Every frame therefore needs at least one GPU→CPU→GPU round trip, and better decisions need more.

**Floors** are generated from a seed and validated, so a seed reproduces a floor bit for bit across platforms
(M2; the same golden hashes on macOS and Windows). Benchmarks use five fixed seeds at 200 × 200 tiles.

## 7.2 One frame

Per simulation tick (60 Hz fixed step; in bench mode one tick per rendered frame, vsync off):

| pass | where | work |
|---|---|---|
| A | GPU | light field, spatial hash (radix sort by 16 px cell), perception for all N agents → `Perception[N]` |
| B | CPU | `decide()` for all agents on W worker threads, light damage, alarm propagation (BFS over the neighbour graph), events, list of ambiguous agents |
| C, C′, C″ | GPU | K ≥ 2: refined queries for ambiguous agents (light at the exact position, then line-of-sight rays for alarms) |
| D, D′, D″ | CPU | re-decide the refined agents, emit the next requests |
| E | GPU | read `Intent[N]`, steer, separate, integrate, then render (albedo, lighting, particles, post, UI) |

**K** is the number of round trips per frame, from 1 to 4, and each extra round trip buys a visible gameplay
quality:
- K = 2: precise decisions at light edges;
- K = 3: alarms confirmed by line of sight;
- K = 4: a second alarm wave.

This gives the game a natural reason to pay for more round trips, rather than an artificial loop.

## 7.3 Strategies

The game code receives `const Perception*` and writes `Intent*`, and never knows how they were supplied
(invariant R2). The orchestrator implements four strategies:

| | name | data path | round trips per frame |
|---|---|---|---|
| S0 | delayed | the CPU reads perception from L = 2 frames ago without waiting; its intents apply next frame | 0 (K forced to 1, delayed) |
| S1 | same-frame copy | research buffers in GPU-private memory, copied to and from host-visible staging at each hand-off | K, plus copies |
| S2 | same-frame direct | research buffers accessed directly by both processors (Metal shared storage; on Vulkan, host-cached system memory for GPU→CPU and ReBAR device memory for CPU→GPU) | K, no copies |
| S3 | all-GPU | `decide()` ported to the GPU; alarm by grid diffusion; no refinement | 0 |

S0 is what engines do today (asynchronous readback). S1 and S2 are the same-frame alternatives whose cost the
microbenchmark measured. S3 is the "don't round-trip at all" baseline, at the price of simpler behaviour.
Submission is `chain` (the frame's GPU work submitted up front, each segment waiting on a CPU-signalled value)
for the main matrix, and `perpass` in a subset.

## 7.4 Control and determinism

- **Fixed render workload** across conditions: internal resolution, light caps (≤ 64 static lights per floor),
  post-processing, vsync off (R5).
- **Determinism:** fixed timestep, PCG32 streams per agent with explicit seeds, no unordered iteration. Worker
  results are merged in chunk order. `decide()` is a pure function of agent state, perception and world context
  (R3, R4). This is what makes the staleness oracle possible (§10): a same-frame run and a delayed run can be
  compared decision by decision.
- **Driven player, constant N:** a deterministic bench bot walks the floor's critical path at 70 px/s, sweeps
  the lantern, and fires flares, strobes and focus beams on a fixed frame schedule, so the stimuli are the same on
  every platform. All N creatures spawn at t = 0 and do not die in bench mode. Damage is still computed, so the
  CPU cost stays, but N stays constant. Each run has 600 warm-up frames and 3,600 measured frames, with 4 worker
  threads on every platform.
- **Matrix:** strategies × K (1–4, S1/S2 only) × N (1k–100k) × 5 seeds × 3 repetitions with a process restart,
  in randomized order with cooldowns. That is 900 runs per platform.

## 7.5 What the microbenchmark does not capture

The game's round trips differ from the microbenchmark's in three ways that §9 must model or bound:
1. The GPU is busy between hand-offs (pass A and independent work), which is the reason for the hot-GPU
   extension.
2. The CPU's work between hand-offs is real and parallel (W workers reading `Perception` and writing `Intent`),
   not a single-threaded sum.
3. The GPU→CPU payload has a fixed part independent of N (the spatial hash's `CellRange`, ≈ 312 KiB per
   200 × 200 floor).
