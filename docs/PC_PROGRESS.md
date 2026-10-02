# PC progress (resume here after a reboot / GPU swap)

Claude Code session log for `docs/PC_CLAUDE_TASK.md` on the Windows 11 PC. Newest entry at the
bottom of each section. Clone: `C:\dev\Lightbound`, branch `pc/m0-m1`.

_Last updated 2026-10-02 (session 1, started 2026-10-01)._

## Where we are

- [x] §−1 Clone + branch `pc/m0-m1` created, git identity set locally.
- [x] §0 Docs read (CLAUDE.md, STATUS.md, DECISIONS.md #11–#26, 07_MILESTONES.md, protocol §2/§6/§12).
- [x] §1 Environment check done — **blocked on the human** (see "Waiting on the human").
- [ ] §2 M0 [PC] checks (build, ctest, play smoke, bench stub, validation pass).
- [ ] §3 M1 [PC] timing runs — card 1: RTX 3060 Ti (installed now).
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

## Next steps
1. Human: approve/install Vulkan SDK, switch power plan, confirm NVIDIA power mode.
2. Configure + build (Release, vulkan), fix MSVC `/W4 /WX` warnings to zero, record in DECISIONS.md.
3. ctest, play smoke with validation, bench stub exit 2, validation pass over the microbench.
4. M1 probe (`sysinfo.rebar` / `bar_heap_mb`), then `run_microbench.py` for the 3060 Ti.
