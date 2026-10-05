# PC follow-up task: RTX 3060 Ti at a narrowed PCIe link (stands in for P-4060)

For Claude Code on the Windows PC, continuing `docs/PC_CLAUDE_TASK.md` on branch `pc/m0-m1`.
Keep using `docs/PC_PROGRESS.md` as the resumable log, and follow the hard rules of `PC_CLAUDE_TASK.md`.
Written on the Mac on 2026-10-05, against `pc/m0-m1` at `6962eff` (the A2 re-run in progress).

## Why

There is **no RTX 4060 Ti** (the human, 2026-10-05). Protocol §2's P-4060 platform exists to give H5
an x8 link next to the 3060 Ti's x16. **Human decision (2026-10-05):** replace it with the **same 3060 Ti
at a narrowed link**, under the platform id **`P-3060-narrow`**. Only the link changes, not the GPU.
**Step D (GPU swap) in `PC_PROGRESS.md` is cancelled** and replaced by step D′ below.

## How this reached you

The Mac couldn't push (SSH to GitHub blocked), so the human copied a git bundle. It holds branch
`mac/narrow-link` = `pc/m0-m1` at `6962eff` + one commit: the `--platform` flag, the
`micro_report.py` change and this file. Bring it in with:
```
git fetch <path-to>\lightbound-narrow-link-2026-10-05.bundle mac/narrow-link:refs/remotes/bundle/narrow-link
git merge bundle/narrow-link
```
(If `pc/m0-m1` hasn't moved since `6962eff`, this is a fast-forward.) Then rebuild inside the vcvars environment (`cmake --build build`)
and run `ctest --test-dir build --output-on-failure`. Expect every test to pass. Push `pc/m0-m1` as usual.

**When to merge it:** only when **no series is unfinished**, i.e. no `_running_run*.csv` in any results
directory. A rebuild changes `git_commit`, and `--resume` refuses to continue a run under a different
binary. The natural moment is after A2 is done and before C starts. Don't rebuild in the middle of C or D′.

## Steps

1. **Finish A2** (ReBAR off, fixed calibration), record it, and push, as in `PC_PROGRESS.md`.
2. **Merge the bundle** (above). It doesn't change any measurement path: without `--platform` the binary
   behaves exactly as before.
3. **B and C as planned:** ReBAR on in the BIOS, then the full-link series with the full protocol incl.
   `s2_rebar` into `results\m1_3060` (`--resume` runner, as in `PC_PROGRESS.md`).
4. **D′: find a way to narrow the link. Ask the human, and change nothing yourself.** Board: MSI MS-B9241
   (OEM), BIOS 8.00. The full link is gen3 x16. In order of preference:
   1. **A second CPU-attached slot that is electrically x8:** gives gen3 x8, the exact analogue. This is rare on OEM boards.
   2. **Primary slot forced to Gen2** in the BIOS (often "PCIe/PEG link speed": Auto/Gen1/Gen2/Gen3).
      gen2 x16 gives ≈ 8 GB/s theoretical, the same as gen3 x8, on the same CPU lanes with no extra hop. Caveat for the report:
      5 GT/s lanes with 8b/10b encoding, so per-transfer latency differs slightly from a real x8 link.
   3. **A chipset-attached slot** (usually x4 over DMI). This adds a chipset hop and shares DMI bandwidth, a
      confound. Use it **only if the human explicitly agrees**, and record it.

   Never suggest taping PCIe contacts or any other physical modification. If none of these is
   possible, stop, write it in `PC_PROGRESS.md`, push, and tell the human: the Mac side then falls
   back to dropping P-4060. Keep ReBAR on.
5. **Verify the narrowed link** after the human changes it. Run the quick probe (`PC_CLAUDE_TASK.md` §3 step 1),
   adding `--platform=P-3060-narrow`, and check:
   - `sysinfo.pcie_link` (measured under load) shows the narrowed link, e.g. `gen2 x16` or `gen3 x8`;
   - `sysinfo.platform: P-3060-narrow`, `sysinfo.platform_detected: P-3060`;
   - ReBAR still on (`sysinfo.rebar: on`, `bar_heap_mb` > 256);
   - 16 MiB raw copy roughly half the full-link value (full link: d2h 12.3, h2d 13.2 GB/s, so expect ≈ 6–7).
6. **Run the series:** use the same protocol as C, 3 process runs, resumable:
   ```
   python tools\run_microbench.py --exe build\lightbound.exe --out results\m1_3060_narrow --tag rebar-on --resume -- --platform=P-3060-narrow
   ```
   Keep `--platform` identical on every restart: it is part of the options that `--resume` compares. The full
   protocol including `s2_rebar` was the human's choice (2026-10-02). At the full link that took ≈ 24 h per run, and
   it may take longer at half bandwidth. **Confirm with the human before starting** (≈ 3+ days), and
   ask them to pause Windows Update. Never mix full-link and narrow runs in one directory.
7. **Restore the BIOS setting** afterwards (with the human) and confirm with nvidia-smi that the link is
   back to gen3 x16.
8. **Report and hand back:** `python tools\analysis\micro_report.py results\m1_3060 results\m1_3060_narrow --out results\m1_pc_report`
   (ReBAR-off series in their own report, as before). Update `PC_REPORT.md` with the narrow-link method, the measured link,
   the tables and the run-to-run verdict. Gzip the CSVs, commit and push as in `PC_CLAUDE_TASK.md` §4.
   In `DECISIONS.md`, after the existing entries, record (a) the P-4060 → P-3060-narrow replacement
   (human decision 2026-10-05) with the narrowing method actually used and its measured link, and (b) the
   `--platform` dev flag (it overrides the platform id in CSV rows, `sysinfo.platform` and run file names;
   the detected id is kept as `sysinfo.platform_detected`).
