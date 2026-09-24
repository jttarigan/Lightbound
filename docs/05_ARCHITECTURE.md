# 05 — Architecture

This document defines the engine structure, the per-frame CPU–GPU pipeline, buffer layouts and synchronization. It is the contract between the game and the research: **every strategy and backend must implement exactly what is written here** (R1, R2).

## 1. Threads

| Thread | Role |
|---|---|
| Main | SDL events, frame orchestrator (`src/frame/`), command encoding/submission, CPU pass coordination |
| Workers ×W | Job system for pass B/D (behavior, alarm BFS, audio binning). `W = --threads` (default 4 for experiments; recorded in CSV) |
| Audio | miniaudio callback + mixer; fed by lock-free SPSC queue |

No thread may block on the GPU except the main thread at defined sync points. Workers never touch the graphics API.

## 2. Simulation timing

- Fixed timestep **60 Hz** simulation (`dt = 1/60`). In play mode, render interpolates; in bench mode, **one sim tick per rendered frame**, vsync off, frame loop runs as fast as possible (R5).
- All research passes (A–E) run once per sim tick.
- Frame index `f` is the canonical clock for RNG streams and CSV rows.

## 3. The per-frame pipeline

Terminology: a **round trip** = one GPU→CPU handoff (CPU waits for GPU results produced *this frame* and reads them) followed by one CPU→GPU handoff (GPU waits for CPU results produced *this frame*). **K** = number of round trips per frame.

```
            GPU                                   CPU
Pass A   ┌─ light field (+ world grid @15Hz)
         │  spatial hash build (radix sort by cell)
         │  perception kernel → Perception[N]
         └─ signal  G1 ─────────────────────────► wait G1
         (GPU may run independent work:           Pass B: decide() for all agents
          tile albedo, particle sim)                alarm BFS, damage, events,
                                                    audio binning, RefineList
         wait C1 ◄────────────────────────────── signal C1
Pass C   ┌─ refine queries for RefineList          (K ≥ 2)
         └─ signal  G2 ─────────────────────────► wait G2
                                                  Pass D: re-decide refined agents,
                                                    emit alarm-LOS requests (K≥3)
         wait C2 ◄────────────────────────────── signal C2
Pass C'  ┌─ alarm LOS rays                         (K ≥ 3)
         └─ signal  G3 ─────────────────────────► wait G3 → Pass D' → signal C3
Pass C'' ┌─ 2nd alarm wave LOS rays (hops 5–8)     (K = 4)
         └─ signal  G4 ─────────────────────────► wait G4 → Pass D'' → signal C4
Pass E   ┌─ read Intent[N] → steering, separation,
         │  wall avoidance, integrate positions
         │  agent sort key (y) → albedo, composite,
         └─ particles, post, UI, present
```

| K | Round trips | What it buys (gameplay) |
|---|---|---|
| 1 | A→B→E | same-frame reaction to light |
| 2 | + C→D | precise decisions at light edges / LOS borders |
| 3 | + C'→D' | alarm confirmed by line of sight |
| 4 | + C''→D'' | second alarm wave (hops 5–8) |

### 3.1 Transfer strategies (runtime flag `--strategy`)

| ID | Name | Data path | Intra-frame syncs |
|---|---|---|---|
| **S0** | Delayed | GPU writes Perception into ring slot `f % 3`; CPU in frame f reads slot of frame `f − L` (`--delay`, default 2) without waiting; Intents written for frame f are consumed by pass E of frame `f+1`. K forced to 1 (refinement also delayed). | none |
| **S1** | Same-frame copy | Research buffers live in GPU-private memory; each GPU→CPU handoff adds a copy to a host-readable staging buffer; each CPU→GPU handoff copies staging → private. | K round trips + copies |
| **S2** | Same-frame direct | Research buffers are directly accessed by both processors (see §4.2 for the per-backend memory choice). No copies. | K round trips |
| **S3** | All-GPU | `decide()` ported to Slang (`shaders/decide.slang`); alarm replaced by 4-iteration grid diffusion; no refinement; CPU does not read agent data in-frame (only a delayed event readback for damage/audio, like S0). | none |

The game code (`src/sim`) receives `const Perception*` and writes `Intent*`; it does not know which strategy supplied the pointers (R2). The orchestrator in `src/frame/` owns strategy logic.

### 3.2 Submission modes (`--submit=chain|perpass`, default `chain`)

- **chain:** the whole frame's GPU work is encoded and submitted up front; GPU segments after each CPU pass begin with a GPU-side wait on a CPU-signaled event value. Lowest latency; used for the main experiments.
- **perpass:** each GPU segment is encoded and submitted only after the CPU pass finishes. Models engines that can't pre-submit. Reported as a secondary condition.

