# Work status (resume here)

_Last updated 2026-10-13 (session 8: state refresh for Main. Step C is final: run-to-run PASS, pushed 10-13 14:25. The RTX 4060 Ti exists after all
(the user's device list, LAB.md 2026-10-13), so finding 8 is reopened. No code work, nothing started)._

## To-do (2026-10-13, in order)
Tags: **[solo]** = this session, unattended; **[device: X, ~time]** = needs a machine; **[user]** = the human's hands or a decision.
Nothing below is started. Device slots come from Main's day plan. No phones or tablets are needed.

1. ~~Let the PC finish the run-1 replacement~~ **Done 2026-10-13.** The redo ran 10-12 10:45 → 10-13 10:30. Run-to-run
   PASS, report built, pushed 14:25 (head 4e051ca). Step C is final. The PC's own notes still say "next: GPU swap (step D)".
   Main has the corrected text for it: **step D is on hold** until the human picks the second PCIe condition (2a).
   Between series the PC is free for other projects. Keep its measurement setup unchanged, though: NVIDIA driver 566.36,
   ReBAR on, Ultimate Performance, "Prefer maximum performance", and leave `C:\dev\Lightbound` alone.
2. **[user] Decisions.**
   (a) **Second PCIe condition** (finding 8, reopened). The 4060 Ti exists, but its PC is broken until ≈ 10-27.
   Recommended: **wait for the 4060 Ti** and run it as the protocol specifies, in the same i7-12700F PC and the same slot
   (protocol §2). It will run at gen3 x8 there. First ask the user: does the card itself work, and may it move into the
   i7 PC? Its own PC is probably the Ryzen that Invadex and NeonTide plan to use. The fallback is the narrowed-link 3060 Ti
   (D′) if the card isn't usable by ≈ 10-27. The gate doesn't have to wait for either: protocol §6 needs the M5 clearly
   below *both* cards, so a NO-GO against the 3060 Ti stands whatever the second card shows. Running both (4060 Ti and
   D′) would separate link width from GPU generation, but it costs a second multi-day series. Only worth it if H5 needs it.
   (b) **`s2_rebar` in the second condition?** It is ≈ 23 of each run's 24 hours. Without it, a 3-run series takes ≈ 3 h
   instead of ≈ 3 days. Recommended: full protocol, as in C, so the two conditions match, unless the PC is needed sooner.
   (c) **Hot-GPU microbench cell** (finding 7)? Recommended: yes. Hot is the regime the game runs in, and hot M5 ≈ 55 µs vs
   3060 Ti 60 µs is what go/no-go turns on. It needs a hot-only run on the 3060 Ti at the full link too, and on the Mac.
   (d) **PCIe gen3** (finding 5)? Recommended: accept the measured gen3 links as the platform definition. The protocol's
   "PCIe 4.0 x16 / x8" can't be met on this board.
3. **[solo, ~½ day] then [device: Mac on AC, quiet, ~30 min] and [device: Windows PC, ~1 h], only if 2c = yes.** Implement the
   hot-GPU cell on Metal and Vulkan with the same algorithm (R1), with tests and ctest on both Mac backends. Then do 3 Mac runs:
   Mac on the charger, lid open, under `caffeinate -is` (LAB.md). No other session may run GPU work, builds or other heavy
   jobs on the Mac during them. The PC part goes to the PC as a bundle (SSH push is blocked), between series.
4. **[user, before 2026-10-16 14:28] Windows Update on the PC.** Extend the pause, or at least keep the NVIDIA driver at
   566.36. Protocol §2 wants the same driver for both PCIe conditions, and C ran on 566.36.
5. **[user] M1 go/no-go**, possible as soon as the hot-cell data is in (or now, without it). By the letter of protocol §6
   it is NO-GO: the M5 must be clearly lower than the best Vulkan path on both cards. Cold M5 is 214 µs empty and 305 µs at
   1 MiB. The 3060 Ti is 60 µs and 142 µs (s2_direct with ReBAR off; with ReBAR on, s2_hostcached 159 and s2_direct 207).
   Hot M5 is ≈ 55 µs empty; hot 1 MiB is unmeasured. Decide at the same time: rt alone or rt + GPU read for ReBAR on
   (recommended: rt as the protocol defines it, with rt + GPU read as a secondary figure), and whether the repo stays
   public (recommended: keep it public for now, because anonymous HTTPS is the Mac's only working way to fetch the PC's
   results). M2 starts only after a GO. On NO-GO the protocol says re-scope: the paper pivots to staleness + energy + design patterns.
6. **Second PCIe condition, as decided in 2a.**
   - 4060 Ti (recommended): **[user]** moves the card into the i7 PC, same slot, ReBAR on. **[device: Windows PC, exclusive,
     ≈ 3–4 days unattended (≈ 3 h without `s2_rebar`)]** the PC runs step D from its task file as written (`results\m1_4060`).
     No new code is needed.
   - D′ (fallback): **[solo, ~30 min]** rebuild the narrow-link bundle on the PC's newest head (replay ec93327; the old file is
     gone, and no file overlaps). Refresh `docs/PC_TASK_NARROW_LINK.md` (C is done, quiet-start lesson). **[user, ~30 min at
     the PC]** moves the bundle over and narrows the link in the BIOS (CPU x8 slot, else force Gen2). **[device: Windows PC,
     exclusive, same durations]** `--platform=P-3060-narrow`, then **[user]** restores the BIOS.
   - Either way, nobody may use the PC during a series. The first ≈ 4 min of each run and the last ≈ 50 min
     (`s2_coherent`) are sensitive even to a git push (the lesson from C). Afterwards the PC writes `PC_REPORT.md` and pushes.
7. **[solo, ~2 h, when the human says merge]** Merge `pc/m0-m1` into `main`, rebuild Metal and MoltenVK, run ctest and
   `micro_report.py` (Mac AC vs the two PCIe conditions, ReBAR-off in its own report), and finish the M1 report (section C).
8. **[user, any time, low priority] Pushing.** SSH to GitHub still times out (port 22, checked 2026-10-13). Every `main`
   commit after `origin/main` (baf605a) is unpushed, and so is `mac/narrow-link`. Fix: a network where port 22 works, or an HTTPS token.

**Deadline:** the PC's Windows Update pause ends 2026-10-16 14:28 WIB (to-do 4). No series is due before then unless 2a picks D′ now.

## State

- **M0 — Scaffold:** accepted on Mac (Metal + MoltenVK). **[PC] checks: ALL PASS** on the branch
  `pc/m0-m1` (2026-10-02; one MSVC fix, DECISIONS #27 on that branch). M0 is fully closed once the branch is merged.
- **M1 — Round-trip microbenchmark (GO/NO-GO gate):** implemented on both backends and verified
  on the Mac. **Not closed:** Mac AC re-run done (still fails run-to-run, see below); needs (2) PC runs on the 3060 Ti at the full link
  (C final, run-to-run PASS) and a second PCIe condition (the 4060 Ti, back ≈ 10-27, or the narrowed-link 3060 Ti, finding 8), (3) the human's go/no-go decision. Do not start M2 before that. New evidence for that
  decision: the M5 sync probe, finding 7. A hot GPU gives a round trip ≈ 55 µs, stable, and polling isn't viable.
- **Open human decisions:** go/no-go (the data so far says NO-GO by the letter, findings 4 and 7); the second PCIe
  condition (wait for the 4060 Ti or run the narrowed-link 3060 Ti, finding 8); whether to add
  a hot-GPU microbench cell (finding 7); the PCIe gen3 link (finding 5); whether the repo should be public (finding 6).
- **PC session:** on branch `pc/m0-m1` (see "PC session progress" below). M0 checks pass. A2 (ReBAR off,
  fixed clock mapping) is done and ReBAR is on. The ReBAR-on full-link series **C is final**: 3 clean runs, run-to-run PASS, pushed
  2026-10-13 14:25. Run 1 was redone on 10-12/13 because the PC's own git push had disturbed two cells of the first one. The pushed `PC_PROGRESS.md` plans "step D = swap to the 4060 Ti". The card exists but is in a
  broken PC until ≈ 10-27, so D is on hold until the human decides (to-do 2a). The narrow-link bundle (finding 8) is not merged on the pushed branch, and
  `~/Desktop/lightbound-narrow-link-2026-10-05.bundle` is gone from the Mac (not found by name on 10-12 or 10-13).
  It is rebuilt from local branch `mac/narrow-link` (to-do 2). Never apply it in the
  middle of a series, because a rebuild breaks `--resume`. On resume: fetch the branch (command below), read its `docs/PC_PROGRESS.md`,
  report progress, and wait. Merge only when the human says so (section C).

## Resume checklist (Mac)
```
export PATH="$HOME/Library/Python/3.9/bin:$PATH"
cd ~/Project/Lightbound
# SSH to github.com may time out on this network; the repo is public, so fetch over HTTPS:
GIT_TERMINAL_PROMPT=0 git -c credential.helper= fetch https://github.com/jttarigan/Lightbound.git \
    pc/m0-m1:refs/remotes/origin/pc/m0-m1
# The fetch can hang or fail quietly: check that the ref really moved (today's date in the reflog), else retry with
# -c http.lowSpeedLimit=1000 -c http.lowSpeedTime=60 (on 2026-10-12 a stale ref was reported as "nothing pushed")
git reflog show refs/remotes/origin/pc/m0-m1 --date=iso | head -1
git log --oneline main..origin/pc/m0-m1
git show origin/pc/m0-m1:docs/PC_PROGRESS.md
# Has the PC applied the narrow-link bundle (finding 8)?
git merge-base --is-ancestor mac/narrow-link origin/pc/m0-m1 && echo "bundle merged on PC" || echo "bundle not merged yet"
```
(macOS has no GNU `timeout` command, so don't wrap git calls in it.) Always fetch before saying what the PC
is doing. `PC_PROGRESS.md` on the branch is the only source; on 2026-10-05 a guess from the old plan turned out wrong.

## PC session progress (as of 2026-10-13 14:52, branch head 4e051ca, pushed 2026-10-13 14:25 +0700)
Read from `origin/pc/m0-m1:docs/PC_PROGRESS.md`. Since 8ce9af6 the PC pushed four docs/results commits and no code.
- **C final:** 3 runs of 23.8 h each, binary 6962eff: run 1 is the replacement (10-12 10:45 → 10-13 10:30), and runs 2 and 3
  ran 10-06/07 and 10-07/08. Exit 0, 0 failures, no restarts, no reboot since 10-05. ReBAR on (bar heap 8024 MB), gen3 x16,
  driver 566.36, Ultimate Performance. CSVs gzipped and the report pushed (`results/m1_3060`). Round trip p50 / p99 (µs),
  chain/spin, 3 runs pooled:

  | path | 0 | 4K | 64K | 256K | 1M | 4M | 16M |
  |---|---:|---:|---:|---:|---:|---:|---:|
  | empty | 60 / 83 | | | | | | |
  | s2_direct | | 60 / 79 | 69 / 104 | 96 / 135 | 207 / 279 | 646 / 804 | 2387 / 2652 |
  | s1_copy | | 73 / 98 | 89 / 113 | 131 / 166 | 311 / 383 | 1054 / 1259 | 3935 / 4395 |
  | s2_hostcached | | 62 / 91 | 69 / 102 | 87 / 124 | 159 / 231 | 463 / 614 | 1742 / 2274 |
  | s2_rebar | | 1748 / 1856 | 27163 / 27960 | 108712 / 110651 | 436651 / 442876 | 1758034 / 1769897 | 7032390 / 7064631 |
  | s2_coherent | | 119 / 148 | 991 / 1217 | 3817 / 4324 | 14960 / 16269 | 59800 / 64968 | 239183 / 256932 |

  g2c / c2g p50 for empty 30.9 / 29.0 µs. Raw copy 16 MiB d2h 12.36 / h2d 13.27 GB/s. **Run-to-run PASS:** 124 cells, none
  ≥ 10 %, worst 8.4 % (s2_hostcached perpass/block 16M), worst primary-condition cell 7.4 %. ReBAR changes only
  `s2_direct`: every other path agrees with A2 within a few percent. For `s2_direct` at 1 MiB, rt is 207 vs 142 µs and
  `cpu_write` 96 vs 33 µs, while the GPU read pass (outside rt) becomes almost free.
- **The disturbed first run 1** is kept in `results/m1_3060_run1_disturbed/` (README; don't pool it). The PC's own push at
  ≈ 14:37:40 on 10-05 spoiled two of its secondary cells (`s2_hostcached` perpass/spin 1M +17.6 %, 4M +11.8 %).
- **PC's lesson (for every future series):** no git commit or push, no log reads, nothing at all on the PC during the first
  ≈ 4 minutes of each run (the fast paths) or during `s2_coherent` (the last ≈ 50 min).
- Its "Next steps" are step D = 4060 Ti swap (≈ 3 days, gen3 x8 expected, extend the Windows Update pause first), then
  E = `PC_REPORT.md` + push. The 4060 Ti exists but isn't usable before ≈ 10-27, so D waits for the human (to-do 2a).

### PC notes from 2026-10-07 (branch head 8ce9af6, pushed 2026-10-07 14:29 +0700)
Read from `origin/pc/m0-m1:docs/PC_PROGRESS.md`. Since 6962eff the PC pushed seven docs/results commits and no code;
DECISIONS on the branch is still at #30.
- **A2 done (2026-10-05 10:26–13:03):** 3060 Ti, ReBAR off, fixed clock mapping, gen3 x16, Ultimate Performance;
  3 runs, 0 failures, no restarts; `results/m1_3060_rebar-off` (+ report) is on the branch. **These are the ReBAR-off
  numbers to use.** Round trip p50 / p99 (µs), chain/spin, 3 runs pooled:

  | path | 0 | 4K | 64K | 256K | 1M | 4M | 16M |
  |---|---:|---:|---:|---:|---:|---:|---:|
  | empty | 60 / 99 | | | | | | |
  | s2_direct | | 59 / 88 | 65 / 89 | 80 / 114 | 142 / 212 | 388 / 530 | 1392 / 1684 |
  | s1_copy | | 74 / 120 | 90 / 129 | 131 / 178 | 312 / 408 | 1074 / 1298 | 3983 / 4583 |
  | s2_hostcached | | 59 / 93 | 67 / 106 | 85 / 146 | 157 / 239 | 470 / 606 | 1806 / 2134 |
  | s2_coherent | | 117 / 152 | 991 / 1168 | 3810 / 4296 | 15063 / 16251 | 60172 / 64126 | 239800 / 254593 |

  g2c / c2g p50 for empty 32 / 28 µs; raw copy 16 MiB d2h 12.55 / h2d 13.17 GB/s. **Run-to-run PASS** (100 cells, none
  ≥ 10 %; worst 9.6 %, worst primary cell 6.9 %). 0 of 100 cells with a negative g2c sample (26 in step A), so the clock
  fix works; `rt` agrees with step A within a few percent.
- **B done:** the human enabled ReBAR in the BIOS on 2026-10-05 (`sysinfo.rebar: on`, `bar_heap_mb: 8024`).
- **C running (3060 Ti, ReBAR on, full protocol incl. `s2_rebar`, binary 6962eff, tag `rebar-on`, `results\m1_3060`):**
  started 2026-10-05 14:34. Run 1 finished 10-06 14:18 (23.7 h) and run 2 finished 10-07 14:04 (23.8 h), both exit 0,
  0 failures, clean logs. Run 3 started 10-07 14:04, expected end ≈ **2026-10-08 13:50**, after which the PC builds the
  report, gzips, commits and pushes. Key cells agree across runs within 1–4 % (1 MiB chain/spin: `s2_direct`
  208 / 207 / 207 µs, `s2_hostcached` 165 / 159 / 159; `s2_rebar` 16 MiB ≈ 7.0 s per iteration). Two progress-log pushes
  fell into run 1's first three minutes (fast-path cells); run 1's p99s are higher than run 2's (`empty` 107 vs 73 µs).
  Runs 2 and 3 are undisturbed.
- **ReBAR on makes `s2_direct` slower from 64K up** (chain/spin rt p50 4K…16M: 61 / 72 / 98 / 208 / 678 / 2506 µs vs
  60 / 65 / 81 / 142 / 411 / 1428 with ReBAR off), as designed by DECISIONS #17 (the CPU→GPU buffer sits in ReBAR memory).
  The PC's per-phase breakdown (run 1): g2c, c2g and `cpu_read_us` are unchanged; `cpu_write_us` into the ReBAR buffer is
  ≈ 3× slower (1 MiB 96 vs 33 µs, 16 MiB 1541 vs 523 µs: ≈ 11 GB/s write-combined over PCIe vs ≈ 32 GB/s into system
  memory). In exchange the GPU's read pass, which `rt` does not include, becomes almost free (`gpu_read_us` 1 MiB 7 vs
  86 µs, 16 MiB 49 vs 1288), so rt + GPU read is slightly *lower* with ReBAR on from 256K up (1 MiB 215 vs 228 µs).
  **Which figure the comparison should use (rt alone, or rt + the GPU read) is a question for the report and the human.**
  Unexplained in a single run: `gpu_write_us` into system memory +66 / +107 µs at 4M / 16M, and g2c at 16M 125 vs 54 µs
  with ReBAR on.
- The human extended the Windows Update pause to **2026-10-16 14:28** local. That only covered the narrow-link series if
  it started by 10-12, so it must be extended again before D′ (to-do 5).
- **The PC has not merged the narrow-link bundle** (`git merge-base --is-ancestor mac/narrow-link origin/pc/m0-m1` says
  no) and its plan still ends with "ask for the GPU swap (step D)". The bundle was meant to go in between A2 and C (task
  file step 2), but C started first. Consequences: (1) after C the human hands over the bundle and
  `docs/PC_TASK_NARROW_LINK.md`; (2) the PC branch has moved past 6962eff, so `git merge bundle/narrow-link` is a real
  merge, not a fast-forward — a trial merge on the Mac (2026-10-07) was clean and no file is touched by both sides;
  (3) C runs on 6962eff and D′ will run on the merge commit, which differ only by the `--platform` plumbing (no measurement
  path changes); the write-up should say so.
- **PC plan on 10-07 (superseded by the to-do list):** C run 3 (→ 10-08 ≈ 13:50) → PC report, gzip, push → human: bundle + narrow-link task → D′ (3 runs,
  ≈ 3 days, `--platform=P-3060-narrow`, `results\m1_3060_narrow`) → E: `PC_REPORT.md` + push → Mac: merge (section C).

### PC notes from 2026-10-05 (branch head 6962eff, pushed 2026-10-05 10:24 +0700)
Read from `origin/pc/m0-m1:docs/PC_PROGRESS.md`. Since 5faba40 the PC had added three commits:
- **#28 Resumable microbench runs** (human request: one full run takes ≈ a day). `lightbound --resume` and
  `run_microbench.py --resume` resume at **cell** boundaries. The run refuses to continue if the binary, options, tag or any
  `sysinfo.*` value differs (the PCIe link only gives a warning), and each restart writes a `# resumed:` line in the CSV.
  The runner keeps unfinished runs as `_running_run<i>.csv`. (This is what "resume from iteration 40000" was about.)
- **#29** The runner streams lightbound's output to the log, and `micro_report.py` writes UTF-8 (on Windows the
  first `report.md` came out empty because of the "→" character).
- **#30 Clock-mapping fix (human decision, amends #23).** On the 3060 Ti the GPU timestamp clock and QPC drift
  ≈ 32 ppm apart. With recalibration only every 500 iterations, g2c fell and c2g rose linearly inside each block.
  `rt` and `g2c + c2g` were unaffected, but the split was biased: ≈ 1 µs for empty, ≈ 3 µs at 1 MiB, badly at 16 MiB, and
  milliseconds for `s2_coherent`. Now the clocks are recalibrated whenever the last calibration is older than 20 ms, and a
  long iteration gets a fresh calibration for c2g. `rt` p50 moved by at most ±1.3 % (noise). The first series is kept as
  `results/m1_3060_rebar-off_old-calibration` (valid `rt`, biased split).
  **Mac check (2026-10-05):** `results/m1_mac_ac` shows no such drift. The g2c median of the first vs last 50
  iterations of each 500-iteration block differs by +0.0 µs on average (|Δ| median 0.8 µs, mixed sign), with 0 negative g2c samples
  in 156 cells. Metal's shared mach clock plus the fitted slope already avoid it, so the Mac data stands.
- **Step A results (3060 Ti, ReBAR off, gen3 x16, old calibration: `rt` valid), round trip p50 / p99 µs, chain/spin:**
  empty **61 / 109**; s2_direct 4K 61, 64K 67, 256K 82, **1M 142**, 4M 408, 16M 1411; s1_copy 1M 314, 16M 4014;
  s2_hostcached 1M 157; s2_coherent 1M 15085. Raw copy 16 MiB d2h 12.3 / h2d 13.2 GB/s. **Run-to-run: PASS**
  (100 cells, worst 9.3 %; worst primary-condition cell 8.4 %).
- **PC plan as of 2026-10-05 (superseded above):** A2 = A again with the fixed binary (running on 2026-10-05, ≈ 2 h 40 min) → B: human enables ReBAR
  in the BIOS → C: 3060 Ti full link, ReBAR on, full protocol incl. `s2_rebar`, 3 runs (≈ 3 days, resumable) →
  ~~D: 4060 Ti~~ **D′: narrowed link** (finding 8) → E: report + push.
- DECISIONS.md on the PC branch now goes up to #30.

### Earlier PC notes (2026-10-02)
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
- The 2026-10-02 plan (A → B → C → D: 4060 Ti → E) is superseded by the current plan above. Risk for the multi-day
  runs: a Windows Update auto-restart (the human should pause updates). Since #28 an interrupted run can be resumed.
- The PCIe gen3 question (finding #5) still has **no recorded human decision** on the branch.
- **Merge rehearsal (2026-10-05):** a trial merge of `origin/pc/m0-m1` at 5faba40 into `main` was clean, and
  ctest passed 36/36 on Metal and MoltenVK. The PC has since changed `src/bench`, `src/app` and the tools (#28–#30). The
  branch `mac/narrow-link` (PC head + the `--platform` commit) builds and passes ctest 38/38 on both Mac backends,
  so the PC's new code compiles and passes on macOS too. `main` has touched only docs and `tools/probes` since, so
  the final merge should stay simple.
- DECISIONS.md: see the current section above (the PC is at #30).

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
   "clearly lower" → NO-GO by the letter of protocol §6. **PC timing data (3060 Ti, gen3 x16): empty round
   trip p50 60 µs vs M5 214 µs (AC); 1 MiB s2_direct 142 µs with ReBAR off (A2, pooled) and 207 µs with ReBAR on
   (C final, 3 runs pooled; best Vulkan path with ReBAR on: s2_hostcached 159 µs) vs M5 305 µs.**
   As specified (cold GPU), the M5 is clearly *slower* on both criterion cells, so M1 is **NO-GO by the letter of
   protocol §6** unless the human takes one of the options below. With a hot GPU (finding 7), the M5's empty round trip is
   ≈ 53–57 µs vs 60 µs, about 10 % lower and probably not beyond run-to-run noise. The hot 1 MiB case is
   unmeasured, because the probe ran the empty payload only. Options for the human (not implemented):
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
8. **Reopened 2026-10-13: the RTX 4060 Ti exists.** The user's device list (LAB.md, 2026-10-13) says a PC with a 4060 Ti
   exists, probably the Ryzen 7 7700 PC. It is broken now and may be back ≈ 2026-10-27. The "no 4060 Ti" note below was
   wrong, so the replacement decision rests on wrong information and goes back to the human (to-do 2a). Nothing below was
   merged, and no DECISIONS entry was written, so there is nothing to undo. `mac/narrow-link` stays as the fallback.
   Original note: **No RTX 4060 Ti (2026-10-05).** The human has only the 3060 Ti. P-4060 (protocol §2) exists to give
   H5 an x8 link ("on PCIe the S1–S2 gap depends on link width (x16 vs x8)"). **Human decision
   2026-10-05: replace P-4060 with the 3060 Ti at a narrowed link**, platform id `P-3060-narrow`. Same GPU,
   so only the link changes. The order of preference for narrowing (feasibility check on the PC, with the human)
   is: a CPU-attached x8 slot (gen3 x8), then the BIOS forcing Gen2 on the primary slot (gen2 x16 ≈ gen3 x8 bandwidth, slightly
   different per-transfer latency), then a chipset x4 slot only with the human's explicit OK (it adds a hop). If none
   works, the fallback is to drop P-4060. PC instructions: `docs/PC_TASK_NARROW_LINK.md`.
   - Spec text this changes (not edited; recorded here and in DECISIONS at merge): CLAUDE.md "RTX 4060 Ti x8",
     protocol §2 P-4060 row, M1 acceptance "[PC with 4060 Ti]", and go criterion "both RTX cards", which now means
     both PCIe conditions (P-3060, P-3060-narrow). H5's link-width part is tested as link *bandwidth* on one card.
   - **Tooling lives on local branch `mac/narrow-link`** (ec93327 = PC head 6962eff + one commit), not on `main`, so that
     the PC can fast-forward and `main` picks it up with the eventual merge of `pc/m0-m1`. It adds a new dev flag
     `--platform=ID` that overrides the detected platform id in CSV rows, in `sysinfo.platform` (the header keeps the
     detected id as `sysinfo.platform_detected`) and in the run script's file names. The flag is part of the options
     that `--resume` compares: tested that the same value resumes, and a different or missing value is refused.
     `micro_report.py` counts `P-3060-narrow` as a PCIe platform for the go criterion and names the platforms it compared.
     ctest 38/38 on Metal and MoltenVK. The task file `docs/PC_TASK_NARROW_LINK.md` is on that branch too.
   - **Delivered by git bundle** (SSH push blocked): `~/Desktop/lightbound-narrow-link-2026-10-05.bundle` (branch
     `mac/narrow-link`, full history, ≈ 10 MB). Tested on a fresh clone of `pc/m0-m1`: fetch + merge is a fast-forward.
     **2026-10-12: the file is gone from the Desktop.** It gets rebuilt on the PC's current head (to-do 2).
     The PC should merge it only when no series is unfinished (a rebuild changes `git_commit`, and `--resume` then refuses);
     the natural moment was after A2 and before C; C started first, and C is now final, so it can go in any time.
   - **DECISIONS.md entries** for the P-4060 → P-3060-narrow replacement and for the `--platform` flag are written by the PC
     on its branch (task file step 8), after its #30. `main` still doesn't touch DECISIONS.md.
   - Optional, still undecided: a hot-GPU microbench cell (finding 7). It would also need to run in the second PCIe
     condition. The session that sets that condition up is the cheapest moment to add it, but only with approval.

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
Then the second PCIe condition (finding 8, human's choice): either swap in the 4060 Ti (same PC, same slot, task file step D,
`results\m1_4060`), or follow `docs/PC_TASK_NARROW_LINK.md` (delivered in the
bundle, branch `mac/narrow-link`): narrowed link, `--resume -- --platform=P-3060-narrow`, into `results\m1_3060_narrow`. Optional (protocol §9): ReBAR off in BIOS, repeat with
`--tag rebar-off` into separate directories. Note: `s2_rebar` at 16 MiB reads uncached PCIe memory
from the CPU on purpose (the slow path); a full run may take tens of minutes.

M0 pass = build clean (expect some MSVC `/W4 /WX` fixes — Vulkan/Windows code never compiled with
MSVC), ctest all pass (the `microbench_smoke` test runs every path with validation on), play log shows
`Vulkan validation layer enabled`, no `ERROR [vk]`, exit 0.

### C. Combine and decide
```
python3 tools/analysis/micro_report.py results/m1_mac_ac results/m1_3060 results/m1_3060_narrow --out results/m1_report
```
Paste back `results/m1_report/report.md` and any errors. Keep ReBAR-off runs in their own report
(runs of one platform are pooled).

## Housekeeping
- Git: repository initialised 2026-09-24 on `main` (local identity jostarigan), remote
  `git@github.com:jttarigan/Lightbound.git` (Mac pushes with a deploy key over SSH). On 2026-10-01 SSH
  to github.com timed out from the Mac's network (ports 22 and 443). HTTPS fetch works (see the
  resume checklist), but pushing needs SSH. **Local `main` is ahead of `origin/main` (baf605a)** with
  every commit since session 3, all unpushed. **SSH still timed out on 2026-10-06 (ports 22 and 443,
  ssh.github.com), on 2026-10-12 and on 2026-10-13 (port 22).** The PC gets the narrow-link tooling through the bundle instead (finding 8). Local branch
  `mac/narrow-link` is unpushed too. When SSH works, push both: `git push origin main mac/narrow-link`. The PC branch doesn't depend on them. The preliminary
  `results/m1_prelim` runs predate the repo (their CSVs say `nogit`).
- Build dirs: `build/` (Metal), `build-vk/` (Vulkan via MoltenVK). Toolchain PATH:
  `export PATH="$HOME/Library/Python/3.9/bin:$PATH"`. Python analysis deps are installed (user site).
- Preliminary data: `results/m1_prelim/` (6 Metal + 3 MoltenVK runs, battery) — not valid for the study.
