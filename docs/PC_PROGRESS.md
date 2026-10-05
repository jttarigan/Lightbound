# PC progress (resume here after a reboot / GPU swap)

Claude Code session log for `docs/PC_CLAUDE_TASK.md` on the Windows 11 PC. Newest entry at the
bottom of each section. Clone: `C:\dev\Lightbound`, branch `pc/m0-m1`.

_Last updated 2026-10-05 (session 2; session 1 was 2026-10-01/02)._

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
- [ ] §3 step A2 — **repeat step A with the fixed binary** (human decision 2026-10-05), ReBAR still
      off → `results\m1_3060_rebar-off`, tag `rebar-off`, ≈ 2 h 40 min.
- [ ] §3 step B — human enables Resizable BAR in the BIOS (**still OFF on 2026-10-05**: probe says
      `sysinfo.rebar: off`, `bar_heap_mb: 214`).
- [ ] §3 step C — RTX 3060 Ti, ReBAR on, full protocol incl. `s2_rebar` → `results\m1_3060`.
- [ ] §3 step D — card 2: RTX 4060 Ti (after swap) → `results\m1_4060`.
- [ ] §4 Report (`docs/PC_REPORT.md`), gzipped results, push.

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

### Revised plan
| Step | What | Where | Est. |
|---|---|---|---|
| A (done) | 3060 Ti, ReBAR off, 5 fast paths, 3 runs, old calibration | `results\m1_3060_rebar-off_old-calibration` | took 2 h 37 min |
| A2 | same as A with the fixed binary (`--micro-paths=empty,s1_copy,s2_direct,s2_hostcached,s2_coherent`) | `results\m1_3060_rebar-off` | ≈ 2 h 40 min |
| B | Human: reboot → BIOS → Above 4G Decoding + Re-Size BAR on → boot → close apps → new session "Continue docs/PC_CLAUDE_TASK.md" | — | — |
| C | Verify ReBAR on (probe: `sysinfo.rebar: on`, `bar_heap_mb` > 256). 3060 Ti **full protocol incl. `s2_rebar`**, 3 runs | `results\m1_3060` tag `rebar-on` | ≈ 24 h/run → **≈ 3 days** |
| D | GPU swap → 4060 Ti, same as C | `results\m1_4060` tag `rebar-on` | ≈ 3 days |
| E | §4 reports + push | `results\m1_pc_report` (+ `m1_3060_rebar-off` separately) | — |

Not planned (gap to note in the report): `s2_rebar` with ReBAR off, and ReBAR-off runs on the 4060 Ti.
Risk for the multi-day runs: Windows Update auto-restart — the human should pause updates.

## Next steps
1. Step A2: human closes the background apps; start (or continue) the ReBAR-off series:
   ```
   python tools\run_microbench.py --exe build\lightbound.exe --out results\m1_3060_rebar-off --tag rebar-off --resume -- --micro-paths=empty,s1_copy,s2_direct,s2_hostcached,s2_coherent
   ```
   Then check logs and report, record the numbers here, gzip, commit, push.
2. Human does step B (reboot into BIOS: Above 4G Decoding + Re-Size BAR on), pauses Windows
   Update, closes the background apps.
3. New session: run the probe (`sysinfo.rebar: on`, `bar_heap_mb` > 256), then start step C with
   the detached `--resume` command above. Check the first cells, then leave it running.
