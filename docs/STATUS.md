# Work status (resume here)

_Last updated 2026-10-20 16:56 WIB (session 13, closed by Main: idle agents shut down). Launch-context retest still unscheduled; nothing run this session._

## RESUME HERE (2026-10-20 16:56, session 13)
- **Nothing run, no device touched.** Fresh start + state review only.
- **Launch-context retest (to-do 1 below) still UNSCHEDULED.** The user said (≈ 11:40) they will tell this session when the
  Mac is free for a quick run. Agreed with the user: ≈ 8 min hands-off, lid open, on the 40 W hub; **BattleStation window
  may be minimized** (helps: its window redraws at ≈ 6–8 % CPU; the pty daemon keeps running, so the S condition is
  unchanged; the 05:45 test showed window visibility doesn't matter); other agents may stay **idle but running** (02:32 and
  05:45 had 9 idle sessions). Log "BattleStation minimized" in the run log. A Terminal.app window opens and closes itself
  (the user OK'd the plan; re-confirm the window when they give the go). On go: heads-up to Main (no builds/GPU jobs for 8 min)
  → full-wake + battery-current check → `lc2.sh` (command below). Avoid overlapping another project's Mac-driven device run
  (e.g. NeonSwarm drove POCO jobs from the Mac 11:43–≈12:15): check the load and ask that project first.
- **GitHub (ls-remote, 10-20 ≈ 11:35 WIB):** `pc/m0-m1` still c593742 (10-14), origin/main f97f48d, `mac/narrow-link` ec93327.
  **No 10-19 PC result was pushed**: ask the user / Main whether the PC run happened. Local `main` is 10 commits ahead
  of origin (paper + STATUS), unpushed until the user says so.
- **Open asks for the user:** (1) the 8-min Mac slot; (2) did the 10-19 PC run happen?; (3) M2 review; (4) push the local
  commits; (5) paper: AI-use declaration wording, title-page data, Zenodo DOI or not.

## 2026-10-20 07:4x (session 12)
- **Mac ext series NOT run; the M5 spin-wait slowdown is unexplained.** Two checks tonight, both via Main's slots
  (README + data: `results/m1_mac_40w_check/`, gitignored, not committed):
  1. 02:32 A/B, 40 W hub, 0 mA: FAILED (spin cells +100…+250 µs vs `m1_mac_ac`, which was measured **09-24**, not 10-13).
     It turned out to run in **macOS dark wake** (Main, from pmset's log).
  2. 05:45 display test, full wake, 12 runs: internal panel only and all displays asleep are both +170…+280 µs.
  **Ruled out:** code (old = new binary), charger (20 W and 40 W both slow), ultrawide/display activity, dark wake.
  The 10-17 runs were in full wake too. Block-wait cells match or beat 09-24; only spin-wait is slow. In spin mode the
  CPU's 1 MiB read is 3× slower (45 vs 14 µs) and the GPU 1 MiB copy 3× (65 vs 21 µs).
- **Lead (not proven): launch context.** Since 10-16 19:26 every session runs under BattleStation's pty daemon (node,
  parent launchd); 09-24 predates it. Lightbound sets no thread QoS. Nice is ruled out: 05:58 session runs at nice 5 were as
  slow as nice 0. **The 05:58 Terminal-vs-session test was INCONCLUSIVE (my driver bug: empty window id, no Terminal runs).**
  **Retest:** the 07:57 slot was **cancelled at 07:33** (the user is back on the MacBook); Main asks the user for a new 8-min
  slot (the run needs ≈ 6 min). Nothing on the Mac before Main's message. Driver ready in the scratchpad and in
  `results/m1_mac_40w_check/launch_0558/lc2.sh` (full-wake gate before the window opens and before every run, since at 07:32,
  after RTBench's caffeinate ended, systemstate showed only "CPU Network" = dark wake). The session-12 scratchpad is gone
  after the 07:36 shutdown, so run the saved copy: `LC_SP=<new scratchpad> LC_END='YYYY-mm-dd HH:MM:SS' caffeinate -is
  results/m1_mac_40w_check/launch_0558/lc2.sh` (LC_END = slot end minus ≈ 40 s). Everything needed is in `results/m1_mac_40w_check/`. RTBench launches from
  Terminal.app tonight, which gives a second data point. If Terminal is fast, protocol §2 must name the launch context (or the
  bench sets its own QoS: a code change, so ask the user first). If not, ask the user what else changed between 09-24 and 10-17.
- **Side effect:** my 03:20 display pre-check put the ultrawide into standby (DP link down); left for the user (mouse touch
  or the monitor's power button). It also ran outside my slot while NeonReactor's series was armed: no pre-checks outside slots.
- **PC run of 10-19 afternoon:** outcome not recorded in this session; check GitHub / ask Main.
- **Z Fold 8 (borrowed from 10-20):** told Main "no gain" for Lightbound (no Android build; not paper-final).

## 2026-10-19 (previous resume point)
- **PC run today (Mon 10-19) in the afternoon, with the user attending** (Main, 00:40 WIB). Order on the PC:
  **Lightbound goes first** and holds `C:\dev\GPU_HOLD.txt` for the whole run, **then NeonTide (≈ 1.5 h), then
  NeonSwarm training**. Lightbound's run = `m1_3060_ext` (≈ 3 h) + the MSVC golden-hash check for M2, as in
  `docs/PC_MESSAGE_2026-10-13.md` (10-17 update: create the hold file → wait ≤ 10 min for no python in nvidia-smi →
  build/ctest → 10-min cooldown → measure → push → delete the hold file). The PC fetches `main` from GitHub
  (origin/main has the PC message; the local commits since are paper/STATUS only, so the PC doesn't need them).
  When Main asks, send the exact lines the user gives the PC Claude. Tell Main and NeonTide when the hold file is deleted.
- **Mac ext run still waits for ≥ 35 W.** Candidate sources: a HAGIBIS PD hub (maybe this evening, 10-19) or the Anker
  100 W power bank. Check both preconditions before starting (adapter ≥ 35 W in `pmset -g ac` and battery current
  ≥ −100 mA), then the 2-min A/B check first (see 10-17 below), and book a quiet-Mac slot through Main.
- **Needs the user** (unchanged from 10-18 evening below): AI-use declaration wording, title-page data, Zenodo DOI or
  not, push of the local commits since f97f48d (paper + STATUS), M2 review.

## 2026-10-18 evening (user AFK 18:15–≈20:20 WIB, blanket OK via Main: agent-only work, own recommendation on reversible choices)
- **Done:** paper to-do "journal limits + template". The journal's rules are in `docs/paper/OUTLINE.md` "Submission
  requirements". The abstract (draft 1, 143/150 words with placeholders) and 6 keywords are in `docs/paper/abstract.md`.
  Fig. 1 now meets the figure rules (174 mm wide, Arial embedded, text ≥ 8 pt, "Fig. 1" caption).
  BibTeX for all 18 related-work sources: `docs/paper/refs.bib` (Crossref/arXiv). Two fixes to the §2 table came out of it:
  Lumpp21 has three authors (H. D. Patel was missing), and Jarzabek17's first name is Łukasz.
- **Decided by agent under the user's 18:15 blanket OK** (all reversible):
  1. The paper stays in Markdown until the results sections exist. It moves to the Springer Nature LaTeX template
     (`sn-jnl`, flat folder) at the polish pass. There is no TeX on the Mac; Overleaf or a local install is the user's call then.
  2. Keywords: Shared physical memory · CPU–GPU synchronization · Round-trip latency · Heterogeneous computing ·
     Apple silicon · Agent-based simulation.
  3. Fig. 1's legend and the §5 table say "shared memory" instead of "unified", matching §1's terminology.
- **Needs the user:** (a) **AI-use declaration.** Springer Nature requires AI use beyond copy-editing to be
  declared in the Methods section, and this draft is written with an AI assistant, so the user decides how to
  declare it. (b) Title-page data: authors, affiliation, phone. (c) Data/code availability: the repo is public; whether
  to archive it with a DOI (e.g. Zenodo) is the user's call. (d) Push the paper commits (still local).

## 2026-10-18 morning
- **PC run HELD by the user until Mon 10-19** (at the office). Nothing started on the PC. When it goes ahead: the PC
  fetches `main` from GitHub (pushed 10-17, origin/main = f97f48d; the Desktop bundle @ 559c377 is the fallback) and
  follows `docs/PC_MESSAGE_2026-10-13.md`, whose 10-17 update wraps every step in the hold file `C:\dev\GPU_HOLD.txt`
  (the PC is shared with NeonSwarm training). Main asked for the exact lines the user should give the PC Claude; the
  request was cancelled 10-17, so re-send them when Main asks again on 10-19.
- **Mac ext run still waits for a 35 W+ charger** (unchanged, see the 10-17 item below).
  Precondition per LAB.md (10-18, RTBench's measurement): adapter ≥ 35 W in `pmset -g ac` **and** battery current
  ≥ −100 mA (`ioreg -rn AppleSmartBattery | grep -i amperage`). The 20 W adapter can show "charging" while the battery drains.
  Log both in the run log.
- **Paper: target journal = The Journal of Supercomputing** (user, 10-17). §1 rewritten for HPC readers (draft 2):
  tightly coupled CPU–GPU nodes, the game presented as a real-time agent simulation, "shared physical memory" vs CUDA
  unified memory. §2 now 18 sources (+ Grace Hopper ICPP'24 and three J. Supercomputing CUDA-UM papers); Velicka25/26 DOIs
  verified. Shortlist, precedents and reasoning in `docs/paper/OUTLINE.md`. Journal limits, abstract,
  keywords and refs.bib: done 10-18 evening (above). Paper commits after f97f48d are **local, not pushed** (no push
  until the user says so).

## Earlier (2026-10-16 / 10-17)
- **M1 decided: NO-GO on H1, re-scoped to the payload crossover (H1′)**, all as recommended (DECISIONS #33). H1′ with its
  predictions, the M1 outcome, the gen3 platform definition, the ReBAR reporting rule and the extension (hot GPU + 12-point
  payload grid) are now in `docs/06_RESEARCH_PROTOCOL.md` §1, §2 and §6. The proposal and the PC message carry dated
  "approved / update" notes.
- **Windows Update pause on the PC: extended by the user (10-16).** The PC Claude checks that the driver is still 566.36 before
  its next run.
- **4060 Ti: not before November** (the user can't do it sooner). Step D and M1's "[PC with 4060 Ti]" acceptance line wait;
  accepted deviation (#33).
- **Paper started (user's request via Main, 10-16):** `docs/paper/OUTLINE.md` (section readiness, venue open = user's call) and
  `docs/paper/methods_microbench.md` (§4 draft). Next in the draft order: §3 platforms + §5 M1 idle results, then related work after
  a real literature search; §6 only once both `_ext` series are in.
- **M2 built on the Mac** (see the M2 section below); waiting for the [PC] golden check and the user's review.
  M2 report given to the user 10-16 ≈ 19:00 WIB (PNGs in `docs/img/floors/`); M3 starts only after the user accepts.
- **Mac ext run NOT done (10-17 06:44–07:05 WIB, 20 W adapter, user-approved).** Attempt 1 stopped (Afterglow job, load 16),
  attempt 2 stopped (Chrome animating, GPU 17–30 % busy). Attempt 3 (Chrome quit, GPU 0 %, load < 2, thermal nominal) was
  stopped too: idle round trips are systematically ≈ +200 µs vs 10-13 (s1_copy 4K 496 vs 289 µs, 1M 577 vs 351; p99 tight,
  so not noise). **The code is ruled out:** the 10-13 commit 055d696, rebuilt into `build/ref_055d696/`, gives the same numbers
  as the frozen binary run back to back (s1_copy 4K 493/502 vs 497/504 µs). Prime suspect: the 20 W adapter (the M5 power
  budget). Partial data set aside in the scratchpad, not used. **Next:** with a 35 W+ charger, first run the 2-min A/B
  check (`--micro-paths=empty,s1_copy,s2_direct --micro-payloads=4K,1M`, both binaries); start `m1_mac_ext` only if it is
  back at the 10-13 levels. Launcher: scratchpad `run_mac_ext.sh` (now `caffeinate -dis`: protocol §2 wants the display on).
- **PC:** still at c593742 on GitHub (checked 10-16 08:31 WIB, `ls-remote` + GitHub API: last push 10-14 16:58 WIB). The user
  said "the PC says the new result is on GitHub", but nothing new had arrived. Ask the user for the PC's `git status -sb` /
  `git log --oneline -3 origin/pc/m0-m1` if it comes up again.

## M2 — procedural floors (built on the Mac 2026-10-16; [PC] golden check pending)
- **Built:** `src/procgen/` (stages 1–6, validation, metrics, `floorHash`, debug image), `--mode=procgen-dump --seed=N
  --depth=D --out=x.png`, `--depth` flag, stb pinned. Interpretations: **DECISIONS #34**.
- **Verified [Mac], Release, Metal build:** ctest 45/45 without the GPU label (the 2 GPU smoke tests not run: light-work
  rule); warnings-as-errors clean.
  - determinism seeds 1..200 ✓; validity seeds 1..1000 ✓ (at most 3 attempts);
  - golden hashes for the bench seeds × depths 1..5 written to `tests/golden/floors.txt`;
  - 200×200 generation: best of 3 per bench seed well under 150 ms (single attempt ≈ 26–35 ms; worst seen with
    re-rolls 84 ms);
  - seal puzzles on seeds 1..40 (d3/d5): scrambled, solvable in 1..8 rotations, the stored solution solves;
  - SDF vs brute force: exact on > 99.9 % of samples, worst < 1 px.
- **Review PNGs:** `docs/img/floors/` (bench seeds at d5 = 200×200, and seed 101 at d1–d4 for each biome).
- **Open for acceptance:** [PC] golden hashes identical with MSVC (in the PC message; runs when the PC gets the bundle).
  Calibration stats (seeds 1..200 per depth): open fraction p50 21.4 → 23.5 %, re-rolls ≈ 7 % of floors (rule 5 dark
  stretches, puzzle search, rule 6).

## To-do (2026-10-16, in order)
Tags: **[solo]** = this session, unattended; **[device: X, ~time]** = needs a machine; **[user]** = the human's hands or a decision.
No phones or tablets are needed.

1. **[user] Review M2** (`docs/img/floors/*.png`, DECISIONS #34). M2's [PC] check rides along with to-do 3. M3 (headless
   simulation core) is next after M2 is accepted.
2. **[device: Mac on AC, ≈ 70 min, unattended, Mac otherwise quiet]** extension series `results/m1_mac_ext` (idle + hot,
   12 payloads; command in `docs/M1_RESCOPE_PROPOSAL.md` §4): charger, lid open, Low Power Mode off, `caffeinate -is`; no
   other session may run GPU work, builds or heavy jobs meanwhile. Needs a slot from Main when the Mac is quiet (evening/night,
   not during another project's quiet-Mac run; Main queued it for 10-16 evening after RTBench, exact time from Main). M2 work
   pauses during it. **Use the frozen binary** `build/frozen_ext_2d5c1ba/lightbound` (+ its `shaders/`; Metal Release, clean
   commit 2d5c1ba, code = 069a95e) as `--exe`, so M2 edits can't change or dirty the measured binary.
   Provenance: built 10-16 08:37 WIB, before Xcode auto-updated to 27.1 at 10:14 (Main's notice); `xcrun metal` is
   32023.921 before and after, the OS is unchanged, and the cache pins no SDK path. Builds after 10:14 use Apple clang 21.0.0
   (Xcode 27.1); a plain incremental build still works. Record both facts in the run log.
3. **[user]** carry `~/Desktop/lightbound-main-2026-10-13.bundle` (rebuilt 10-16 at 364ffbf: decisions + M2) and
   `docs/PC_MESSAGE_2026-10-13.md` to the PC and start the PC Claude on the message. (Bundle rebuilt again 10-17 for
   the hold-file update below; the 10-16 copy is outdated.) **Since 10-17 20:3x WIB main is on GitHub (559c377)**, so the
   PC can `git fetch origin main` instead; the bundle is the fallback. **The user halted the PC run until Mon 10-19**
   (at the office).
4. **[device: Windows PC, ≈ 3 h, unattended]** the same series on the 3060 Ti (`results\m1_3060_ext`), after the PC Claude
   fast-forwards and rebuilds. Nothing on the PC during the first ≈ 4 min. **The PC is shared with NeonSwarm training
   since 10-17** (Main): every PC step is wrapped in the hold file `C:\dev\GPU_HOLD.txt` (create → wait ≤ 10 min for no
   python in nvidia-smi → build/ctest → 10-min cooldown → measure → push → delete); the PC message's 10-17 update has the
   steps, and the run log records whether training ran that day.
5. **[solo, ≈ 1 h]** after 2 and 4: fetch, report both with `micro_report.py` (`gpuload` column), compute the crossover per GPU
   state with its run-to-run spread, compare with the H1′ predictions.
6. **4060 Ti step D, November** (the user's date). **[user]** installs it in the i7-12700F PC, same slot (protocol §2; if it goes
   into another PC, ask before planning). **[device: Windows PC, exclusive, ≈ 3 days unattended]** step D as written
   (`results\m1_4060`), then optionally `m1_4060_ext`. **[solo, ≈ 1 h]** merge and add P-4060 to the combined report.
7. ~~Pushing~~ **done 10-17 evening**: SSH port 22 worked again (Main checked 20:29 WIB); pushed main baf605a..559c377 and
   `mac/narrow-link` (new branch on origin).

## State

- **M0 — Scaffold: accepted** on the Mac (Metal + MoltenVK) and on the PC (all five [PC] checks pass, `docs/PC_REPORT.md` §2).
  `pc/m0-m1` was merged into `main` on 2026-10-13 (45b6182). Both Mac backends rebuilt warning-free; ctest 37/37 on each.
- **M1 — Round-trip microbenchmark (GO/NO-GO gate):** the Mac (AC) and the RTX 3060 Ti (ReBAR on and off) are measured.
  The combined reports are `results/m1_report` (3060 Ti ReBAR on, the primary condition) and `results/m1_report_rebar-off`.
  Both say **NO-GO** by protocol §6. **Decided 2026-10-16: NO-GO on H1, re-scoped to H1′ (DECISIONS #33).** Acceptance still lacks the 4060 Ti
  (November, accepted deviation). M2 may start. The hot-GPU cells (DECISIONS #32) are built, off by default, unmeasured.
- **PC:** idle since 2026-10-13 10:30 (still idle 10-14, c593742, merged). All its work is merged: `docs/PC_REPORT.md` is the hand-over, and `docs/PC_PROGRESS.md`
  is the session log with the resume procedure. Step D (4060 Ti) waits for the card. The PC Claude must be told it is coming
  (to-do 5).
- **Open human decisions:** none (M1 decided 2026-10-16).

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

## PC results (merged 2026-10-13)
`docs/PC_REPORT.md` is the hand-over (environment, M0 checks, source changes, all tables), and `docs/PC_PROGRESS.md` is the
full session log, both now on `main`. DECISIONS #27–#31 came with the merge. Key facts:
- **Primary series** `results/m1_3060` (= `results/m1_pc_report`): 3060 Ti, ReBAR on, gen3 x16, driver 566.36, binary 6962eff,
  full protocol including `s2_rebar`, 3 × 23.8 h. Run 1 was redone 10-12/13 because the PC's own git push disturbed two
  cells of the first one (`results/m1_3060_run1_disturbed`, never pool it). Run-to-run **PASS** (124 cells, worst 8.4 %).
- **ReBAR off** `results/m1_3060_rebar-off`: all paths except `s2_rebar`, 3 × 52 min, run-to-run PASS (worst 9.6 %). Its own
  report, never pooled with ReBAR on. `results/m1_3060_rebar-off_old-calibration` (before #30) has valid `rt` only.
- **ReBAR changes only `s2_direct`:** the CPU→GPU buffer sits in VRAM, so the CPU write over PCIe is ≈ 3× slower (1 MiB
  `cpu_write` 96 vs 33 µs → rt 207 vs 142 µs). The GPU read pass, which rt excludes, becomes almost free (7 vs 86 µs).
  Every other path agrees within a few percent.
- **Checks on the Mac after the merge (2026-10-13):**
  - **#30** (clock drift) on `results/m1_mac_ac`, comparing the g2c median of the first vs last 50 iterations of each
    500-iteration block: no drift. The sign is split 50/50 over 780 blocks, the median |Δ| is 1.9 µs, the empty-path mean
    Δ is +0.5 µs, and there are 0 negative g2c samples. The mean Δ of −1.1 µs comes only from noisy 16 MiB cells (median +0.4).
    The same script on the PC's old-calibration series shows the drift: Δ < 0 in 98 % of blocks, 83 989 negative g2c samples.
    The Mac data stands.
  - **#31** (`.csv` + `.csv.gz` counted twice): no Mac report directory had both. The Mac AC CSVs are now gzipped next to the
    raw files, and a rebuilt Mac report still counts 3 runs with an identical `summary.csv`.

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
  (worst primary cell 7.4 %). The hot 1 MiB case is unmeasured, and the 3060 Ti hot is unmeasured too (to-do 2a).
- **Still missing for M1 acceptance:** the 4060 Ti (to-do 5). It cannot turn a NO-GO into a GO.

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
- **Hot-GPU cells (2026-10-13, DECISIONS #32):** `--micro-gpuload=idle|hot|both`; `Gfx::loadDispatch/loadWait` on a second
  queue in both backends, shared pacing thread + calibration in `src/bench/MicroLoad.*`, `micro_burn` in
  `shaders/microbench.slang`, cells written as `<path>_hot` with `# cell_load:` duty notes, report support, 3 new tests
  (ctest 40/40 on Metal and MoltenVK).

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

9. **Payload crossover (analysis of existing data, 2026-10-13, at the human's request "moving data is the strength, evaluate
   and exploit it").** Chain/spin p50, `results/m1_report*/summary.csv`. Per round trip, the CPU's read + write of the payload
   costs on the M5 4K…16M = 0 / 2 / 7 / 30 / 124 / 647 µs, and on the 3060 Ti (ReBAR off, s2_direct) 1 / 6 / 21 / 82 / 322 / 1300 µs.
   The M5's whole deficit is the fixed hand-off cost (cold c2g ≈ 170–230 µs at small payloads).
   - **Cold M5 (as measured):** the round trip crosses below the 3060 Ti at ≈ **2.0 MiB** (vs ReBAR-on s2_hostcached) to
     **2.6 MiB** (vs ReBAR-off s2_direct) per direction, by log-log interpolation between the 1M and 4M points. No point was measured
     in between.
   - **Hot M5 (estimate, not measured):** the probe's hot hand-offs (g2c 23 + c2g 31 µs), plus the measured payload growth of g2c,
     plus the measured CPU read + write give 54 / 60 / 73 / 108 / 215 / 743 µs. Against the 3060 Ti's 59 / 65 / 80 / 142 / 388 / 1392 µs,
     that is ≈ 1.1× faster up to 256K (within noise) and **≈ 1.3× at 1 MiB, ≈ 1.8× at 4–16 MiB**. This must be measured
     before it is used (hot-GPU cell).
   - **In game terms:** round trip 1 moves ≈ 36 B/agent GPU→CPU (Perception 32 + SortedIds 4, plus CellRange and events) and
     ≈ 16–20 B/agent CPU→GPU (Intent 16, plus RefineRequest ≤ 4 at K ≥ 2). With ≈ 28 B/agent per direction on average, 1 MiB ≈ 37k
     agents and the cold crossover ≈ 75k–100k agents. The planned N levels go to 100k, and capacity is 131,072. A per-round-trip
     win is paid K times per frame, which is the H3 mechanism.
   - **What it means:** H1 as written ("lower for all payload sizes") is refuted by M1. The data instead shows the crossover
     the protocol already calls the expected honest outcome (§1: "a crossover (H2 + H3), not a universal win") and supports
     H5's direction. Any exploitation must stay within R1: the same pipeline on both backends. The lever is the workload (N,
     bytes per agent, K), not an M5-only optimization.

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

### B. PC (Windows 11) — M0 checks + M1 runs in one session — 3060 Ti DONE and merged 2026-10-13; 4060 Ti (step D) waits for the card
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

### C. Combine and decide — DONE 2026-10-13 for the 3060 Ti (see "M1 combined report")
```
python3 tools/analysis/micro_report.py results/m1_mac_ac results/m1_3060 --out results/m1_report
python3 tools/analysis/micro_report.py results/m1_mac_ac results/m1_3060_rebar-off --out results/m1_report_rebar-off
```
After step D add `results/m1_4060` to the first command. Keep ReBAR-off runs in their own report (runs of one platform are pooled).

## Housekeeping
- Git: repository initialised 2026-09-24 on `main` (local identity jostarigan), remote
  `git@github.com:jttarigan/Lightbound.git` (Mac pushes with a deploy key over SSH). On 2026-10-01 SSH
  to github.com timed out from the Mac's network (ports 22 and 443). HTTPS fetch works (see the
  resume checklist), but pushing needs SSH. **Local `main` is ahead of `origin/main` (baf605a)** with
  every commit since session 3 and the 2026-10-13 merge of `pc/m0-m1`, all unpushed. **SSH still timed out on 2026-10-06 (ports 22 and 443,
  ssh.github.com), on 2026-10-12 and on 2026-10-13 (port 22).** Code for the PC goes over as a git bundle instead (finding 8). Local branch
  `mac/narrow-link` is unpushed too. When SSH works, push both: `git push origin main mac/narrow-link`. The PC branch doesn't depend on them. The preliminary
  `results/m1_prelim` runs predate the repo (their CSVs say `nogit`).
- **Bundle for the PC (2026-10-13):** `~/Desktop/lightbound-main-2026-10-13.bundle` holds branch `main` (the merge of
  `pc/m0-m1` plus the hot-GPU cells and the PC message). On the PC: `git fetch <bundle> main:mac-main`, then
  `git merge --ff-only mac-main` on `pc/m0-m1` (it is an ancestor of `main`), only between series. It supersedes the
  narrow-link bundle, which is needed only if the 4060 Ti never returns.
- Build dirs: `build/` (Metal), `build-vk/` (Vulkan via MoltenVK). Toolchain PATH:
  `export PATH="$HOME/Library/Python/3.9/bin:$PATH"`. Python analysis deps are installed (user site).
- Preliminary data: `results/m1_prelim/` (6 Metal + 3 MoltenVK runs, battery) — not valid for the study.
