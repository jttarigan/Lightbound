# Message to the PC Claude (from the Mac, 2026-10-13)

Read this when you resume on the PC. It arrives with a git bundle of the Mac's `main`; the user copies it.

## 1. Step D is back on: the RTX 4060 Ti exists
Your `PC_REPORT.md` ("the human has no RTX 4060 Ti") and the "D (cancelled)" row in `PC_PROGRESS.md` are
wrong, through no fault of yours: the user told the Mac on 2026-10-13 that the card is **in service and
comes back in 2–3 weeks (≈ 2026-10-27 to 11-03)**. The user installs it in the **same PCIe slot of this
i7-12700F PC** (protocol §2: same PC). If it ends up in another PC instead, stop and tell the user: that
changes the CPU and memory, and the Mac must know.

When you resume with the card installed:
1. Correct the two documents (one line each, dated).
2. Do step D exactly as the task file writes it (`docs/PC_CLAUDE_TASK.md` §3 steps 1–4): probe first
   (`sysinfo.rebar`, `bar_heap_mb`), then `python tools\run_microbench.py --exe build\lightbound.exe
   --out results\m1_4060 --tag rebar-on` (full protocol, `s2_rebar` included, ≈ 3 days; `--resume` after
   any interruption). Expect `sysinfo.pcie_link` = gen3 x8: the slot is gen3 (the user accepts the measured
   link as the platform definition, pending their final word). Keep the same driver (566.36), ReBAR on,
   Ultimate Performance, "Prefer maximum performance".
3. Nothing on the PC during the first ≈ 4 minutes of each run and during `s2_coherent` (the last ≈ 50 min);
   no git push mid-run.
4. Rebuild `results\m1_pc_report` from `results\m1_3060` and `results\m1_4060`, extend `PC_REPORT.md`, push.

## 2. New on `main` since your merge (in the bundle)
- **Hot-GPU cells** (`--micro-gpuload=idle|hot|both`, DECISIONS #32): a second queue keeps the GPU busy
  during the CPU step; cells are written as `<path>_hot`. Off by default; the protocol run is unchanged.
  On the RTX cards the Vulkan backend takes a second queue of the main family (`sysinfo.second_queue` in
  the header). New ctest `microbench_smoke_hot`.
- `micro_report.py` understands the `_hot` cells (`gpuload` column); idle-only reports are unchanged.
- Your DECISIONS #27–#31 are merged; the Mac continues at #32.

Merge the bundle only when no series is unfinished (a rebuild changes `git_commit`, and `--resume` then
refuses). Then rebuild (expect MSVC `/W4 /WX` to be clean; if not, fix and note it) and run
`ctest --test-dir build --output-on-failure` (40 tests).

## 3. If the user approves the hot-GPU / crossover extension (`docs/M1_RESCOPE_PROPOSAL.md` 1a)
Before or after step D, on whichever card is installed, into its own directory (never pooled with the
protocol series), ReBAR on, ≈ 3 h (rough):
```
python tools\run_microbench.py --exe build\lightbound.exe --out results\m1_3060_ext --tag rebar-on -- ^
    --micro-gpuload=both --micro-paths=empty,s1_copy,s2_direct,s2_hostcached ^
    --micro-payloads=4K,64K,256K,1M,1536K,2M,3M,4M,8M,16M,32M,64M
```
(`m1_4060_ext` for the 4060 Ti.) Check every `# cell_load:` note has duty ≈ 1 and the log shows
`0 verification/R7 failures, 0 API errors`. Push results gzipped as before.

## 4. Windows Update
The pause ends **2026-10-16 14:28 WIB**. The user extends it; if the driver changes anyway, record the new
version and tell the Mac before any series (protocol §2 wants the same driver for both cards).
