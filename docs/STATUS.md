# Work status (resume here)

_Last updated 2026-09-24 (session 2)._

- **M0 — Scaffold:** accepted on Mac (Metal + MoltenVK). **[PC] checks deferred** (DECISIONS #16).
- **M1 — Round-trip microbenchmark (GO/NO-GO gate):** implemented on both backends and verified
  on the Mac. **Not closed:** needs (1) a Mac re-run on AC power, (2) PC runs on the 3060 Ti and the
  4060 Ti, (3) the human's go/no-go decision. Do not start M2 before that.

## M1 report (Mac side, preliminary)

### Built
- `Gfx.h` research interface (buffers with `MemoryClass`, compute pipelines from Slang blobs with
  push constants, binding sets, GPU segments with wait/signal, `cpuSignal/cpuWait(spin|block)`,
  GPU timestamps per pass, clock calibration, R7 research-copy counter), implemented in
  `src/gfx/metal/MetalGfx.cpp` and `src/gfx/vulkan/VulkanGfx.cpp`; both backends headless-capable.
- `src/frame/MemoryPolicy.*`: docs/05 §4.2 placement table for S1 / S2 / Vulkan variants.
- `src/bench/Microbench.*`: protocol §6, all paths (`empty`, `s1_copy`, `s2_direct`, Vulkan
  `s2_hostcached`, `s2_rebar`, `s2_coherent`) × 6 payloads (4 KiB–16 MiB) × chain/perpass ×
  spin/block, 200 warm-up + 2000 measured iterations per cell, raw GPU copy bandwidth, per-iteration
  checksums both ways + R7 copy-count check, `micro.csv` with full header (sysinfo incl. power source,
  thermal state, memory types, PCIe link via nvidia-smi under load, ReBAR).
- `src/bench/SysInfo.*`, build-time `GitInfo.h`, `shaders/microbench.slang` (push constants,
  deterministic per-group checksum).
- `tools/analysis/micro_report.py` (figure 1, summary tables, run-to-run check, latency modes, go
  criterion), `tools/run_microbench.py` (N process restarts with cooldown + report).
- Tests: 6 new unit tests (patterns/checksums, payload parsing, memory policy) + `microbench_smoke`
  GPU test (all paths, validation on). ctest 36/36 on Metal and on MoltenVK; 0 warnings.

### Verified on the Mac
- Every iteration of every run: CPU read-back sum = GPU-written pattern, GPU checksum = CPU-written
  pattern, research copies = 2 (S1) / 0 (S2). **0 failures** in 6 Metal + 3 MoltenVK full runs. Validation-on
  smoke runs (Metal API validation; ctest `microbench_smoke`): 0 errors. (No Vulkan validation layer
  on the Mac — that check happens on the PC.)
- Timing chain cross-checked with a standalone metal-cpp probe (same clock on both sides, no
  calibration involved): same latencies as the benchmark.

### Numbers (preliminary: **on battery**, protocol requires AC; see `results/m1_prelim/report/`)
Metal, chain/spin, pooled over 6 runs, round trip p50 / p99 (µs):

| path | empty | 4K | 64K | 256K | 1M | 4M | 16M |
|---|---:|---:|---:|---:|---:|---:|---:|
| empty | 209 / 522 | | | | | | |
| s2_direct | | 201 / 515 | 215 / 531 | 208 / 406 | 247 / 391 | 325 / 790 | 864 / 2050 |
| s1_copy | | 228 / 402 | 225 / 470 | 241 / 563 | 336 / 723 | 415 / 1129 | 1669 / 3744 |

Raw GPU copy 16 MiB (median of runs): ≈ 38–39 GB/s each direction. Fast-mode floor (p10) of the
empty round trip ≈ 90 µs. MoltenVK control (3 runs): empty 242 µs p50, similar shape.

### Findings the human needs to see
1. **Sync latency on M5 is dominated by the CPU→GPU hand-off and is multimodal.** GPU→CPU (g2c) is
   ≈ 40 µs and stable. CPU→GPU (c2g: `setSignaledValue` → waiting encoder starts) has three discrete
   levels ≈ **45, 155, 235 µs**; which level dominates persists for minutes and changes between runs.
   `setSignaledValue` itself takes 1–2 µs, so the time is spent in the driver/GPU resume path. Thread
   QoS makes no difference. Putting both segments in one command buffer makes no difference. A
   concurrent process doing event round trips pulled c2g to ≈ 50 µs (observed accidentally), a
   same-process GPU compute load did not. This is consistent with GPU idle-state exit latency under
   power management; battery power + thermal "fair" (later runs) may make it worse.
2. **Run-to-run acceptance (< 10 %) fails** on battery: per-run p50s jump between ≈ 195, ≈ 270 and
   ≈ 450 µs plateaus (the 45/155/235 c2g levels). Block-wait cells are more stable than spin.
3. **Payload cost is small until ~1 MiB** on M5 (flat ≈ 200 µs to 256 KiB); at 16 MiB S2 is ≈ 2×
   faster than S1 (864 vs 1669 µs) — early support for H5's direction.
4. The go criterion cannot be evaluated without PC data. If the RTX round trips land around
   50–150 µs (my expectation for timeline semaphores over PCIe — not measured), the M5 as measured here would **not** be
   "clearly lower" → NO-GO by the letter of protocol §6. Options for the human (not implemented):
   (a) re-run plugged in first; (b) add a "hot GPU" microbench variant where the GPU has independent
   work during the CPU step, as it does in the real frame (docs/05 §3 "GPU may run independent work");
   (c) report latency modes/p10 alongside p50; (d) add a memory-polling sync path (GPU spins on a
   Shared-memory flag) as an alternative to `MTLSharedEvent` — a spec change, needs approval.

### Deviations from spec
Recorded in `docs/DECISIONS.md` #17–#26 (ReBAR read policy, binding convention, headless microbench,
extra micro.csv columns/rows, Vulkan variant definitions, one command buffer per segment, timestamp
& clock method, Vulkan 1.3 requirement, per-iteration verification, microbench CLI semantics).

