# PC report — M0 [PC] and M1 [PC] on the RTX 3060 Ti

Hand-over from the Windows PC to the Mac, per `docs/PC_CLAUDE_TASK.md` §4. Branch `pc/m0-m1`,
work done 2026-10-01 … 2026-10-13. The session log with every intermediate step is
`docs/PC_PROGRESS.md`. **Only one card was measured: the RTX 3060 Ti. The human has no RTX 4060 Ti,
so `results/m1_4060` does not exist** and the combined report `results/m1_pc_report` holds the 3060 Ti
alone.

> **Correction (Mac, 2026-10-13):** the RTX 4060 Ti does exist. The user says it is in service and will be
> available in 2–3 weeks (≈ 2026-10-27 to 11-03). Step D (4060 Ti, same PC, same slot) is postponed, not dropped.
> See `docs/STATUS.md`, to-do 4.

Facts only; the go/no-go decision belongs to the human.

## 1. Environment

| | |
|---|---|
| CPU / RAM | Intel Core i7-12700F (12 cores / 20 threads), 32 GB |
| Board / BIOS | MSI MS-B9241 (OEM), BIOS 8.00 (2022-01-26) — **the slot runs at PCIe gen3**, not gen4 |
| GPU / driver | NVIDIA GeForce RTX 3060 Ti (8 GB), driver 566.36, Vulkan 1.3.289 |
| PCIe link, measured under load | **gen3 x16** (max gen3 x16) in every run — protocol §2 assumed gen4 x16 |
| ReBAR | off by BIOS default (BAR heap 214 MB) for the first series; **on** (BAR heap 8024 MB) for the primary series |
| OS / toolchain | Windows 11 Home 10.0.26200; VS Build Tools 2022 17.14 (MSVC 19.44), CMake 3.31.6, Ninja 1.12.1, Vulkan SDK 1.4.363.0, Python 3.13 |
| Power plan | Ultimate Performance (human decision); NVIDIA "Prefer maximum performance" confirmed by the human; sleep/hibernate off |
| Clock domain | `QueryPerformanceCounter`; Vulkan calibrated timestamps available |

The Vulkan SDK was not installed at the start (hard blocker for the build and for the validation
checks); the human approved `winget install KhronosGroup.VulkanSDK`.

## 2. M0 [PC] — all five checks PASS (2026-10-02)

| # | Check | Result |
|---|---|---|
| 2.1 | Configure + build Release/vulkan with MSVC `/W4 /WX` | **PASS** after one fix (below). Zero compiler and linker warnings; the Vulkan backend compiled clean on the first MSVC build. |
| 2.2 | `ctest` | **PASS 36/36**, incl. `microbench_smoke` with validation on. |
| 2.3 | `--mode=play --validation=on --exit-after=300` | **PASS.** `Vulkan validation layer enabled (VK_LAYER_KHRONOS_validation)`, no `ERROR [vk]`, `backend errors: 0`, exit 0. `results/m0_play_validation.log`. |
| 2.4 | `--mode=bench` | **PASS.** Prints `mode 'bench' is not implemented yet (milestone M4)`, exit 2. |
| 2.5 | Full-payload microbench with validation on (`--frames=30 --warmup=5`) | **PASS.** Exit 0, `0 verification/R7 failures, 0 API errors`, **0** `ERROR [vk]`, **0** `WARN [vk]` (no performance warnings to list). `results/validation_check.csv.gz` / `.log`. |

Validation findings: none. The research sync code (barriers, timestamp stages, semaphores) produced
no validation message of any kind on the Khronos layer.

