# Task for Claude Code on the Windows PC — M0 [PC] checks + M1 [PC] microbenchmark runs

You are Claude Code running on the **Windows 11 PC** (Vulkan platform of the Lightbound study).
The Mac side of M0 and M1 is already done. Your job is to do the PC side with as little manual work
for the human as possible, then push your fixes and results back so the Mac can continue.

The human only needs to do what you physically can't: sign in to GitHub once, swap GPUs, toggle
ReBAR in the BIOS, answer questions. Everything else is yours.

**Repository:** `https://github.com/jttarigan/Lightbound.git` (private, branch `main`).
**Your branch:** `pc/m0-m1` — all your commits go there; never push to `main`.

---

## −1. Get the code (you may be reading this file outside the repository)

1. If the current directory is not already a clone of the repository above:
   - check `git --version`; if Git for Windows is missing, ask the human to install it
     (`winget install --id Git.Git -e`) and restart the session;
   - pick a short path without spaces, e.g. `C:\dev` (ask the human if unsure) and run
     `git clone https://github.com/jttarigan/Lightbound.git C:\dev\Lightbound`. The first clone opens a GitHub sign-in
     (Git Credential Manager) — tell the human to complete it in the browser;
   - `cd` into the clone.
2. `git checkout pc/m0-m1` if that branch exists on the remote (a previous session already worked),
   otherwise `git checkout -b pc/m0-m1`.