Both must be implemented on both backends with identical segmenting.

## 4. Backends

`src/gfx/Gfx.h` exposes a thin interface: buffers (with a `MemoryClass`), compute & render pipelines from Slang-compiled blobs, command recording, segment submit, timeline sync (`gpuSignal(v)`, `gpuWait(v)`, `cpuWait(v, timeout)`, `cpuSignal(v)`), GPU timestamps, clock calibration.

### 4.1 Sync primitives

| Operation | Metal (metal-cpp) | Vulkan 1.3 |
|---|---|---|
| Timeline object | `MTL::SharedEvent` | timeline `VkSemaphore` |
| GPU signals v | `cmdBuf->encodeSignalEvent(ev, v)` | submit with `pSignalSemaphoreValues = v` |
| CPU waits v | `ev->waitUntilSignaledValue(v, timeoutMs)` (spin-poll `signaledValue()` variant behind `--cpuwait=block|spin`) | `vkWaitSemaphores` (block) or poll `vkGetSemaphoreCounterValue` (spin) |
| CPU signals v | `ev->setSignaledValue(v)` | `vkSignalSemaphore` |
| GPU waits v | `cmdBuf->encodeWait(ev, v)` | submit with `pWaitSemaphoreValues = v` (wait-before-signal is legal for timeline semaphores) |
| Memory visibility GPU→CPU | Shared storage is coherent after event observed | barrier `SHADER_WRITE → HOST_READ` (stage `HOST`) before signal; `vkInvalidateMappedMemoryRanges` if not `HOST_COHERENT` |
| Memory visibility CPU→GPU | Shared storage; writes complete before `setSignaledValue` (use release fence `std::atomic_thread_fence`) | `vkFlushMappedMemoryRanges` if not coherent; host writes before `vkSignalSemaphore` are visible to the waiting submission |

`--cpuwait` is a recorded experimental factor (spin vs block changes latency and energy).

### 4.2 Memory classes for research buffers

| Buffer direction | S1 (copy) | S2 (direct) |
|---|---|---|
| **GPU-written, CPU-read** (Perception, RefineResults, GPU events) | Metal: `Private` + blit → `Shared` staging. Vulkan: `DEVICE_LOCAL` + `vkCmdCopyBuffer` → `HOST_VISIBLE\|HOST_CACHED` staging | Metal: `Shared`. Vulkan: `HOST_VISIBLE\|HOST_CACHED` (system memory; GPU writes over PCIe, CPU reads cached) |
| **CPU-written, GPU-read** (Intent, RefineList, requests) | Metal: `Shared` staging + blit → `Private`. Vulkan: `HOST_VISIBLE` staging + copy → `DEVICE_LOCAL` | Metal: `Shared`. Vulkan: `DEVICE_LOCAL\|HOST_VISIBLE` (ReBAR) if available; else `HOST_VISIBLE\|HOST_COHERENT` system memory. **Variant recorded in CSV** (`rebar=1/0`) |

Notes:
- On Vulkan, **do not** make the CPU read from ReBAR (`DEVICE_LOCAL|HOST_VISIBLE`) memory: CPU reads across PCIe from uncached device memory are extremely slow. That would be a strawman. Write this reasoning into DECISIONS.md.
- `--memvariant=` allows forcing alternatives for sensitivity analysis (e.g., `s2_readback=rebar` to demonstrate the slow path). Default is the table above.
- Non-research buffers (tiles, SDF, textures, particles) are `Private` / `DEVICE_LOCAL` in every condition.
- On Metal, `MTLResourceHazardTrackingModeUntracked` for research buffers; explicit sync only.

### 4.3 Shaders

Slang sources in `shaders/`, compiled by CMake to SPIR-V (`slangc -target spirv`) and Metal (`-target metal`). Same entry points, same thread-group sizes (64 threads for agent kernels, 8×8 for image kernels). Struct layouts come from `shaders/shared/layouts.slang` + generated `src/gfx/layouts.gen.h`; a unit test compares sizes/offsets.

## 5. Buffer layouts (std430-compatible, little-endian)

