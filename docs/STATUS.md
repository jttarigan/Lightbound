# Lightbound status (resume here)

_Saved 2026-10-26 ≈ 04:10 WIB (session 21 closed: user's Mac shutdown via Main; clock check run 10-25 07:08, analysed, 5fd28c7). Sessions 1–19, M1 findings
1–8, PC results and old run instructions: `docs/archive/STATE-history-2026-10.md` (read only when needed)._

## Resume point
- **Next: to-do 2** (user: spin-wait protocol decision a / b / b′; the clock check is done), then 7, 6, 1, 4, 9.
- Running: nothing. Devices: none in use. Word: not touched; scripted only on Main's turn. Push: main 45 ahead of origin.
- Mac shut down 10-26 ≈ 04:10: start a quiet block ≥ 30 min after boot (LAB.md). RTBench had the PC Mon 10-26 (M7): ask Main
  what it saw of `results\m1_3060_ext` / GPU_HOLD.txt.

## Current state
- **Milestones.** M0 accepted (Mac + PC). M1 measured (Mac AC 09-24; 3060 Ti ReBAR on/off, merged 10-13): NO-GO on H1,
  re-scoped 10-16 to H1′ = payload/N crossover (DECISIONS #33). 4060 Ti missing (November, accepted deviation). M2
  (procedural floors) built 10-16: awaits user review + [PC] MSVC golden check (DECISIONS #34). M3 after M2 is accepted.
- **M5 spin-wait shift** (paper §12.2 item 5, DECISIONS #36): Mac spin cells +170…+290 µs vs the 09-24 reference; block-wait
  cells fine. Ruled out: charger, displays, dark wake, launch context, nice, QoS/priority. **Cause confirmed 10-25 (root
  powermetrics, `pm_1025_0708/`): the SoC clock.** Bench alone: the spinning thread hops across the 4 S-cores at ≈ 2.7 GHz,
  spin cells +65…+268 µs. With 2× `yes` as companion load: S-cluster 4.3–4.5 GHz, spin cells +4…+34 µs (1 MiB included;
  4K perpass/s2_direct +80…+88), but block cells get slower (4K +85…+96 µs). GPU at 338 MHz in both. 09-24 matches neither
  state whole (block 4K ≈ idle, spin and block 1M ≈ loaded): it ran under uncontrolled load. Data
  `results/m1_mac_40w_check/` (README last section). Blocks m1_mac_ext (to-do 3).
- **Paper** (The Journal of Supercomputing): drafts in `docs/paper/` → `build_docx.py` → `lightbound_jsc.docx`; 21 live
  ZOTERO_ITEM + 1 ZOTERO_BIBL field, Zotero collection `Lightbound` (GMGWF6JT, 18 refs, `zotero_map.json`). Refresh test
  passed 10-23 12:3x (NPUBenchSwarm `tools/zotero_refresh_test.sh`; rerun after each rebuild, Main's Word turn). Drafted:
  §1–§5, §6.1–6.2, §9 (Word equation), §12, Declarations (`declarations.md`). Placeholders: §6.3, §8, §10, §11, §13 (need
  m1_mac_ext, m1_3060_ext, M3–M8). The repo has no LICENSE file.
- **PC:** last push on `pc/m0-m1` = c593742 (10-14). A PC session ran 10-20 12:02–12:11 WIB (NeonTide), driver 566.36.
  Whether m1_3060_ext ran then, and GPU_HOLD.txt, unknown (to-do 4). Fetch `pc/m0-m1` before saying what the PC did.

## To-do (dependency order; [user] = the user's hands or decision, [device] = machine time, [solo] = this session)
1. **[user] Review M2:** `docs/img/floors/*.png`, DECISIONS #34. The [PC] golden check rides with 4. Then M3.
2. **[user] Spin-wait shift, decide** (clock check done 10-25): (a) idle only: re-measure the idle reference as the Mac is
   now, report the shift (docs/06 §2 unchanged); (b) loaded only: a defined companion load (2 busy threads) becomes the
   microbench condition (docs/06 §2 changes; both platforms re-measure); (b′) both as named conditions, report the load
   sensitivity as a finding (ext series ≈ 2× time on Mac and PC). Recommendation: (b′), since the load moves Mac spin and
   block cells in opposite directions and the game never runs a lone thread. Then DECISIONS #37 + docs/06 §2.
3. **[device: Mac on AC, ≈ 70 min, quiet, Main slot; after 2]** `results/m1_mac_ext` (idle + hot, 12 payloads; command in
   `docs/M1_RESCOPE_PROPOSAL.md` §4) with `--exe build/frozen_ext_2d5c1ba/lightbound` (+ its `shaders/`; Metal Release,
   commit 2d5c1ba). Pre-checks per LAB.md (AC ≥ 35 W, ≈ 0 mA, full wake, load/top logged), one `caffeinate -is` for the
   whole series. Log: frozen binary built 10-16 08:37 WIB, before Xcode 27.1; `xcrun metal` 32023.921 before and after.
4. **[user] PC checklist** (RTBench has the PC Mon 10-26 for M7; it was asked to check `results\m1_3060_ext` and GPU_HOLD.txt): did m1_3060_ext run on 10-20; is `C:\dev\GPU_HOLD.txt` gone; driver still 566.36? If it did not
   run: **[device: PC, ≈ 3 h]** `results\m1_3060_ext` after the PC Claude fast-forwards and rebuilds, wrapped in the hold
   file (create → wait ≤ 10 min for no python in nvidia-smi → build/ctest → 10-min cooldown → measure → push → delete);
   nothing on the PC in the first ≈ 4 min; plus the M2 MSVC golden check (never regenerate goldens there).
5. **[solo, ≈ 1 h, after 3 + 4]** `python3 tools/analysis/crossover.py M5=results/m1_mac_ext 3060=results/m1_3060_ext
   --shared M5 --modes all --out results/crossover_ext`; `micro_report.py` (`gpuload` column); §6.3 + Fig. 2; fill the
   §1/abstract placeholders.
6. **[user] Word check:** title page (prefilled from NPUBenchSwarm: name, USU; e-mail/phone placeholders); reference style
   (Springer basic prints no DOI for conference papers, incl. IPDPSW, and no arXiv id); the §9.1 equation renders; the
   Declarations block (bracketed items). Then **[solo]** style fix, `python3 docs/paper/build_docx.py` (`--sync` after any
   Zotero edit), refresh test on Main's Word turn.
7. **[user] Paper decisions:** AI-use wording (proposal in `docs/paper/declarations.md`), a repository licence, funding
   statement, Zenodo DOI or not, extend the N grid past 100k (ReBAR-off crossover ≈ 112k agents).
8. **4060 Ti step D, November:** [user] installs it in the i7-12700F PC, same slot (ask before planning another PC);
   [device: PC, ≈ 3 days] `results\m1_4060`, optional `m1_4060_ext`; [solo] add P-4060 to the combined report.
9. **[user] Push:** main is 45 commits ahead of origin.

## Rules carried forward
- Never pool `results/m1_3060_run1_disturbed`; ReBAR off gets its own report; `*_ext` dirs stay apart from protocol series.
- No Mac pre-checks or test runs outside my Main slot (10-20: one hit NeonReactor's armed series).
- R1: no M5-only code; exploit the M5 through the workload (N, bytes/agent, K).
- PC setup stays: driver 566.36, ReBAR on, Ultimate Performance, "Prefer maximum performance", `C:\dev\Lightbound` untouched.
- `results/` is gitignored: `git add -f`; Mac CSVs gzipped; redact the charger serial from pmset logs (public repo).
- Analysis scripts: `python3` without `-I` (pandas lives in user site-packages).
- Toolchain: `export PATH="$HOME/Library/Python/3.9/bin:$PATH"`; build dirs `build/` (Metal), `build-vk/` (MoltenVK);
  no GNU `timeout`. Next DECISIONS number: #37.

## M1 combined report (2026-10-13): Mac M5 vs RTX 3060 Ti — facts for the human's go/no-go
`results/m1_report/report.md` (ReBAR on) and `results/m1_report_rebar-off/report.md`, built at 45b6182. Mac data: AC re-run
2026-09-24, 3 runs. PC data: 3 runs per condition. Round trip p50 / p99 in µs, chain/spin (the primary condition).

| | M5 (Metal, unified) | 3060 Ti ReBAR on | 3060 Ti ReBAR off |
|---|---:|---:|---:|
| empty round trip | 214 / 316 | 60 / 85 | 60 / 99 |
| 1 MiB, best path | 305 / 349 (s2_direct) | 160 / 230 (s2_hostcached) | 142 / 212 (s2_direct) |
| 1 MiB s2_direct | 305 / 349 | 207 / 278 | 142 / 212 |
| 1 MiB s1_copy | 352 / 442 | 312 / 412 | 312 / 408 |
| 16 MiB s2_direct | 842 / 1814 | 2395 / 2650 | 1392 / 1684 |
| 16 MiB s1_copy | 1587 / 3001 | 3940 / 4437 | 3983 / 4583 |
| g2c / c2g p50, empty | 38 / 179 (c2g multimodal: ≈ 50, 155, 235) | 31 / 29 | 32 / 28 |
| raw copy 16 MiB | 55–58 GB/s | 12.4 / 13.3 GB/s (gen3 x16) | 12.6 / 13.2 GB/s |
| run-to-run (cells ≥ 10 %) | 26 of 52 → FAIL | 0 of 124 → PASS | 0 of 100 → PASS |

- **Go criterion (tool output, both reports): NO-GO.** Empty: M5 213.5 vs 60.4 µs (noise margin ±24.9 %). 1 MB: M5 305.5 vs
  159.8 µs ReBAR on (±2.6 %) and vs 142.3 µs ReBAR off (±1.9 %). The M5 is ≈ 2–3.5× *slower* on both cells, not lower.
- **Where the M5 wins:** large payloads. At 16 MiB, s2_direct is 842 vs 1392–2395 µs and s1_copy 1587 vs ≈ 3950 µs, and raw copy
  bandwidth is ≈ 4.5× higher. The PCIe transfer cost grows with payload, but the M5's is small. Payload cost on the M5 is
  nearly flat up to 1 MiB.
- **Where the M5 loses:** the sync hand-off. On the PC it is ≈ 30 µs each way and stable. On the M5, CPU→GPU is multimodal
  (≈ 45 / 155 / 235 µs, the GPU's power state, finding 1), and that makes the Mac's run-to-run check fail.
- **Hot GPU (finding 7, probe, not part of the protocol):** with the GPU kept busy, the M5 empty round trip is ≈ 53–57 µs and
  stable (within 2 % run to run). That is ≈ 10 % below the 3060 Ti's 60 µs, which is about the size of the PC's noise
  (worst primary cell 7.4 %). The hot 1 MiB case is unmeasured, and the 3060 Ti hot is unmeasured too (`m1_3060_ext`, to-do 4).
- **Still missing for M1 acceptance:** the 4060 Ti (to-do 8). It cannot turn a NO-GO into a GO.
- **Crossover (M1 data, best path, chain/spin):** 2.38 MiB vs 3060 Ti ReBAR on (runs 2.25–2.44), 2.98 MiB ReBAR off
  (2.78–3.06), ≈ 89k / 112k agents; H1′ predicted 2.0–2.6 MiB. `results/m1_crossover/`.

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