3. From now on, work inside the clone and follow `docs/PC_CLAUDE_TASK.md` **from the clone** (it is
   the same file; the clone's copy is authoritative if they differ).
4. `git config user.name` / `user.email` — if unset, set them locally in the clone
   (`git config user.name "jostarigan"`, `git config user.email "jostarigan@outlook.com"`).

## 0. Read first (in this order)

1. `CLAUDE.md` — project rules and research invariants R1–R7. **They apply to you.**
2. `docs/STATUS.md` — where the project stands, the Mac results, and why M1 is not closed.
3. `docs/DECISIONS.md` — especially #11–#26 (M0/M1 decisions). Don't undo them.
4. `docs/07_MILESTONES.md` — M0 and M1 acceptance criteria.
5. `docs/06_RESEARCH_PROTOCOL.md` §2 (platforms), §6 (microbenchmark), §12 (CSV schemas).

Before starting, check `docs/PC_PROGRESS.md`. If it exists, a previous session already did part of
this task (the PC reboots when GPUs are swapped): **resume from where it stopped.**

## Hard rules

- **Do not start M2** or any other milestone. M1 is a go/no-go gate; the human decides.
- **Allowed fixes:** compile errors and warnings (MSVC `/W4 /WX`), link errors, Windows-only bugs,
  and genuine Vulkan validation errors (wrong barrier/stage/usage flags etc.). Keep each fix minimal,
  keep it working on the Mac (don't break the Metal or MoltenVK builds — use `#if defined(_WIN32)`
  only when the fix is really Windows-specific), and record every non-trivial fix in
  `docs/DECISIONS.md` with date and reason.
- **Stop and ask the human** before any fix that would change measured behaviour: algorithms, buffer
  layouts, memory classes / placement (`src/frame/MemoryPolicy.cpp`), what the timed intervals contain,
  sync semantics, or anything that differs between backends (R1). Also ask before installing system
  software (Visual Studio, Vulkan SDK, drivers). `pip install --user` of Python packages is fine.
- **Never weaken a check to make it pass** (checksums, R7 copy counter, validation error count,
  warnings-as-errors).
- **During timing runs the machine must be otherwise idle.** While `run_microbench.py` is running do
  not build, do not run other GPU programs, do not run heavy commands. Launch it, then only wait
  (poll a file at most every ~30 s). Timing runs use **no** `--validation` flag.
- Keep `docs/PC_PROGRESS.md` updated after every step (what's done, what's next, key numbers), and
  **commit + push it to `pc/m0-m1` after every step**, so a new session can resume after a reboot
  (even on a fresh clone).
- Commit messages: imperative summary line, then details; end with the attribution line your
  harness gives you (if any). Never commit `build\` or raw `.csv` files (see section 4).

---

## 1. Environment check (no changes yet)

Run these and record the results in `docs/PC_PROGRESS.md`:

| What | How | Needed |
|---|---|---|
| GPU, driver, PCIe link | `nvidia-smi --query-gpu=name,driver_version,pcie.link.gen.max,pcie.link.width.max --format=csv` | RTX 3060 Ti or 4060 Ti |
| Vulkan SDK | `echo %VULKAN_SDK%` (cmd) / `$env:VULKAN_SDK` (PowerShell), `vulkaninfo --summary` | SDK ≥ 1.3 with `VK_LAYER_KHRONOS_validation` |
| Visual Studio 2022 C++ | `"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath` | found |
| CMake ≥ 3.25, Ninja | `cmake --version`, `ninja --version` | both on PATH (VS ships both) |
| Python ≥ 3.9 | `python --version` (or `py -3 --version`) | yes |
| Python packages | `python -c "import pandas, matplotlib, scipy"` | else `python -m pip install --user pandas matplotlib scipy` |
| Power plan | `powercfg /getactivescheme` | "High performance" (or Ultimate) |

If something required is missing, tell the human exactly what to install and stop.
If the power plan is wrong, ask the human (`powercfg /setactive SCHEME_MIN` sets High performance —
ask before running it). Also ask the human to confirm NVIDIA Control Panel → Manage 3D settings →
Power management mode = **Prefer maximum performance**.

### Build environment
MSVC needs the VS developer environment. From your shell, wrap build commands like this (cmd):

```
cmd /c "call "<VS path>\VC\Auxiliary\Build\vcvars64.bat" >nul && cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DLB_BACKEND=vulkan"
```

`<VS path>` = output of the vswhere command. If you are in Git Bash, run the same through
`cmd //c "..."`. The first configure downloads SDL3, Slang, VMA, etc. (a few minutes).

---

## 2. M0 [PC] checks (deferred from M0, DECISIONS #16)

1. Configure and build (Release, `LB_BACKEND=vulkan`) as above, then `cmake --build build`.
   The Vulkan/Windows code has **never been compiled with MSVC**: expect some `/W4 /WX` errors
   (typical: C4267/C4244 conversions, C4100 unused parameters, C4456/C4458 shadowing, C4702
   unreachable code). Fix them properly (casts with the right type, not blanket `#pragma warning`
   disables; a targeted disable is acceptable only for third-party headers). Rebuild until **zero
   warnings**. Record notable fixes in DECISIONS.md.
2. `ctest --test-dir build --output-on-failure` — all tests must pass. It includes
   `microbench_smoke`, which runs every microbench path on the GPU with `--validation=on`.
3. Play-mode smoke test with validation:
   `build\lightbound.exe --mode=play --validation=on --exit-after=300` then check the exit code.
   Pass = log contains `Vulkan validation layer enabled`, **no** `ERROR [vk]` lines,
   `backend errors: 0`, exit code 0. A window opens and closes by itself.
4. `build\lightbound.exe --mode=bench` must print "not implemented" and exit 2.
5. Full-payload validation pass over the research sync code (not a timing run):
   `build\lightbound.exe --mode=microbench --validation=on --frames=30 --warmup=5 --out=results\validation_check.csv`
   Pass = exit 0, `0 verification/R7 failures, 0 API errors`, no `ERROR [vk]` lines.
   Validation errors here are likely real bugs in barriers / timestamp stages / semaphore usage in
   `src/gfx/vulkan/VulkanGfx.cpp` — fix them (they are correctness fixes), but if a fix changes *what*
   is synchronized or timed rather than *how it is expressed in Vulkan*, stop and ask.
   `WARN [vk]` performance warnings: list them in the report, don't chase them.

Record M0 results in `docs/PC_PROGRESS.md`.

---

## 3. M1 [PC] timing runs

For each GPU (whichever is installed now first):

1. Confirm which card is installed (`nvidia-smi`) and whether ReBAR is on: run
   `build\lightbound.exe --mode=microbench --frames=5 --warmup=1 --micro-paths=empty --out=results\probe.csv`
   and read `sysinfo.rebar` and `sysinfo.bar_heap_mb` from the CSV header (ReBAR on ⇔ BAR heap
   > 256 MB). Use that for the tag: `rebar-on` / `rebar-off`.
2. Make sure nothing else is running (close browsers/launchers if the human agrees; at least don't
   run anything yourself). Then run the timing series — **3 separate process runs with cooldowns**:
   ```
   python tools\run_microbench.py --exe build\lightbound.exe --out results\m1_3060 --tag rebar-on
   ```
   (`m1_4060` for the 4060 Ti.) Run it in the background and wait. A full run can take a long time:
   `s2_rebar` deliberately makes the CPU read uncached PCIe memory, which is slow at 16 MiB. After the
   first run finishes, note its duration in `PC_PROGRESS.md`. If one run takes over ~90 minutes, tell
   the human and ask whether to continue.
3. Check the results:
   - each `micro_*.log` ends with `0 verification/R7 failures, 0 API errors` and exit code 0;
   - CSV header: `sysinfo.pcie_link` shows a measured link like `gen4 x16` (3060 Ti) / `gen4 x8`
     (4060 Ti). If it says `unknown`/`not measured`, investigate (nvidia-smi on PATH?) and report;
   - `sysinfo.power_plan` is High performance.
   - `results\m1_<gpu>\report\report.md` was produced (run-to-run variation, tables, figure).
4. Update `docs/PC_PROGRESS.md` (numbers: empty and 1 MiB round-trip p50 per path, chain/spin;
   run-to-run verdict).

### Swapping GPUs
After the first card is done, **tell the human**: "Please shut down, install the other RTX card in
the same PCIe slot, boot, open Claude Code **in the clone folder** (e.g. `C:\dev\Lightbound`) and
say: *Continue docs/PC_CLAUDE_TASK.md*". Before you ask, make sure `PC_PROGRESS.md` says exactly
what is done and what is next, and that it is committed and pushed (results so far included, gzipped
as in section 4, so nothing is lost if the disk state is unexpected after the swap).
After the swap: re-run step 1 of section 3 (nothing needs rebuilding) and do the second card.

### Optional: ReBAR off (protocol §9)
Only if the human wants it: they disable Resizable BAR (and usually "Above 4G decoding" stays on) in
the BIOS; you repeat section 3 with `--tag rebar-off` into a separate directory
(`results\m1_3060_rebar-off`). Never mix ReBAR-on and -off runs in one results directory — the
report pools all runs of a platform.

---

## 4. Report and hand back via git

1. Combined PC report:
   ```
   python tools\analysis\micro_report.py results\m1_3060 results\m1_4060 --out results\m1_pc_report
   ```
   (Add any other `results\m1_*` directories you produced, e.g. ReBAR-off, as separate reports.)
   The go criterion section will say PENDING (no Mac AC data on the PC) — that is expected; the Mac
   side combines everything.
2. Write `docs/PC_REPORT.md` with:
   - M0 [PC]: pass/fail for each check in section 2, build warnings fixed, validation findings.
   - Every source change you made (file + one line why), and the DECISIONS.md entries you added.
   - M1 [PC]: environment (GPU, driver, PCIe link measured, ReBAR, power plan), per card the table
     from `report.md` (round trip p50/p99, chain/spin), g2c/c2g p50 for `empty` and `s2_direct` 1 MiB,
     raw copy bandwidth, run-to-run variation verdict, run durations, anything odd.
   - Short comparison with the Mac numbers in `docs/STATUS.md` (M5: g2c ≈ 40 µs, c2g multimodal
     45/155/235 µs, empty round trip p50 ≈ 200 µs — preliminary, on battery). Facts only; the
     go/no-go decision belongs to the human.
3. Put the results into git (results are normally gitignored and raw CSVs are 10–25 MB each):
   - gzip every run CSV: `python -c "import gzip,shutil,glob; [shutil.copyfileobj(open(f,'rb'), gzip.open(f+'.gz','wb')) for f in glob.glob('results/m1_*/micro_*.csv')]"`
   - `git add -f results/m1_*/*.csv.gz results/m1_*/*.log results/m1_*/report/* results/m1_pc_report/*`
     (plus `results/validation_check.csv` gzipped if you like). Do **not** add the raw `.csv` files.
   - `git add` your source/doc changes, including `docs/PC_REPORT.md`, `docs/PC_PROGRESS.md`,
     `docs/DECISIONS.md`.
   - commit on `pc/m0-m1` and `git push -u origin pc/m0-m1`.
4. Final message to the human: a short summary (M0 pass/fail, M1 per-card headline numbers, open
   issues) and: "Everything is pushed to branch `pc/m0-m1`. On the Mac, tell Claude: *Merge
   pc/m0-m1 and continue from docs/PC_REPORT.md*." Then stop.