```
// Agent SoA (GPU-private except where noted; N = capacity)
float2   AgentPos[N]        // px
float2   AgentVel[N]
uint32   AgentMeta[N]       // bits 0-1 type, 2-4 state, 5-15 state_timer (frames), 16-31 flags
float    AgentHP[N]

struct Perception {         // 32 B, research buffer (GPU → CPU)
  float  lit;               // luminance at agent position (0..~3)
  float2 lit_grad;          // gradient of luminance (per px), from half-res mip
  float  dist_player;       // px
  uint32 los_flags;         // bit0 LOS to player (coarse), bit1 near-occluder-edge, bit2 in lantern cone
  uint32 neighbor_count;    // agents in 3x3 hash cells within 48 px (capped 255)
  uint32 cell;              // spatial hash cell index
  uint32 frame;             // frame index written (for staleness/audit checks)
};

struct Intent {             // 16 B, research buffer (CPU → GPU)
  float2 desired_dir;       // normalized or zero
  float  desired_speed;     // px/s
  uint32 packed;            // bits 0-2 state, 3 alarmed, 4 speed_boost, 5 despawn, 8-31 frame (low 24 bits)
};

struct RefineRequest { uint32 agent; uint32 kind; float2 target; };        // 16 B (CPU → GPU)
struct RefineResult  { uint32 agent; float lit_ss; float2 lit_grad_ss;     // 16 B
                       /* kind LOS: lit_ss = 1/0 visibility, grad = hit point */ };

uint2    CellRange[cells]   // spatial hash: [begin, end) into SortedIds; cell = 16 px
uint32   SortedIds[N]       // agent ids sorted by (cell, id) — deterministic
struct GpuEvents { uint32 count; uint32 pad[3]; GpuEvent ev[1024]; };      // contacts, beam→sigil hits
struct FrameConsts { ... player, lantern, dynamic lights, dt, frame, K, strategy ... }; // CPU → GPU, ring ×3
```

Research buffers (Perception, Intent, RefineRequest/Result, CellRange + SortedIds read-only on CPU for alarm BFS, GpuEvents) use the memory class of §4.2. **CellRange and SortedIds count as GPU→CPU research buffers** because the alarm BFS reads them.

Capacity: N up to 131,072. RefineRequest capacity N/4.

## 6. CPU passes

- **Pass B** — parallel-for over agents in chunks of 1024 on W workers:
  1. `decide(state_i, perception_i, world)` → Intent_i (pure, R3).
  2. Damage from light; state transitions to BURNING; emit sim events into per-worker queues.
  3. Ambiguity test → per-worker RefineRequest queues.
  Then single-threaded: alarm BFS (§GDD 5.3) over neighbor graph using CellRange/SortedIds; merge queues deterministically (worker queues concatenated in chunk order); write RefineRequest count; audio binning.
- **Pass D / D' / D''** — read RefineResults, re-run `decide()` with refined perception for those agents only, overwrite their Intents, emit next requests.
- The CPU writes each Intent exactly once per pass; Intents not touched keep pass B values.

## 7. Determinism

- Within a (backend, strategy, K, seed) tuple, repeated runs must produce identical agent state hashes at frames 600/1200/1800 (tested). Requires: deterministic spatial hash (radix sort of `(cell<<17)|id`), no float atomics, deterministic reductions, fixed dt, seeded RNG only.
- **Across backends bit-exactness is NOT required** (GPU float differences, fast-math). Instead, the research protocol checks statistical equivalence (distributions of kills, state occupancy, mean speed within tolerance).
- Across strategies trajectories legitimately diverge (that is the effect being measured).

## 8. Instrumentation (R6)

- GPU timestamps at start/end of each pass (one encoder per pass):
  - Metal: `MTL::CounterSampleBuffer` with `MTLCommonCounterSetTimestamp`, stage-boundary sampling (Apple GPUs support sampling at encoder boundaries). Clock correlation via `device->sampleTimestamps(cpu, gpu)` at startup and every 600 frames.
  - Vulkan: `vkCmdWriteTimestamp2`; clock correlation via `VK_KHR_calibrated_timestamps` (or `VK_EXT_` fallback).
- CPU timestamps: `std::chrono::steady_clock` (monotonic), per stage, per worker (max over workers reported).
- **Per round trip i:** `t_gpu_signal_i` (GPU timestamp at end of producing pass, converted to CPU clock), `t_cpu_wake_i` (CPU clock when wait returned), `t_cpu_signal_i`, `t_gpu_resume_i` (GPU timestamp at start of consuming pass). Derived: `lat_g2c = wake − gpu_signal`, `lat_c2g = gpu_resume − cpu_signal`.
- Frame record flushed to a preallocated ring and written to CSV at run end (no I/O in the frame loop).

## 9. Module boundaries

```
app  → frame, sim, procgen, render, audio, bench
frame → gfx, sim, render (orchestrates passes & strategies)
sim  → core (no gfx!)       // decide(), alarm BFS, damage, player, events
render → gfx, core          // pipelines for passes A, C, E, albedo, post
procgen → core
audio → core
bench → frame, sim, core
```

`sim` must compile and run its unit tests without any graphics backend (headless tests of `decide()` and alarm BFS with synthetic Perception arrays).
