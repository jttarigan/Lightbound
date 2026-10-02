# PC progress (resume here after a reboot / GPU swap)

Claude Code session log for `docs/PC_CLAUDE_TASK.md` on the Windows 11 PC. Newest entry at the
bottom of each section. Clone: `C:\dev\Lightbound`, branch `pc/m0-m1`.

_Last updated 2026-10-02 (session 1, started 2026-10-01)._

## Where we are

- [x] §−1 Clone + branch `pc/m0-m1` created, git identity set locally.
- [x] §0 Docs read (CLAUDE.md, STATUS.md, DECISIONS.md #11–#26, 07_MILESTONES.md, protocol §2/§6/§12).
- [x] §1 Environment check done; human decisions taken; Vulkan SDK 1.4.363.0 installed.
- [x] §2 M0 [PC] checks — **ALL PASS** (see "§2 M0 [PC] checks").
- [ ] §3 M1 [PC] timing runs — card 1: RTX 3060 Ti, **ReBAR OFF** → `results\m1_3060_rebar-off`,
      tag `rebar-off`. Main paths running; `s2_rebar` handling and ReBAR-on question open (see §3).
- [ ] §3 M1 [PC] timing runs — card 2: RTX 4060 Ti (after swap).
- [ ] §4 Report (`docs/PC_REPORT.md`), gzipped results, push.

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

Open questions for the human:
1. `s2_rebar`: (a) skip (30-iteration validation data already characterises it), (b) reduced run
   (e.g. `--micro-paths=s2_rebar --submit=chain --cpuwait=spin --frames=200 --warmup=20`, ≈ 35 min/run),
   (c) reduced iterations but all 4 modes (≈ 2.3 h/run), (d) payloads ≤ 1 MiB at full iterations
   (≈ 85 min/run), (e) full protocol (≈ 23 h/run).
2. ReBAR: enable in BIOS ("Above 4G Decoding" + "Re-Size BAR Support", UEFI/CSM off) and re-run the
   3060 Ti as `rebar-on`? Can be combined with the GPU-swap reboot.
3. Close browsers/launchers for the timing series?

## Next steps
1. Main series (card 1, rebar-off) running → check logs, report, record numbers here.
2. Human answers on `s2_rebar` / ReBAR / background apps → run `s2_rebar` accordingly.
3. GPU swap message → card 2 (4060 Ti) → §4 report and push.