## What the human must run

### A. Mac, plugged in (AC power, Low Power Mode off, no other apps, display on)
```
export PATH="$HOME/Library/Python/3.9/bin:$PATH"
cd ~/Project/Lightbound
cmake --build build
python3 tools/run_microbench.py --exe build/lightbound --out results/m1_mac_ac
```
(~7 min. Check the log header says `power_source: AC Power`.)

### B. PC (Windows 11) — M0 checks + M1 runs in one session
Automated: copy only `docs/PC_CLAUDE_TASK.md` to the PC and tell Claude Code there: *Read
PC_CLAUDE_TASK.md and do it.* It clones the repo, works on branch `pc/m0-m1`, keeps
`docs/PC_PROGRESS.md` (resumable across the GPU-swap reboot), writes `docs/PC_REPORT.md`, and pushes
fixes + gzipped results. Back on the Mac: *Merge pc/m0-m1 and continue from docs/PC_REPORT.md*.
Manual equivalent:
Prerequisites: Visual Studio 2022 (C++ workload), LunarG Vulkan SDK (validation layers), CMake ≥ 3.25,
Ninja, Python ≥ 3.9 with `pip install pandas matplotlib scipy`, git. Power plan "High performance",
NVIDIA "Prefer maximum performance". From an **x64 Native Tools Command Prompt for VS 2022**:
```
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DLB_BACKEND=vulkan
cmake --build build
ctest --test-dir build --output-on-failure
build\lightbound.exe --mode=play --validation=on --exit-after=300
echo exit code: %ERRORLEVEL%
python tools\run_microbench.py --exe build\lightbound.exe --out results\m1_3060 --tag rebar-on
```
Then swap to the 4060 Ti (same slot) and run `python tools\run_microbench.py --exe build\lightbound.exe
--out results\m1_4060 --tag rebar-on`. Optional (protocol §9): ReBAR off in BIOS, repeat with
`--tag rebar-off` into separate directories. Note: `s2_rebar` at 16 MiB reads uncached PCIe memory
from the CPU on purpose (the slow path); a full run may take tens of minutes.

M0 pass = build clean (expect some MSVC `/W4 /WX` fixes — Vulkan/Windows code never compiled with
MSVC), ctest all pass (the `microbench_smoke` test runs every path with validation on), play log shows
`Vulkan validation layer enabled`, no `ERROR [vk]`, exit 0.

### C. Combine and decide
```
python3 tools/analysis/micro_report.py results/m1_mac_ac results/m1_3060 results/m1_4060 --out results/m1_report
```
Paste back `results/m1_report/report.md` and any errors. Keep ReBAR-off runs in their own report
(runs of one platform are pooled).

## Housekeeping
- Git: repository initialised 2026-09-24 on `main` (local identity jostarigan). The preliminary
  `results/m1_prelim` runs predate it (their CSVs say `nogit`).
- Build dirs: `build/` (Metal), `build-vk/` (Vulkan via MoltenVK). Toolchain PATH:
  `export PATH="$HOME/Library/Python/3.9/bin:$PATH"`. Python analysis deps are installed (user site).
- Preliminary data: `results/m1_prelim/` (6 Metal + 3 MoltenVK runs, battery) — not valid for the study.
