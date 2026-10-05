# Work status (resume here)

_Last updated 2026-10-05 (session 5)._

- **M0 — Scaffold:** accepted on Mac (Metal + MoltenVK). **[PC] checks: ALL PASS** on the branch
  `pc/m0-m1` (2026-10-02; one MSVC fix, DECISIONS #27 on that branch). M0 is fully closed once the branch is merged.
- **M1 — Round-trip microbenchmark (GO/NO-GO gate):** implemented on both backends and verified
  on the Mac. **Not closed:** Mac AC re-run done (still fails run-to-run, see below); needs (2) PC runs on the 3060 Ti and the
  4060 Ti, (3) the human's go/no-go decision. Do not start M2 before that. New evidence for that
  decision: the M5 sync probe, finding 7. A hot GPU gives a round trip ≈ 55 µs, stable, and polling isn't viable.
- **PC session:** in progress on branch `pc/m0-m1` (see "PC session progress" below). M0 checks
  pass. M1 runs have started, but the plan grew to **about 6 days of runs** because of the human's choice of the full `s2_rebar`
  protocol. On resume: fetch the
  branch (command below), read its `docs/PC_PROGRESS.md`, report progress, and wait. Merge only when
  the human says so (section C).

## Resume checklist (Mac)
```
export PATH="$HOME/Library/Python/3.9/bin:$PATH"
cd ~/Project/Lightbound
# SSH to github.com may time out on this network; the repo is public, so fetch over HTTPS:
GIT_TERMINAL_PROMPT=0 git -c credential.helper= fetch https://github.com/jttarigan/Lightbound.git \
    pc/m0-m1:refs/remotes/origin/pc/m0-m1
git log --oneline main..origin/pc/m0-m1
git show origin/pc/m0-m1:docs/PC_PROGRESS.md
```
(macOS has no GNU `timeout` command, so don't wrap git calls in it.)

## PC session progress (as of 2026-10-05, branch head 5faba40, last push 2026-10-02 11:01 +0700)
Read from `origin/pc/m0-m1:docs/PC_PROGRESS.md`. The branch changes `src/core/Math.h` (MSVC fix)
and `docs/DECISIONS.md` (#27), and adds `docs/PC_PROGRESS.md`.
- PC rig: i7-12700F (12C/20T), 32 GB, OEM MSI board MS-B9241 (BIOS 8.00, 2022), Windows 11 Home
  10.0.26200, RTX 3060 Ti installed (driver 566.36). VS Build Tools 2022 17.14 (MSVC 19.44; CMake 3.31 + Ninja
  1.12 via `vcvars64.bat`), Python 3.13 with pandas/matplotlib/scipy.
- Human decisions (2026-10-02): Vulkan SDK 1.4.363.0 installed; power plan **Ultimate Performance**
  (the task asks for "High performance"); NVIDIA "Prefer maximum performance"; background apps
  closed; **`s2_rebar` at the full protocol** (≈ 23 h per run, see below); **ReBAR enabled in the BIOS**
  for the primary `rebar-on` runs.
- **§2 M0 [PC]: ALL PASS.** MSVC `/W4 /WX` build clean after one fix (`alignUp` overloads collided
  because `size_t` == `uint64_t` on MSVC x64. They are now one `requires std::is_unsigned_v<T>` template, DECISIONS
  #27. Both call sites pass same-typed args, so it also compiles on macOS clang). ctest 36/36 including
  `microbench_smoke`. Play with validation: exit 0, layer enabled, no `ERROR [vk]`. The bench stub exits 2.
  Microbench with validation: 0 failures, 0 errors, 0 warnings.
- Facts from the validation run: ReBAR was **off** (BAR heap 214 MB); link **gen3 x16** under load;
  raw copy 16 MiB d2h 12.4 / h2d 13.2 GB/s; **empty round trip p50 ≈ 76 µs with validation ON**
  (g2c ≈ 45, c2g ≈ 31). The timing runs should come in lower. See finding #4.
- **`s2_rebar` is very slow by design.** The CPU reads uncached BAR memory: ≈ 7.1 s per iteration at 16 MiB,
  ≈ 23 h per protocol run, while all the other paths take ≈ 55 min per run (`s2_coherent` ≈ 49 min of it).
- PC plan (from `PC_PROGRESS.md`): **A** 3060 Ti ReBAR off, 5 fast paths × 3 runs →
  `results\m1_3060_rebar-off` (started 2026-10-02 10:13, ≈ 3 h). **B** human reboots to the BIOS to enable ReBAR.
  **C** 3060 Ti ReBAR on, full protocol incl. `s2_rebar`, 3 runs → `results\m1_3060` (≈ 3 days).
  **D** GPU swap to the 4060 Ti, same as C → `results\m1_4060` (≈ 3 days). **E** `PC_REPORT.md` + push.
  Known gaps: no `s2_rebar` with ReBAR off, and no ReBAR-off runs on the 4060 Ti.
  Risk: a Windows Update auto-restart during the multi-day runs (the human should pause updates).
- **Nothing has been pushed since 2026-10-02 11:01** (before step A finished). From the Mac I can't tell whether
  step C is running now (that fits the ≈ 3-day estimate) or the PC session stalled. Ask the human.
  Earliest full PC results: ≈ 2026-10-08/09 if C started on 10-02 and D follows immediately.
- The PCIe gen3 question (finding #5) still has **no recorded human decision** on the branch.
- **Merge rehearsal (2026-10-05):** a trial merge of `origin/pc/m0-m1` (5faba40) into `main` in a scratch worktree
  was clean (no conflicts). The merged tree builds with 0 warnings and passes ctest 36/36 on both Metal and MoltenVK.
  The real merge (section C) should therefore need no code fixes on the Mac side.
- DECISIONS.md: the PC branch appended #27. Main still doesn't touch DECISIONS.md, so the numbers
  don't collide at merge.

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

### AC-power re-run (2026-09-24, 3 runs, commit 055d696, thermal "fair" throughout) — `results/m1_mac_ac/`
Metal, chain/spin, round trip p50 / p99 (µs):

| path | empty | 4K | 64K | 256K | 1M | 4M | 16M |
|---|---:|---:|---:|---:|---:|---:|---:|
| empty | 214 / 316 | | | | | | |
| s2_direct | | 201 / 317 | 276 / 319 | 283 / 327 | 305 / 349 | 316 / 493 | 842 / 1814 |
| s1_copy | | 292 / 344 | 294 / 359 | 305 / 379 | 352 / 442 | 393 / 739 | 1587 / 3001 |

- 0 failures; raw GPU copy 16 MiB ≈ 55–58 GB/s (vs ≈ 38 on battery: battery was throttling).
- Run-to-run: 26/52 cells ≥ 10 % (worst primary-condition cell 29 %) → still **FAIL**, but many cells
  now repeat within 1–3 % (e.g. s2_direct chain/spin 4K 201/200/202, 1M 304/303/309).
- g2c ≈ 32–40 µs. c2g is bimodal: ≈ 50 µs (fast path, 35–45 % of iterations) or ≈ 235 µs.
- Key observation: for 64K–4M the round trip stays ≈ 280–316 µs while the CPU's read+write time
  grows from 2 to 124 µs — c2g shrinks as CPU time grows. The GPU appears to resume on a coarse
  ≈ 250–300 µs schedule after it starts waiting on the event, not promptly when the CPU signals,
  except on the fast path. Floor (p10) of the empty round trip ≈ 80 µs.

### Findings the human needs to see
1. **Sync latency on M5 is dominated by the CPU→GPU hand-off and is multimodal.** GPU→CPU (g2c) is
   ≈ 40 µs and stable. CPU→GPU (c2g: `setSignaledValue` → waiting encoder starts) has three discrete
   levels ≈ **45, 155, 235 µs**; which level dominates persists for minutes and changes between runs.
   `setSignaledValue` itself takes 1–2 µs, so the time is spent in the driver/GPU resume path. Thread
   QoS makes no difference. Putting both segments in one command buffer makes no difference. A
   concurrent process doing event round trips pulled c2g to ≈ 50 µs (observed accidentally), a
   same-process GPU compute load did not (**superseded by finding 7:** sustained load on a second queue does bring c2g to ≈ 31 µs; how the earlier test differed was not recorded). This is consistent with GPU idle-state exit latency under
   power management; battery power + thermal "fair" (later runs) may make it worse.
2. **Run-to-run acceptance (< 10 %) fails** on battery: per-run p50s jump between ≈ 195, ≈ 270 and
   ≈ 450 µs plateaus (the 45/155/235 c2g levels). Block-wait cells are more stable than spin.
3. **Payload cost is small until ~1 MiB** on M5 (flat ≈ 200 µs to 256 KiB); at 16 MiB S2 is ≈ 2×
   faster than S1 (864 vs 1669 µs) — early support for H5's direction.
4. The go criterion cannot be evaluated without PC data. If the RTX round trips land around
   50–150 µs (my expectation for timeline semaphores over PCIe — not measured), the M5 as measured here would **not** be
   "clearly lower" → NO-GO by the letter of protocol §6. **Early PC evidence (2026-10-02,
   validation ON, not a timing run): the 3060 Ti's empty round trip p50 ≈ 76 µs vs M5 ≈ 214 µs (AC).**
   If the timing runs confirm this, M1 points to NO-GO on the protocol's criterion unless the human
   takes one of the options below. Options for the human (not implemented):
   (a) re-run plugged in first; (b) add a "hot GPU" microbench variant where the GPU has independent
   work during the CPU step, as it does in the real frame (docs/05 §3 "GPU may run independent work");
   (c) report latency modes/p10 alongside p50; (d) add a memory-polling sync path (GPU spins on a
   Shared-memory flag) as an alternative to `MTLSharedEvent` — a spec change, needs approval.
   **Update 2026-10-05 (finding 7): (b) is strongly supported by a probe (hot GPU → M5 round trip
   ≈ 53–57 µs, run-to-run within 2 %); (d) does not work on M5 (the GPU never sees the CPU's write).**
5. **Open: the PC's PCIe link is gen3, not gen4.** Protocol §2 specifies P-3060 = PCIe 4.0 x16 and
   P-4060 = PCIe 4.0 x8. On the PC, nvidia-smi reports `pcie.link.gen.max` = 3 and `width.max` = 16,
   so the platform caps the link at gen3. Expected links: 3060 Ti at gen3 x16 (≈ 15.8 GB/s theoretical,
   the same as gen4 x8), and 4060 Ti probably at gen3 x8 (≈ 7.9 GB/s). The x16-vs-x8 ratio that H5
   compares is unchanged, but absolute transfer costs will be higher than on gen4, and the "P-3060" and
   "P-4060" labels won't match §2. `docs/PC_CLAUDE_TASK.md` §3 step 3 expects to see `gen4 x16` /
   `gen4 x8` in the CSV header, so the PC Claude should treat gen3 as known, not as a failure. The
   PC Claude flagged it to the human; **no decision is recorded yet.** Options: (a) accept and record
   the measured gen3 links as the platform definition (protocol §2 already says "**measured** link
   gen/width"); (b) find a gen4 board/slot; (c) treat it as a limitation in the write-up. This needs a
   human decision at go/no-go (CLAUDE.md rule 5).
6. `docs/PC_CLAUDE_TASK.md` calls the GitHub repo private, but it is public (confirmed from the Mac
   and the PC). The human decides whether that's intended.
7. **M5 sync probe (2026-10-05): the slow c2g is a GPU power-state effect, and polling cannot
   replace the event.** `tools/probes/sync_probe.cpp` is a standalone metal-cpp program, not part of the build or the
   research pipeline. It uses the same timing definitions as the microbench (empty payload, chain, CPU spin).
   Variants are interleaved in rotating blocks, 10 rounds × 300 iterations per variant, 3 processes, AC
   power, no thermal warning. Data: `results/probe_sync_2026-10-05/` (local). Round trip p50 per process run (µs):

   | variant | what runs alongside | rt p50 runs 1/2/3 | rt p99 | g2c p50 | c2g p50 |
   |---|---|---|---|---|---|
   | idle | nothing (= microbench) | 170 / 67 / 67 | ≈ 220 | 31 | 37–111 |
   | busy | 2nd queue: small ALU kernel back to back | 54 / 54 / 53 | ≈ 85 | 23 | 31 |
   | heavy | 2nd queue: whole-GPU kernels back to back | 57 / 57 / 57 | ≈ 102 | 25 | 32 |
   | half | busy kernel at ≈ 50 % duty | 61 / 61 / 60 | ≈ 209 | 25 | 32 |
   | light | tiny kernel every ≈ 200 µs | 72 / 71 / 71 | ≈ 200 | 33 | 39 |
   | pingpong | 2nd thread doing event round trips | 68 / 67 / 68 | ≈ 215 | 33 | 35 |

   - With sustained GPU load the hand-off is fast and stable: c2g ≈ 31 µs at 100 % of iterations, round trip
     ≈ 53–57 µs, and run-to-run within 2 %. Even a load that fills the whole GPU adds only ≈ 3 µs. So the
     45/155/235 µs c2g levels come from the GPU's power state, not the event mechanism.
   - "Idle" is bimodal per block (≈ 62–67 or ≈ 196 µs). It is slow at every process start (round 0) and in some
     later blocks. Here it is fast more often than in the microbench, probably because the neighbouring
     load blocks keep the GPU clocked up (hysteresis). That is why idle must be measured in isolation, as M1 does.
   - **Memory polling does not work:** a kernel spinning on a Shared-buffer flag never saw the CPU's
     store (atomic load or atomic RMW). It hit the 20 ms bound in 298 of 300 iterations, and a GPU flag write
     reached the CPU only when its command buffer completed. This fits Metal's coherence model (Shared
     memory is coherent at command-buffer boundaries). Option (d) is not viable on M5.
   - Also seen: an `encodeSignalEvent` in the middle of a command buffer reached the CPU only when the whole
     command buffer finished, not when the preceding encoder did. Producer and consumer need separate command buffers
     (the microbench already does this).
   - **What it means for go/no-go:** the microbench as specified measures the M5 with a cold GPU. A real frame keeps
     the GPU busy (render passes, vsync off), so the hot regime is the realistic one for the game. Hot M5 ≈ 55 µs is
     about the same as the 3060 Ti's ≈ 76 µs *with validation on*. The PC's timing runs (no validation) will decide whether
     the M5 is "clearly lower", and that looks like a close call, not a clear win. A fair hot-GPU condition would also have
     to run on the RTX cards (R1: same algorithm on both backends). The 3060 Ti may also speed up when hot, although
     "Prefer maximum performance" already holds its clocks. Adding a hot-GPU cell to `--mode=microbench` is a
     protocol change and needs the human's approval.

### Deviations from spec
Recorded in `docs/DECISIONS.md` #17–#26 (ReBAR read policy, binding convention, headless microbench,
extra micro.csv columns/rows, Vulkan variant definitions, one command buffer per segment, timestamp
& clock method, Vulkan 1.3 requirement, per-iteration verification, microbench CLI semantics).

## What the human must run

### A. Mac, plugged in (AC power, Low Power Mode off, no other apps, display on) — DONE 2026-09-24 (`results/m1_mac_ac`)
```
export PATH="$HOME/Library/Python/3.9/bin:$PATH"
cd ~/Project/Lightbound
cmake --build build
python3 tools/run_microbench.py --exe build/lightbound --out results/m1_mac_ac
```
(~7 min. Check the log header says `power_source: AC Power`.)

### B. PC (Windows 11) — M0 checks + M1 runs in one session — IN PROGRESS since 2026-10-01 (branch `pc/m0-m1`)
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
- Git: repository initialised 2026-09-24 on `main` (local identity jostarigan), remote
  `git@github.com:jttarigan/Lightbound.git` (Mac pushes with a deploy key over SSH). On 2026-10-01 SSH
  to github.com timed out from the Mac's network (ports 22 and 443). HTTPS fetch works (see the
  resume checklist), but pushing needs SSH. **Local `main` is ahead of `origin/main` (baf605a)** with
  the session-3, 4 and 5 status commits, which are unpushed. Push with `git push origin main`
  when SSH works. The PC branch doesn't depend on them. The preliminary
  `results/m1_prelim` runs predate the repo (their CSVs say `nogit`).
- Build dirs: `build/` (Metal), `build-vk/` (Vulkan via MoltenVK). Toolchain PATH:
  `export PATH="$HOME/Library/Python/3.9/bin:$PATH"`. Python analysis deps are installed (user site).
- Preliminary data: `results/m1_prelim/` (6 Metal + 3 MoltenVK runs, battery) — not valid for the study.