The one build failure was Windows-only: `alignUp(usize)` and `alignUp(u64)` are the same function on
MSVC x64 (`size_t` == `unsigned long long`), C2084 in the test TUs. Fixed with one constrained
template (DECISIONS #27); behaviour unchanged.

## 3. Source changes made on the PC (all on `pc/m0-m1`)

| File | Why | DECISIONS |
|---|---|---|
| `src/core/Math.h` | `alignUp` as one `requires std::is_unsigned_v<T>` template instead of three overloads (MSVC C2084). | #27 |
| `src/bench/MicroResume.cpp`, `.h` (new), `src/bench/Microbench.cpp`, `.h`, `src/app/Cli.cpp`, `.h`, `src/app/main.cpp`, `src/CMakeLists.txt` | `lightbound --resume`: a microbench run continues from its last complete cell after a shutdown; refuses if binary, options, GPU, driver, ReBAR, power plan differ. Needed because one full run takes 24 h on the PC. Nothing inside a timed interval changed. | #28 |
| `src/bench/Microbench.cpp` | GPU→host clock mapping refreshed by elapsed time (20 ms) as well as every 500 iterations, and `c2g` of a long iteration uses a calibration taken at its end. Fixes a ≈ 32 ppm GPU/QPC drift that biased the g2c/c2g split (not `rt`) on the PC. | #30 |
| `tools/run_microbench.py` | `--resume` for the series (finished runs skipped, interrupted run continued); log written straight to file; `printSummary` flush. | #28, #29 |
| `tools/analysis/micro_report.py` | UTF-8 in/out (cp1252 could not write "→", report came out empty on Windows); a run present as both `.csv` and `.csv.gz` is counted once. | #29, #31 |

DECISIONS.md entries added: **#27, #28, #29, #30, #31**.

Binaries: M0 checks and the first series used `6a2d02e`; everything from the clock-mapping fix on
used `6962eff` (`micro_report.py`'s last fix, #31, is Python only and changes no measurement).

## 4. M1 [PC] — RTX 3060 Ti

### 4.1 What was run

| Series | Condition | Paths | Runs | Directory |
|---|---|---|---|---|
| **Primary** | ReBAR **on**, binary `6962eff` | full protocol incl. `s2_rebar` (200 + 2000 iterations, all submit × wait modes, all payloads) | 3 × 23.8 h | `results/m1_3060` (= `results/m1_pc_report`) |
| ReBAR off | ReBAR **off**, binary `6962eff` | all paths **except `s2_rebar`** (≈ 23 h per run for that path alone; the human chose to spend that time on the ReBAR-on condition) | 3 × 52 min | `results/m1_3060_rebar-off` |
| ReBAR off, old calibration | ReBAR off, binary `6a2d02e` (before #30) | as above | 3 × 52 min | `results/m1_3060_rebar-off_old-calibration` — `rt` valid, g2c/c2g split biased by the drift; superseded, kept for reference |
| Disturbed run | ReBAR on, `6962eff` | full protocol | 1 | `results/m1_3060_run1_disturbed` — see 4.5; **not** part of any report |

Every run: exit 0, `0 verification/R7 failures, 0 API errors`, no `ERROR`/`WARN` line, no restart
(`--resume` was never needed). `sysinfo.pcie_link` was `gen3 x16 (max gen3 x16)` and
`sysinfo.power_plan` was `Ultimate Performance` in every header.

### 4.2 Primary series: ReBAR on (`results/m1_pc_report/report.md`)

Round trip p50 / p99 (µs), chain/spin, 3 runs pooled (6000 samples per cell):

| path | 0 | 4K | 64K | 256K | 1M | 4M | 16M |
|---|---:|---:|---:|---:|---:|---:|---:|
| empty | 60 / 85 | | | | | | |
| s2_direct | | 60 / 80 | 69 / 103 | 97 / 134 | 207 / 278 | 648 / 810 | 2395 / 2650 |
| s1_copy | | 73 / 105 | 89 / 119 | 131 / 179 | 312 / 412 | 1053 / 1296 | 3940 / 4437 |
| s2_hostcached | | 62 / 94 | 69 / 102 | 87 / 123 | 160 / 230 | 466 / 612 | 1759 / 2343 |
| s2_rebar | | 1748 / 1853 | 27164 / 27964 | 108717 / 110729 | 436664 / 442832 | 1757962 / 1769825 | 7031791 / 7063888 |
| s2_coherent | | 119 / 145 | 991 / 1211 | 3813 / 4365 | 14960 / 16343 | 59824 / 65164 | 239149 / 260482 |

- g2c / c2g p50 (µs), chain/spin: **empty 31.0 / 28.9**; **s2_direct 1 MiB 33.0 / 29.2**, 16 MiB
  46.0 / 33.5; s1_copy 1 MiB 115.6 / 115.0; s2_hostcached 1 MiB 33.0 / 29.9.
- Latency modes of the empty round trip (5 µs bins): g2c 30–35 µs (73 %); c2g 25–30 µs (48 %) and
  35–40 µs (40 %). No slow mode: the empty round trip is 60–65 µs for 36 % of samples, p99 85 µs.
- Raw GPU copy 16 MiB: d2h **12.38 GB/s**, h2d **13.27 GB/s** — the practical gen3 x16 maximum.
- **Run-to-run variation: PASS** — 124 cells, none ≥ 10 %; worst 8.4 % (s2_hostcached
  perpass/block 16M), worst in the primary condition 7.4 %.
- Run durations: 85532 / 85550 / 85545 s. `s2_rebar` takes 22.8 h of each run (16 MiB: 7.0 s per
  iteration — the CPU reads uncached BAR memory word by word, one core busy, GPU idle);
  `s2_coherent` 50 min; the other four paths together 3.5 min.

### 4.3 ReBAR off (`results/m1_3060_rebar-off/report/report.md`)

Round trip p50 / p99 (µs), chain/spin, 3 runs pooled:

| path | 0 | 4K | 64K | 256K | 1M | 4M | 16M |
|---|---:|---:|---:|---:|---:|---:|---:|
| empty | 60 / 99 | | | | | | |
| s2_direct | | 59 / 88 | 65 / 89 | 80 / 114 | 142 / 212 | 388 / 530 | 1392 / 1684 |
| s1_copy | | 74 / 120 | 90 / 129 | 131 / 178 | 312 / 408 | 1074 / 1298 | 3983 / 4583 |
| s2_hostcached | | 59 / 93 | 67 / 106 | 85 / 146 | 157 / 239 | 470 / 606 | 1806 / 2134 |
| s2_coherent | | 117 / 152 | 991 / 1168 | 3810 / 4296 | 15063 / 16251 | 60172 / 64126 | 239800 / 254593 |

- g2c / c2g p50: empty 32.3 / 28.3; s2_direct 1 MiB 32.5 / 28.6. Modes: g2c 30–35 µs (81 %); c2g
  25–30 µs (48 %), 35–40 µs (29 %). Raw copy 12.55 / 13.17 GB/s.
- Run-to-run variation: **PASS** — 100 cells, none ≥ 10 %; worst 9.6 %, primary condition 6.9 %.
- Not measured: `s2_rebar` with ReBAR off (gap).

### 4.4 ReBAR on vs off — the one real difference

Everything except `s2_direct` is the same in both conditions within a few percent (empty 60 / 60,
s1_copy 1 MiB 312 / 312, s2_hostcached 1 MiB 160 / 157, s2_coherent 1 MiB 14960 / 15063 µs).

`s2_direct` is **slower with ReBAR on** from 64K upward: 1 MiB 207 vs 142 µs, 16 MiB 2395 vs
1392 µs (4K equal). The per-phase columns show why. With ReBAR on, the default S2 placement puts the
CPU→GPU buffer in `DEVICE_LOCAL|HOST_VISIBLE` memory as designed (DECISIONS #17, docs/05 §4.2;
with ReBAR off it falls back to `HOST_VISIBLE|HOST_COHERENT` system memory), and

- the CPU's write into that buffer runs at ≈ 11 GB/s across PCIe instead of ≈ 32 GB/s into system
  memory: `cpu_write` 1 MiB **96 vs 33 µs**, 16 MiB 1540 vs 514 µs. `rt` = g2c + cpu_read +
  cpu_write + c2g, and g2c / c2g / cpu_read are identical in both conditions, so this is the whole
  difference;
- in exchange the GPU's read of the request, which `rt` does **not** include, becomes almost free
  because it reads local VRAM: `gpu_read` 1 MiB **7 vs 86 µs**, 16 MiB 49 vs 1285 µs.

Round trip plus GPU read pass is therefore slightly *lower* with ReBAR on from 256K up (1 MiB 214 vs
228 µs, 16 MiB 2444 vs 2677 µs), while the hand-off latency alone is higher. Which of the two matters
for the engine is a design question for the Mac side, not something the PC data decides. On a gen4
slot both the write and the DMA read would be about twice as fast; this board is gen3.

### 4.5 Anything odd

1. **Clock drift between GPU timestamps and QPC (≈ 32 ppm)** biased the g2c/c2g split of the first
   series (`rt` unaffected); found, fixed (#30), the ReBAR-off series was repeated with the fixed
   binary and `rt` agreed within a few percent everywhere. Metal shares one clock, so the Mac data
   is not expected to be affected (check recipe in #30).
2. **`s2_rebar` is 23 h per run** on this PC at protocol iteration counts (the human chose to keep
   the full protocol). The runs were therefore made interruptible (#28), although no interruption
   occurred.
3. **One run was spoiled by the agent and redone.** In the first run 1 of the ReBAR-on series two
   `s2_hostcached` perpass/spin cells (1M, 4M) came out 17.6 % / 11.8 % high because a git commit
   and push ran on the PC while they were measured; the pooled variation check failed. Human
   decision: redo run 1 (2026-10-12/13, nothing else running). The old run is kept in
   `results/m1_3060_run1_disturbed` and is not pooled. Lesson recorded in `PC_PROGRESS.md`.
4. **Report tool double-counted gzipped runs** (#31): found and fixed at hand-over time; the
   committed reports were rebuilt and verified against a build from the `.csv.gz` files alone.
5. **The slot is PCIe gen3**, not gen4 as protocol §2 lists for P-3060; `sysinfo.pcie_link` records
   it in every header. Raw copy 12.4 / 13.3 GB/s is consistent with gen3 x16.
6. `report.md` prints `?` in the run list where the Mac has the power source: the PC headers
   have no `sysinfo.power_source` (desktop on mains; the power plan is recorded instead) and no
   thermal state. Cosmetic, not changed.
7. Background processes that could not be closed (MSI Center services, Teams, OneDrive sync,
   Voicemod) were present but idle during all series; CPU load 0–3 %, GPU 0–1 % before each start.

## 5. Comparison with the Mac (facts only; Mac numbers from `docs/STATUS.md`)

| | Mac M5 (Metal, AC re-run, 3 runs) | PC 3060 Ti (Vulkan, ReBAR on, 3 runs) |
|---|---|---|
| Empty round trip p50 / p99 | ≈ 214 / 316 µs (floor p10 ≈ 80 µs) | **60 / 85 µs** |
| g2c p50 | ≈ 32–40 µs | 31 µs |
| c2g p50 | bimodal/multimodal: ≈ 45–50 µs fast path (35–45 % of iterations), 155 µs, 235 µs | 29 µs; two close modes 25–30 and 35–40 µs, no slow mode |
| s2_direct 1 MiB rt p50 | ≈ 305 µs | 207 µs (142 µs with ReBAR off) |
| s2_direct 16 MiB rt p50 | ≈ 842 µs | 2395 µs (1392 µs with ReBAR off) — discrete GPU, PCIe gen3 |
| s1_copy 1 MiB / 16 MiB | ≈ 352 / 1587 µs | 312 / 3940 µs |
| Raw copy 16 MiB | ≈ 55–58 GB/s (unified memory) | 12.4 / 13.3 GB/s (PCIe gen3 x16) |
| Run-to-run (≥ 10 % cells) | 26 / 52 → FAIL | 0 / 124 → PASS |

In one sentence: on the PC the sync hand-off itself is short and stable in both directions (≈ 30 µs
each way, no slow mode), and the cost that grows with payload is the PCIe transfer; on the Mac the
transfer is nearly free and the hand-off (CPU→GPU resume) dominates and is multimodal.

## 6. Where everything is

- `results/m1_pc_report/` — the §4 combined PC report (3060 Ti, ReBAR on).
- `results/m1_3060/` — primary series: `micro_P-3060_rebar-on_run{1,2,3}.csv.gz`, `.log`, `report/`,
  `run_microbench.out` (includes the replaced run 1's lines).
- `results/m1_3060_rebar-off/`, `results/m1_3060_rebar-off_old-calibration/` — as in 4.1.
- `results/m1_3060_run1_disturbed/` — the replaced run, with a README.
- `results/validation_check.csv.gz`, `.log`, `m0_play_validation.log`, `m0_bench_stub.log` — M0.
- `docs/PC_PROGRESS.md` — the full session log, human decisions, resume procedure.
- Raw `.csv` files are not committed (gzipped copies are byte-identical sources for `micro_report.py`).
