# PC progress (resume here after a reboot / GPU swap)

Claude Code session log for `docs/PC_CLAUDE_TASK.md` on the Windows 11 PC. Newest entry at the
bottom of each section. Clone: `C:\dev\Lightbound`, branch `pc/m0-m1`.

_Last updated 2026-10-14 (idle; hand-over 10-13; sessions on 2026-10-01/02, 10-05, 10-06 … 10-14)._

## Where we are

- [x] §−1 Clone + branch `pc/m0-m1` created, git identity set locally.
- [x] §0 Docs read (CLAUDE.md, STATUS.md, DECISIONS.md #11–#26, 07_MILESTONES.md, protocol §2/§6/§12).
- [x] §1 Environment check done; human decisions taken; Vulkan SDK 1.4.363.0 installed.
- [x] §2 M0 [PC] checks — **ALL PASS** (see "§2 M0 [PC] checks").
- [x] §3 step A — RTX 3060 Ti, **ReBAR OFF**, 5 fast paths, 3 runs (done 2026-10-02 with binary
      `6a2d02e`; `rt` valid, g2c/c2g biased by clock drift) → kept as
      `results\m1_3060_rebar-off_old-calibration`.
- [x] Runs are **interruptible and resumable** (DECISIONS #28) — see "If the PC has to be switched off".
- [x] Clock-drift fix (human decision 2026-10-05, DECISIONS #30).
- [x] §3 step A2 — step A repeated with the fixed binary `6962eff` (human decision 2026-10-05),
      ReBAR off → `results\m1_3060_rebar-off`, tag `rebar-off` (done 2026-10-05, **these are the
      ReBAR-off numbers to use**, see "Step A2 results").
- [x] §3 step B — human enabled Resizable BAR in the BIOS (2026-10-05; probe says
      `sysinfo.rebar: on`, `bar_heap_mb: 8024`).
- [x] §3 step C — RTX 3060 Ti, ReBAR on, full protocol incl. `s2_rebar` → `results\m1_3060`
      — **DONE 2026-10-13**: 3 clean runs, run-to-run **PASS**, report built, gzipped and
      committed (see "Step C results"). Run 1 was redone on 10-12/13 after the first one had
      been disturbed by Claude's git activity (old run kept in `results\m1_3060_run1_disturbed\`).
- [x] §3 step D — card 2: RTX 4060 Ti — **CANCELLED 2026-10-13: the human has no 4060 Ti.**
      `results\m1_4060` does not exist; the PC side covers the 3060 Ti only.
- [x] §4 Report — **DONE 2026-10-13**: `results\m1_pc_report` (3060 Ti ReBAR on; the
      ReBAR-off series has its own report), `docs/PC_REPORT.md`, everything gzipped and pushed.

## If the PC has to be switched off (or anything interrupts a series)

Nothing is lost except the cell that was being measured (worst case one 16 MiB `s2_rebar` cell,
≈ 4.3 h). Just shut down. To continue afterwards:

1. Boot, close the background apps (Chrome, Edge, Teams, Epic, OneDrive, Voicemod, MSI Center, …).
2. Open Claude Code in `C:\dev\Lightbound` and say *Continue docs/PC_CLAUDE_TASK.md* — or start the
   series yourself with the **same command plus `--resume`** from a terminal in `C:\dev\Lightbound`:
   ```
   python tools\run_microbench.py --exe build\lightbound.exe --out results\m1_3060 --tag rebar-on --resume
   ```
   (`results\m1_4060` for the 4060 Ti.) Finished runs are skipped; the interrupted run continues
   from its last complete cell; the report is built when all three runs are done.

Rules for the next session:
- **Always pass `--resume`** for the M1 series (it also works for a fresh start). Without it the
  runner refuses to start when `_running_run*.csv` exists, and would overwrite finished runs.
- Before starting, check that no `lightbound.exe` is still running (`Get-Process lightbound`).
- **Do not rebuild between the parts of a run**: `--resume` refuses to continue when the binary's
  `git_commit`, the options, the GPU, the driver, ReBAR or the power plan differ from the
  interrupted run's CSV header (delete `_running_run<i>.csv/.log` to start that run over).
- State on disk: `micro_*_run<i>.csv` = finished run; `_running_run<i>.csv/.log` = interrupted run;
  `run_microbench.out` = series progress. Each restart adds a `# resumed:` line to the CSV.
- Launch detached so the series survives the Claude Code session (PowerShell):
  ```
  Start-Process cmd.exe -WorkingDirectory C:\dev\Lightbound -WindowStyle Hidden -ArgumentList '/c',
    'python tools\run_microbench.py --exe build\lightbound.exe --out results\m1_3060 --tag rebar-on --resume >> results\m1_3060\run_microbench.out 2>> results\m1_3060\run_microbench.err'
  ```
  (create `results\m1_3060` first). Sleep and hibernate timeouts are "never" on AC (checked).

## Human decisions (session 1, 2026-10-02)

1. **Vulkan SDK was not installed** (hard blocker: `cmake/LbDependencies.cmake` aborts with
   "Vulkan SDK not found" on Windows, and the M0 checks need `VK_LAYER_KHRONOS_validation`).
   Human approved installing it: `winget install --id KhronosGroup.VulkanSDK -e` (1.4.363.0).
2. **Power plan** was "Balanced"; the human chose **Ultimate Performance**
   (`powercfg /setactive 601bada7-b936-4131-9cc9-83e4d49842c6`) — active now.
3. NVIDIA Control Panel → Manage 3D settings → Power management mode = Prefer maximum performance —
   **confirmed by the human** (cannot be checked from the shell).
4. Note: the GitHub repository is **public** (the task file says private); the clone needed no
   credentials. Pushing needed a one-time Git Credential Manager browser sign-in, which worked.

A first `cmake -B build` (before the SDK) already fetched all dependencies (SDL3 3.4.16, doctest,
miniaudio, Slang 2026.18.2 Windows zip, Vulkan-Headers, VMA) and detected MSVC 19.44.35225; it
stopped only at the Vulkan SDK check, as expected.

## §1 Environment check (2026-10-01)

| What | Result | Needed | OK? |
|---|---|---|---|
| GPU / driver | NVIDIA GeForce RTX 3060 Ti, driver 566.36 | 3060 Ti or 4060 Ti | yes |
| PCIe link (nvidia-smi) | `pcie.link.gen.max` = **3**, `width.max` = 16 (idle: gen1 x16) | protocol assumes gen4 x16 | **note** — the system caps the link at gen3; the measured link in the CSV header will say so. Flagged to the human. |
| Vulkan SDK | `VULKAN_SDK` unset; no `C:\VulkanSDK`; `vulkaninfo` is only the driver's copy in `system32` (instance 1.3.301); layers present: EOS overlay, NV optimus, OBS hook — **no Khronos validation layer** | SDK ≥ 1.3 with validation layer | **MISSING** |
| Visual Studio 2022 C++ | Build Tools 2022 17.14.37111 at `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools`, MSVC 14.44.35207, Windows SDK 10.0.26100 (`vswhere -latest` alone finds nothing — needs `-products *`) | found | yes |
| CMake / Ninja | not on PATH; bundled with Build Tools: CMake 3.31.6, Ninja 1.12.1 (available after `vcvars64.bat`) | ≥ 3.25 / present | yes (via vcvars) |
| Python | 3.13.14 (Microsoft Store build, `python` on PATH) | ≥ 3.9 | yes |
| Python packages | were missing; installed with `python -m pip install --user`: pandas 3.0.6, matplotlib 3.11.2, scipy 1.18.1 | import OK | yes |
| Power plan | **Balanced** (High performance and Ultimate Performance schemes exist) | High performance | **no** — asked |
| Git | 2.53.0.windows.1; clone worked without a sign-in prompt | present | yes |

### System info for the report (protocol §2)
- CPU: 12th Gen Intel Core i7-12700F (12 cores / 20 threads)
- Board: Micro-Star International MS-B9241 (OEM), BIOS 8.00 (2022-01-26)
- RAM: 32 GB
- OS: Windows 11 Home Single Language 10.0.26200
- Disk free on C:: ~536 GB

### Build environment (for the next session)
```
cmd /c "call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul && cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DLB_BACKEND=vulkan"
```
After the Vulkan SDK is installed, the shell that Claude Code was started in will not have
`VULKAN_SDK`; set it from the machine environment first
(`$env:VULKAN_SDK = [Environment]::GetEnvironmentVariable('VULKAN_SDK','Machine')`) or restart
the session.

## §2 M0 [PC] checks (2026-10-02) — ALL PASS

| # | Check | Result |
|---|---|---|
| 2.1 | Configure + build Release/vulkan, MSVC `/W4 /WX` | **PASS.** One compile error, Windows-only: `alignUp` `usize`/`u64` overloads collide on MSVC x64 (`size_t` == `uint64_t`), hit only in the test TUs. Fixed with one constrained template (DECISIONS #27). Zero compiler and linker warnings after the fix; the Vulkan backend itself compiled clean on the first try. |
| 2.2 | `ctest --test-dir build --output-on-failure` | **PASS 36/36**, incl. `microbench_smoke` (17.4 s, validation on). |
| 2.3 | `--mode=play --validation=on --exit-after=300` | **PASS.** Exit 0; log has `Vulkan validation layer enabled (VK_LAYER_KHRONOS_validation)`, no `ERROR [vk]`, `backend errors: 0`; 300 frames in 4.98 s (vsync on). Log: `results\m0_play_validation.log`. |
| 2.4 | `--mode=bench` | **PASS.** Prints `mode 'bench' is not implemented yet (milestone M4)`, exit 2. |
| 2.5 | `--mode=microbench --validation=on --frames=30 --warmup=5` | **PASS.** Exit 0, `0 verification/R7 failures, 0 API errors`, **0** `ERROR [vk]`, **0** `WARN [vk]`. Took 1371 s (23 min — see the `s2_rebar` note below). Files: `results\validation_check.csv/.log`. |

Facts from the validation run's sysinfo header (also what the M1 probe would report):
- `sysinfo.rebar: off`, `sysinfo.bar_heap_mb: 214` → **ReBAR is OFF** on this PC (BIOS default).
- `sysinfo.pcie_link: gen3 x16 (max gen3 x16)` measured under load (board/CPU slot limit, not gen4).
- `sysinfo.power_plan: Ultimate Performance`; `clock_domain: QueryPerformanceCounter`; GPU timestamps yes.
- Raw copy 16 MiB: d2h 12.4 GB/s, h2d 13.2 GB/s (gen3 x16 practical maximum ≈ 12–13 GB/s).
- With validation ON: empty round trip p50 ≈ 76 µs (g2c ≈ 45, c2g ≈ 31). Timing runs will be lower.
- **`s2_rebar` is extremely slow by design** (CPU reads uncached BAR memory word by word): per
  iteration p50 ≈ 1.8 ms (4K), 27 ms (64K), 110 ms (256K), 0.44 s (1M), 1.77 s (4M), **7.1 s (16M)**.
  At protocol iteration counts (200 + 2000) × 4 submit/wait cells that is ≈ **23 h per run** for this
  one path. `s2_coherent` (write-combined system memory) is ≈ 49 min per run; all other paths
  together ≈ 2–3 min. → A full protocol run ≈ 24 h; three runs ≈ 3 days. Not feasible as specified.

## §3 M1 [PC] — card 1: RTX 3060 Ti, ReBAR OFF

Decisions taken by Claude (documented, reversible):
- Tag `rebar-off`, directory `results\m1_3060_rebar-off` (the task's `m1_3060 --tag rebar-on` assumed
  ReBAR on). ReBAR-on runs, if the human enables it in the BIOS, go to `results\m1_3060` as planned.
- The main series runs **without `s2_rebar`** (`--micro-paths=empty,s1_copy,s2_direct,s2_hostcached,s2_coherent`,
  everything else at protocol defaults: 200 + 2000 iterations, all submit × wait modes, all payloads).
  Each cell is measured independently (own warm-up and calibration), so this does not change what is
  measured for the included paths. `s2_rebar` is to be run as separate process runs into the same
  directory once the human picks an option (see "Open questions"). Estimated ≈ 55 min per run.
- Background apps at launch (not closed — needs the human): Chrome, Edge, Teams, Epic Games Launcher
  (+ EOS overlay renderer), OneDrive, Voicemod, MSI Center, ChatGPT desktop, Task Manager. GPU 0 % util.

**Human decisions (2026-10-02, while the rebar-off series was running):**
1. `s2_rebar`: **full protocol** (200 + 2000 iterations, all modes, all payloads) — ≈ 23 h per run.
2. **Enable Resizable BAR in the BIOS** and run the cards as `rebar-on` (primary condition).
3. Background apps closed by the human at 10:2x (after the series had started; they will close
   them again after every restart).

### Step A results (2026-10-02, 10:13–12:50; checked 2026-10-05)

Session 1 ended a few minutes before the series finished; checked in session 2:
- 3 runs, 3186 / 3083 / 3081 s; each log ends with `0 verification/R7 failures, 0 API errors`, exit 0.
- Header: `sysinfo.pcie_link: gen3 x16 (max gen3 x16)`, `power_plan: Ultimate Performance`,
  `rebar: off`, binary `6a2d02e` (clean).
- `report\report.md` was empty: `micro_report.py` crashed on Windows writing "→" with cp1252
  (fixed, DECISIONS #29); regenerated 2026-10-05.
- Round trip p50 / p99 (µs), chain/spin, 3 runs pooled:

  | path | 0 | 4K | 64K | 256K | 1M | 4M | 16M |
  |---|---:|---:|---:|---:|---:|---:|---:|
  | empty | 61 / 109 | | | | | | |
  | s2_direct | | 61 / 102 | 67 / 100 | 82 / 129 | 142 / 198 | 408 / 537 | 1411 / 1688 |
  | s1_copy | | 75 / 124 | 92 / 128 | 134 / 197 | 314 / 420 | 1103 / 1318 | 4014 / 4590 |
  | s2_hostcached | | 61 / 101 | 67 / 107 | 85 / 134 | 157 / 241 | 468 / 665 | 1768 / 2212 |
  | s2_coherent | | 118 / 166 | 991 / 1253 | 3801 / 4392 | 15085 / 16457 | 60376 / 64769 | 240359 / 259409 |

- g2c / c2g p50 (µs), chain/spin: empty 32.0 / 29.8; s2_direct 1 MiB 28.6 / 33.7; s1_copy 1 MiB
  113.5 / 119.8 (see the drift caveat below: the split is biased by a few µs at 1 MiB and badly
  at larger payloads; `rt` is not affected).
- Raw copy 16 MiB: d2h 12.27 GB/s, h2d 13.17 GB/s.
- Run-to-run variation: **PASS** — 100 cells, none ≥ 10 %; worst 9.3 % (s2_direct perpass/block
  16M), worst in the primary condition 8.4 %.
- Run 1 had the background apps open for its first ≈ 10 minutes (closed by the human at 10:2x).

### Finding (2026-10-05): GPU↔CPU clock drift between recalibrations — fixed (DECISIONS #30)

Found while checking step A: `g2c` falls and `c2g` rises linearly inside every 500-iteration
calibration block, by equal amounts, and jump back at each recalibration (iterations 300, 800,
1300, 1800). The GPU timestamp clock and QPC drift apart by ≈ 32 ppm, and the microbench
recalibrated by iteration count, not by elapsed time (DECISIONS #23). Consequences in step A:
- `rt` (GPU clock only) and the sum `g2c + c2g` are **not** affected.
- The g2c/c2g split is off by half the drift accumulated over a block: ≈ 1 µs for `empty`,
  ≈ 3 µs at 1 MiB (s2_direct: 32 → 26 µs across a block), tens of µs at 16 MiB, milliseconds for
  `s2_coherent` (16M: g2c p50 = −1.8 ms), and it would be ≈ 100 ms for `s2_rebar` 16M.
- 26 of 100 cells have negative g2c samples.

**Human decisions (2026-10-05):** (1) fix it now; (2) repeat the ReBAR-off series with the fixed
binary before the BIOS change. Fix: the mapping is refreshed when older than 20 ms, and c2g of a
long iteration uses a calibration taken at its end. Checked against the old schedule with three
interleaved runs each (background apps open, so indicative): `rt` p50 within ±1.3 % in every
cell, block drift gone (2.1 → 0.2 µs for `empty`, 74 → 0 µs at 16 MiB), no negative samples.
Slow paths now give plausible splits (e.g. `s2_rebar` 4M at 1.75 s per iteration: g2c p50 64 µs,
c2g p50 177 µs; c2g is ≈ 150–180 µs after second-long CPU phases, 30–35 µs on the fast paths).

### Step A2 results (2026-10-05, 10:26–13:03) — RTX 3060 Ti, ReBAR off, fixed clock mapping

- 3 runs, 3185 / 3078 / 3085 s; every log has `0 verification/R7 failures, 0 API errors`, exit 0,
  no `ERROR`/`WARN` lines, no restarts. Binary `6962eff` (clean).
- Header: `sysinfo.pcie_link: gen3 x16 (max gen3 x16)`, `power_plan: Ultimate Performance`,
  `rebar: off`, `bar_heap_mb: 214`.
- Background apps closed by the human before the start; still present at launch: MSI Center
  services, `ms-teams`, `OneDrive.Sync.Service`, `Voicemod` (CPU load 2 %, GPU 0 %).
- Round trip p50 / p99 (µs), chain/spin, 3 runs pooled:

  | path | 0 | 4K | 64K | 256K | 1M | 4M | 16M |
  |---|---:|---:|---:|---:|---:|---:|---:|
  | empty | 60 / 99 | | | | | | |
  | s2_direct | | 59 / 88 | 65 / 89 | 80 / 114 | 142 / 212 | 388 / 530 | 1392 / 1684 |
  | s1_copy | | 74 / 120 | 90 / 129 | 131 / 178 | 312 / 408 | 1074 / 1298 | 3983 / 4583 |
  | s2_hostcached | | 59 / 93 | 67 / 106 | 85 / 146 | 157 / 239 | 470 / 606 | 1806 / 2134 |
  | s2_coherent | | 117 / 152 | 991 / 1168 | 3810 / 4296 | 15063 / 16251 | 60172 / 64126 | 239800 / 254593 |

- g2c / c2g p50 (µs), chain/spin: empty 32.3 / 28.3; s2_direct 1 MiB 32.5 / 28.6, 16 MiB
  45.2 / 32.5; s1_copy 1 MiB 116.3 / 114.8; s2_hostcached 1 MiB 30.9 / 29.8; s2_coherent 1 MiB
  36.2 / 55.9, 16 MiB 59.2 / 82.5.
- Empty round trip modes: g2c 30–35 µs (81 %); c2g 25–30 µs (48 %) and 35–40 µs (29 %).
- Raw copy 16 MiB: d2h 12.55 GB/s, h2d 13.17 GB/s.
- Run-to-run variation: **PASS** — 100 cells, none ≥ 10 %; worst 9.6 % (s2_direct perpass/spin
  16M), worst in the primary condition 6.9 %.
- Clock drift check: no trend of g2c/c2g inside the former calibration blocks, **0 of 100 cells
  with a negative g2c sample** (26 of 100 in step A).
- `rt` agrees with step A (old calibration, 2026-10-02) within a few percent in every primary
  cell (e.g. empty 61 → 60, s2_direct 1 MiB 142 → 142, 16 MiB 1411 → 1392 µs).

### Step C (started 2026-10-05 14:34) — RTX 3060 Ti, ReBAR on, full protocol — RUNNING

Checks before the start (session 3, right after the BIOS change; boot at 14:27):
- Probe: `sysinfo.rebar: on`, `bar_heap_mb: 8024` (was `off` / 214), `pcie_link: gen3 x16 (max
  gen3 x16)`, `power_plan: Ultimate Performance`, driver 566.36, binary `6962eff` (not rebuilt).
- `s2_rebar` works with ReBAR on: a 4 + 1 iteration pass over all payloads and modes took 187 s,
  `0 verification/R7 failures, 0 API errors`. Per iteration p50 is the same as with ReBAR off:
  1.7 ms (4K), 27 ms (64K), 108 ms (256K), 0.44 s (1M), 1.75 s (4M), 7.0 s (16M) → ≈ 22.8 h per
  run for this path, ≈ 23.7 h per run in total.
- Windows Update paused until 2026-10-09 14:20 local time, no reboot pending; sleep and hibernate
  "never" on AC; 493 GB free; CPU load 0–3 %, GPU 1 %; no `lightbound.exe` running.
- Still present at launch (same as step A2): MSI Center services, `ms-teams`,
  `OneDrive.Sync.Service`, `Voicemod` — all idle in a 10 s sample.

Launched detached at 14:34:45 with the `--resume` command above (tag `rebar-on`). First 15 cells
of run 1 agree with step A2 (`s1_copy` chain/spin 1 MiB 313 µs, 16 MiB 4067 µs; A2: 312 / 3983),
no `ERROR`/`WARN` lines; CSV header: 2000 + 200 iterations, `rebar: on`, `6962eff`.
Expected: run 1 done ≈ 2026-10-06 14:15, run 2 ≈ 10-07 14:00, run 3 and the report ≈ 10-08 13:45,
about one day before the Windows Update pause ends. If the series is interrupted and resumed
later, extend the pause first.

Early observation from run 1 (not a failure; to be confirmed by the three pooled runs): with
ReBAR on, `s2_direct` and `empty` put the CPU→GPU buffer in ReBAR memory
(`up=DeviceLocalHostVisible` instead of `HostVisibleCoherent`, as designed, DECISIONS #17), and
the `s2_direct` round trip is slower than with ReBAR off from 64K up. Chain/spin rt p50, 4K … 16M:
61 / 72 / 98 / 208 / 678 / 2506 µs, against 60 / 65 / 81 / 142 / 411 / 1428 µs in step A2 run 1.
Per-phase medians from the CSV (checked 2026-10-06 while run 1 was still going) show where the
time goes: g2c, c2g and `cpu_read_us` are unchanged (1 MiB: 32 / 29 / 50 µs in both), but
`cpu_write_us` into the ReBAR buffer is ≈ 3× slower (1 MiB 96 vs 33 µs, 16 MiB 1541 vs 523 µs,
i.e. ≈ 11 GB/s write-combined over PCIe vs ≈ 32 GB/s into system memory), and `rt` is the sum
g2c + cpu_read + cpu_write + c2g. In exchange the GPU's read pass, which `rt` does not include,
becomes almost free because it reads local VRAM: `gpu_read_us` 1 MiB 7 vs 86 µs, 16 MiB 49 vs
1288 µs. Round trip plus GPU read is therefore slightly *lower* with ReBAR on from 256K up (1 MiB
215 vs 228 µs, 16 MiB 2555 vs 2716 µs). Which of the two matters is for the report / the human.
Unexplained so far (single run): `gpu_write_us` into system memory is +66 / +107 µs at 4M / 16M
and g2c at 16M is 125 vs 54 µs with ReBAR on.

Run 1 finished 2026-10-06 14:18 after **85401 s (23.7 h)**, exit 0, `0 verification/R7 failures,
0 API errors`, no `ERROR`/`WARN` → `micro_P-3060_rebar-on_run1.csv`. Run 2 started 14:18 (expected
end ≈ 10-07 14:00; run 3 ≈ 10-08 13:45). Run 2's fast-path cells repeat run 1 within 1–4 % (1 MiB
chain/spin: `s1_copy` 310 vs 313, `s2_direct` 207 vs 208, `s2_hostcached` 159 vs 165 µs); its p99s
are lower (`empty` 73 vs 107 µs), consistent with run 1's first minutes having been disturbed.

Run 2 finished 2026-10-07 14:04 after **85550 s (23.8 h)**, exit 0, clean log →
`micro_P-3060_rebar-on_run2.csv`. Run 3 started 14:04 (expected end ≈ 10-08 13:50, then the
report is built). Key cells agree across runs 1 / 2 / 3 within 1–4 % (1 MiB chain/spin:
`s2_direct` 208 / 207 / 207, `s2_hostcached` 165 / 159 / 159, `s2_coherent` 15008 / 14935 / – µs;
`s2_rebar` 16 MiB 7.016 / 7.034 / – s). Human extended the Windows Update pause on 10-07 to
**2026-10-16 14:28** local time, which covers step D as well.

### Step C results (final, 2026-10-13) — RTX 3060 Ti, ReBAR on, full protocol

History: the first run 1 (10-05 → 10-06) had two secondary cells (`s2_hostcached` perpass/spin
1M and 4M) spoiled by 17.6 % / 11.8 % because a progress-log commit + push ran on the PC at
≈ 14:37:40 while they were being measured; the pooled run-to-run check therefore said FAIL
(primary condition passed at 8.5 %). Human decision 2026-10-12: replace run 1. The old run is
kept in `results\m1_3060_run1_disturbed\` (README there), the replacement ran 2026-10-12 10:45 →
10-13 10:30 with nothing else happening on the PC, and the runner rebuilt the report.

- 3 runs, 85532 / 85550 / 85545 s (23.8 h each): run 1 on 10-12/13, runs 2 and 3 on 10-06/07
  and 10-07/08. Every log ends with `0 verification/R7 failures, 0 API errors`, exit 0, no
  `ERROR`/`WARN` lines, no restarts. Binary `6962eff`, no reboot since 10-05.
- Header: `sysinfo.rebar: on`, `bar_heap_mb: 8024`, `pcie_link: gen3 x16 (max gen3 x16)`,
  `power_plan: Ultimate Performance`, driver 566.36.
- Report bug found and fixed at hand-over time (DECISIONS #31): `micro_report.py` pooled runs 2
  and 3 twice because their `.csv` and `.csv.gz` were both present (`n` = 10000 per cell). The
  report below is the corrected one (`n` = 6000), identical to a build from the `.csv.gz` alone.
- Round trip p50 / p99 (µs), chain/spin, 3 runs pooled (`report\report.md`):

  | path | 0 | 4K | 64K | 256K | 1M | 4M | 16M |
  |---|---:|---:|---:|---:|---:|---:|---:|
  | empty | 60 / 85 | | | | | | |
  | s2_direct | | 60 / 80 | 69 / 103 | 97 / 134 | 207 / 278 | 648 / 810 | 2395 / 2650 |
  | s1_copy | | 73 / 105 | 89 / 119 | 131 / 179 | 312 / 412 | 1053 / 1296 | 3940 / 4437 |
  | s2_hostcached | | 62 / 94 | 69 / 102 | 87 / 123 | 160 / 230 | 466 / 612 | 1759 / 2343 |
  | s2_rebar | | 1748 / 1853 | 27164 / 27964 | 108717 / 110729 | 436664 / 442832 | 1757962 / 1769825 | 7031791 / 7063888 |
  | s2_coherent | | 119 / 145 | 991 / 1211 | 3813 / 4365 | 14960 / 16343 | 59824 / 65164 | 239149 / 260482 |

- g2c / c2g p50 (µs), chain/spin: empty 31.0 / 28.9; s2_direct 1 MiB 33.0 / 29.2, 16 MiB
  46.0 / 33.5; s1_copy 1 MiB 115.6 / 115.0; s2_hostcached 1 MiB 33.0 / 29.9; s2_coherent 1 MiB
  35.0 / 59.7; s2_rebar 1 MiB 41.1 / 188.8, 16 MiB 42.9 / 198.7.
- Empty round trip modes: g2c 30–35 µs (73 %); c2g 25–30 µs (48 %) and 35–40 µs (40 %).
- Raw copy 16 MiB: d2h 12.38 GB/s, h2d 13.27 GB/s.
- **Run-to-run variation: PASS** — 124 cells, none ≥ 10 %; worst 8.4 % (s2_hostcached
  perpass/block 16M), worst in the primary condition 7.4 %.
- ReBAR on vs off, `s2_direct` pooled (chain/spin): 1 MiB rt 207 vs 142 µs, `cpu_write` 96 vs
  33 µs, `gpu_read` 7 vs 86 µs; 16 MiB rt 2395 vs 1392 µs, `cpu_write` 1540 vs 514 µs,
  `gpu_read` 49 vs 1285 µs; g2c / c2g unchanged. The per-phase analysis above holds for the
  pooled data: the CPU write into the ReBAR buffer is ≈ 3× slower, the GPU read pass (outside
  `rt`) becomes almost free.
- `empty`, `s1_copy`, `s2_hostcached`, `s2_coherent` and `s2_rebar` agree with the ReBAR-off
  series within a few percent (e.g. empty 60 / 60, s1_copy 1 MiB 312 / 312, s2_hostcached 1 MiB
  160 / 157, s2_coherent 1 MiB 14960 / 15063 µs), so ReBAR only changes `s2_direct`, as expected.

Lesson (applies to every future series): **no git commit/push, no log reads, nothing at all on
the PC during the first ≈ 4 minutes of each run** (the fast paths) and during `s2_coherent`
(the last ≈ 50 min). Record findings in the log only while `s2_rebar` is running (cells of
seconds; shown to be insensitive: runs 1–3 agree within 0.3 % there).
`s1_copy` and `s2_hostcached` are close to A2 (1 MiB chain/spin 313 and 165 µs; A2 pooled 312 and
157); the probe's `empty` chain/spin was 60 µs (A2: 60).

Note: two progress-log commits with a push fell into run 1's fast-path cells (≈ 14:36:20 in
`s2_direct` chain/block, ≈ 14:37:40 in `s2_hostcached` perpass/spin), and the log was read several
times during run 1's first three minutes. Runs 2 and 3 are undisturbed; the run-to-run check will
show whether it mattered.

### Revised plan
| Step | What | Where | Est. |
|---|---|---|---|
| A (done) | 3060 Ti, ReBAR off, 5 fast paths, 3 runs, old calibration | `results\m1_3060_rebar-off_old-calibration` | took 2 h 37 min |
| A2 (done) | same as A with the fixed binary (`--micro-paths=empty,s1_copy,s2_direct,s2_hostcached,s2_coherent`) | `results\m1_3060_rebar-off` | took 2 h 37 min |
| B (done) | Human: reboot → BIOS → Above 4G Decoding + Re-Size BAR on → boot → close apps → new session "Continue docs/PC_CLAUDE_TASK.md" | — | — |
| C (done) | Verify ReBAR on (probe: `sysinfo.rebar: on`, `bar_heap_mb` > 256). 3060 Ti **full protocol incl. `s2_rebar`**, 3 runs | `results\m1_3060` tag `rebar-on` | took 2 d 23 h 16 min |
| D (cancelled) | GPU swap → 4060 Ti, same as C | — (no 4060 Ti available, 2026-10-13) | — |
| E (done) | §4 reports + push | `results\m1_pc_report` (+ `m1_3060_rebar-off` separately) | 2026-10-13 |

Not planned (gap noted in the report): `s2_rebar` with ReBAR off; no second card.
Risk for the multi-day runs: Windows Update auto-restart — the human should pause updates.

## Next steps
The PC task is complete (2026-10-13). Everything is on branch `pc/m0-m1`; the Mac continues
from `docs/PC_REPORT.md` (the human was given the Mac prompt on 2026-10-14: *Merge pc/m0-m1 and
continue from docs/PC_REPORT.md*). If a second card ever becomes available: boot with it, probe,
run `results\m1_<card>` with `--tag rebar-on --resume`, then rebuild `results\m1_pc_report` from
both directories (`micro_report.py results\m1_3060 results\m1_<card> --out results\m1_pc_report`)
and extend `PC_REPORT.md`. Nothing on the PC during the first 4 minutes of each run.

**State on 2026-10-14 (session paused):** PC idle — no `lightbound.exe`/`python` running, working
tree clean, `pc/m0-m1` even with origin at the hand-over commit. The PC session may be resumed
later if the Mac side asks for additional measurements (e.g. a re-run after a Mac-side code
change, an extra path/payload, or a ReBAR-off `s2_rebar` cross-check). In that case: start from
"If the PC has to be switched off" above, **rebuild first if `src/` changed** (the `--resume`
header check refuses a different `git_commit`), use a **new output directory** for a new series
(never add runs to `results\m1_3060`, its three runs are final), and keep the 4-minute rule.
