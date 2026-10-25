# 40 W hub A/B check (2026-10-20 02:32–02:35 WIB, Mac, not a measurement series)

Diagnostic only; never pool with `m1_mac_ac` or `m1_mac_ext` (DECISIONS #35). Gate for `m1_mac_ext`: FAILED, so the
ext series was not started.

- Conditions: HAGIBIS hub with a 45 W charger, `pmset -g ac` 40 W (20 V / 2 A); battery 80 % at the 80 % charge limit,
  0 mA throughout (`power_log.csv`, one line per minute); Low Power Mode off; thermal nominal, no warnings; 1-min load
  1.2–2.4 (9 idle Claude sessions, Terminal, the BattleStation Electron app ≈ 6–8 % CPU, Chrome open but idle);
  GPU utilization 0–3 % before the runs. One `caffeinate -dis` held for the whole check. macOS 26.6.2 (25G83), unchanged.
  **External display attached:** U34G3G3R3, 3440×1440 @ 165 Hz, plus the internal panel.
- `ab_{old,new}_{1,2}.csv.gz`, run alternately old, new, old, new (≈ 28 s each): idle cells, paths empty/s1_copy/s2_direct,
  payloads 4K and 1M. old = `build/ref_055d696/` (the `m1_mac_ac` commit, sha256 a92926f9…), new = the frozen ext binary
  `build/frozen_ext_2d5c1ba/` (sha256 de1575eb…).
- Reference: `m1_mac_ac`, measured 2026-09-24 (thermal "fair"; the 10-13 date in older notes is the combined report).
- Result (`ab_compare.txt`, rt p50 against the median of the 3 reference runs): old and new agree again. **Every spin-wait
  cell is +100 to +250 µs slower** (s1_copy chain/spin 4K 327–479 vs 291 µs, 1M 564–586 vs 352 µs). Block-wait cells match
  at 4K (±10 µs) and are ≈ 100–140 µs *faster* at 1M. Same pattern as the 20 W check of 10-17 (`m1_mac_20w_check`), so the
  20 W adapter was not the cause.
- ~~Open lead: the 165 Hz ultrawide~~ ruled out at 05:45 (below). **Also: this 02:32 check ran in macOS dark wake**
  (pmset log: DarkWake from Deep Idle 01:23, FullWake only at 03:17, found by Main/NeonReactor).

## Display / wake test (2026-10-20 05:45–05:55 WIB, `display_0545/`, slot from Main, user GO via Main)

- 12 alternating runs of the same cells, frozen binary, full wake throughout (`pmset -g systemstate` CPU+Graphics+Audio+Network,
  power state 4, logged before/after each run). The ultrawide was offline (a 03:20 sleep/wake pre-check put it in standby), so
  "awake" = internal panel only. Run 1 was meant awake but the panel stayed asleep: counted as asleep. 40 W, 0 mA, thermal nominal,
  load 1.4–3.3 (corespotlightd 284 % once, during run 3). Lock-screen aerial wallpaper drew during the awake runs.
- Result (`compare.txt`): spin-wait cells vs 09-24 are +172…+254 µs (internal only awake) and +205…+282 µs (all asleep),
  as slow as the 02:32 dark-wake runs. **Display activity, the ultrawide and dark wake are ruled out.** The 10-17 20 W runs were
  in full wake too (`pmset_sleep_wake_10-17_to_10-20.txt`: FullWake 06:36:37, nothing until after 07:05), so the slow state
  has held since at least 10-17 in every condition tried. pmset's log does not reach 09-24.
- Legs (s1_copy chain): in spin mode the CPU's own 1 MiB read is 45 µs (09-24: 14) and the GPU 1 MiB copy 65 µs (21);
  in block mode both match or beat 09-24. So the spinning bench thread runs at an E-core-like rate and its GPU work slower too.
- New lead, not proven: launch context. Since 10-16 19:26 the sessions run under BattleStation's pty daemon (`node`, parent
  launchd); 09-24 predates it. Lightbound sets no thread QoS, so it inherits the context. Test: alternate launches from a
  session and from a Terminal.app window.

## Launch-context attempt (2026-10-20 05:58–06:07 WIB, `launch_0558/`, slot from Main): INCONCLUSIVE

- Driver bug: the osascript that opened the Terminal window returned an empty id (the check only caught "-1"), so all 4
  Terminal-launched runs failed ("Can't get tab 1 of window") and none ran. The window (id 2324, ttys020, shell started
  05:58:45) was found by its tty start time and closed; Terminal kept an invisible 0-tab object 2324, tty gone.
- The 4 session-launched runs ran at **nice 5** (zsh's BG_NICE for `&` jobs; the 02:32/05:45 runs were foreground, nice 0,
  priority 31). They are as slow as the nice-0 runs (spin cells 428–570 vs 450–585 µs; 09-24: 201–352): nice is irrelevant.
- `lc2.sh` is the fixed driver for the retest: window matched by tty and a numeric id check, an exit trap that closes only
  that window, NO_BG_NICE, a full-wake gate (before the window opens and before every run; tested offline: "CPU Network" and an empty
  pmset output both stop it), and per-run `taskinfo` (+ `--threads`) dumps for the task role/QoS.
- **`lc3.sh` (2026-10-21) replaces `lc2.sh` for the retest.** `taskinfo` needs root, so v2's `ti_*.txt` dumps would only have
  said "must be run as root". v3 runs every bench under `tools/rusage_wrap.py`, which reads the bench's exit rusage
  (RUSAGE_INFO_V6, no root): P-core share of CPU time, effective GHz on P- and E-cores, QoS breakdown → `ru_<i>_<c>.json`,
  summarized in `lc.log`. Offline check (10-21, Mac busy, load ≈ 3.8, not a measurement): a 1-s shell busy loop launched from a
  session ran 74–99 % on P-cores at ≈ 3.3 GHz, QoS legacy; the session tree (node daemon, claude, zsh) is QoS legacy at nice 5.
  So a session-launched CPU-bound process does reach the P-cores; whether the bench's *spinning* thread does is what v3 records.
- Not committed (public repo): `display_0545/pmset_sleep_wake_10-17_to_10-20.txt` (the Mac's sleep/wake history); it stays local.

## Launch-context retest (2026-10-21 12:53–12:58 WIB, `launch_1253/`, Main's slot, user's go via Main): launch context RULED OUT

- Driver `launch_0558/go.sh` → `pre.sh` gate (35 W Apple adapter, 80 % at 0 mA, full wake, GPU ≤ 5 %, load < 3; the first
  check at 12:53:29 failed on GPU 6 % after TopDownBoxy's run, the second at 12:53:49 passed: GPU 1 %, load 1.56) → `lc3.sh`
  (frozen binary, cells empty/s1_copy/s2_direct × 4K/1M, alternating S T T S S T T S, ≈ 29 s each) → postcheck. Full wake
  before every run, load 1.5–2.5, thermal nominal. Ultrawide G34w-30 3440×1440 @ 165 Hz + internal panel online; Music playing
  (user's, ≈ 9 % + coreaudiod 8 %); Main held `caffeinate -dis`; NPUBench's co-sim SIGSTOPped, other agents idle.
  S runs at nice 5 (daemon tree), T runs at nice 0 (Terminal window 1196, closed afterwards).
- Result (`compare.txt`, median over 4 runs of per-run p50): **T is as slow as S.** Spin cells vs 09-24: S +111…+262 µs,
  T +109…+243 µs; T − S within ±23 µs in 11 of 12 spin cells (s1_copy perpass 4K: −114, one multimodal cell). Block cells
  match 09-24 at 4K (±5 µs) and beat it at 1M (−80…−133 µs), as on 10-20. Nice 5 vs 0: irrelevant again.
- Exit rusage (`ru_*.json`, whole process, all threads): 68–75 % of CPU time on P-cores in 7 runs, 90 % in run 7T;
  P-cores ≈ 2.8–3.0 GHz, E-cores ≈ 1.6–1.9 GHz; QoS legacy 100 % in both contexts. Run 7T had the fastest empty spin rt
  (299 vs 434–471 µs; 09-24: 249): across the 8 runs r(P-core share, empty spin rt) = −0.89 (s1_copy 1M spin −0.70, 4K −0.40).
  In the same runs the CPU's 1 MiB read takes 32–45 µs in spin mode and 15–16 µs in block mode. Consistent with the spinning
  bench thread spending part of its time on E-cores, but the process-level counters can't attribute time to that thread.
- Also noted: 09-24 runs started at thermal state "fair", all checks since at "nominal". Same macOS build 25G83 and driver.
- Next (needs the user): what else changed on the Mac between 09-24 and 10-17? Optional code-level test (a code change, so ask
  first): the bench thread sets QOS_CLASS_USER_INTERACTIVE, or logs per-thread counters, to see whether P-core placement
  restores the 09-24 spin levels.

### 10-22: the code test is built (DECISIONS #36); the retest waits for the user's go
- Probe in this session's process tree (nice 5): `pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE)` succeeds, but the
  kernel caps a command-line process at priority 31 on every thread kind (main thread, `pthread_attr` QoS thread, `std::thread`);
  lowering works (BACKGROUND → 4). `THREAD_PRECEDENCE_POLICY` works without root (+16 → 42 = task priority 26 (31 − nice 5) + 16;
  63 cap). So a QoS request alone cannot test the P-core lead.
- `lightbound --micro-qos=high` = the QoS request plus base priority 47 by precedence (what a user-interactive thread has in an
  app). Smoke run 10-22: `# sysinfo.thread_priority: cur=47 base=47`. Every CSV header now records `sysinfo.thread_qos`,
  `sysinfo.thread_priority` and `sysinfo.process_nice`, so a run always says what its thread ran as.
- Retest driver `launch_0558/lc4_qos.sh`: D H H D D H H D (default / high), all launched from the session (the launch context is
  ruled out), `build/lightbound` (the frozen ext binary predates the flag), rusage per run, the header's qos/priority lines logged.
  Gates: full wake, AC ≥ 35 W. Needs the user's go and a Main slot (≈ 8 min). Compare against 09-24 with `ab_compare.py`.

## QoS/priority retest (2026-10-22 19:48:00–19:52:10 WIB, `launch_1948/`, Main's quiet-Mac slot, user's go via Main): QoS and priority RULED OUT; the lead moves to the SoC clock

- Driver `launch_0558/lc4_qos.sh` via `launch_1948/lc4_launch.sh` (epoch-timed start, environment log, own `caffeinate -is`):
  `build/lightbound` at 6cef8eb clean, cells empty/s1_copy/s2_direct × 4K/1M, D H H D D H H D (D = `--micro-qos=default`,
  H = `--micro-qos=high`), all launched from the session, 25–31 s each. Full wake before and after every run, 40 W through the
  hub, 80 % at 0 mA throughout, internal panel only (no ultrawide), HID = internal keyboard/trackpad + USB mouse dongle
  (Kanata S wireless) + Bluetooth headset; Mac rebooted at 18:26; the user had left; other agents idle (Main). Process nice 0.
  The flag took effect: H headers read `thread_priority: cur=47 base=47`, `qos: high`; D headers `cur=31 base=31`.
- **Result 1 (`compare.txt`, `lc4_compare.py`, median over 4 runs each): H = D.** H − D is within ±12 µs on 16 of 20 cells.
  The 4 cells where D reads lower (empty chain spin −96, empty perpass spin −86, s1_copy perpass 4K −88, s2_direct perpass 4K
  −127) are the cells that sped up late in the series, and the two late D runs (5, 8) happen to be the fastest: the interleaved
  order shows a time trend, not a flag effect. P-core share did not move with the flag (H 0.55–0.65, D 0.56–0.69). So a
  user-interactive QoS request plus base priority 47 does **not** restore the 09-24 spin levels: scheduling class and priority
  are ruled out, and `--micro-qos=high` is not worth adopting (recommendation; DECISIONS #36 left the call to the user).
- **Result 2: the spin cells track the effective clock, and a background load lifted it mid-series.** Exit rusage per run:
  runs 1–3 (19:48:03–19:49:41) ran at 2.17–2.26 GHz process-wide (P-cores 2.6–2.8, E-cores 1.4–1.5), spin cells +170…+290 µs
  vs 09-24, as in every check since 10-17. From run 4 (19:49:45) on: 2.8–3.2 GHz (P 3.3–3.9, E 1.8–2.9), CPU time per run
  16.0–16.4 → 12.8–14.1 s, and the empty/4K spin cells fell to 09-24 levels or below (empty chain spin 101–130 µs in runs 6 and
  8 vs ref 249; s2_direct chain 4K 210–214 in runs 7–8 vs 201; s2_direct perpass 4K 109–196 vs 203). Across the 8 runs
  r(rt, GHz) = −0.81…−0.95 for the empty and 4K spin cells, r(rt, P-core share) only −0.52…+0.35: the clock explains the shift
  better than the core placement did on 10-21. The 1 MiB spin cells stayed +180…+230 µs in every run but one (run 6 H: s1_copy
  chain 1M 368 vs ref 352), so they carry a second component (the spin-mode 1 MiB read, 45 vs 14 µs on 09-24) that the clock
  alone does not remove. Block cells: 4K +1…+13 µs, 1M −62…−127 µs vs 09-24, as since 10-20.
  The 1-min load rose from 1.1–2.2 to 4.16 at 19:50:15 and stayed 3.4–3.9: Main's process log says macOS launched FPCKService
  (iCloud Drive sync, ≈ 50 % CPU over 4 min) at 19:50:00, with Spotlight/photoanalysisd bursts, post-reboot housekeeping; no
  agent ran anything. So **another process's load raised the SoC clock and the lone spinning bench thread profited.**
- Reading (not proven): on an idle Mac the governor leaves a single spin-waiting thread at ≈ 2.2 GHz effective; on 09-24 the Mac
  ran at thermal "fair", i.e. something else kept the clocks up then. That would make "what changed 09-24 → 10-17" the Mac's
  background state rather than a setting. The process-wide counters cannot show the bench thread's own clock; `powermetrics`
  (root) during a run would.
- Next (user's call, STATUS to-do 2): (c) a 2-min root `powermetrics --samplers cpu_power` check during two runs (idle, then
  with a companion load) to see the clock directly; then (a) report the shift and re-measure the idle reference in the present
  state, or (b) make a defined CPU-side load part of the microbench condition (the game never runs a lone spinning thread;
  docs/06 §2 changes and both platforms re-measure). Recommendation: (c) first, then (b) if it confirms.

## Clock check with root powermetrics (2026-10-25 07:08:21–07:11:30 WIB, `pm_1025_0708/`, Main's quiet-Mac slot, user's go and sudo waiter): the SoC clock is CONFIRMED as the spin-wait shift

- Driver `pm_check.sh` (one `caffeinate -is`): Main's shared root waiter (`labpm_go`) ran `powermetrics --samplers
  cpu_power,gpu_power -i 250 -n 720` (3 min); `build/lightbound` at 6cef8eb code (HEAD 44c6bfb, src clean), cells
  empty/s1_copy/s2_direct × 4K/1M, runs I L I L (≈ 29 s each; I = bench alone, L = bench + 2× `yes >/dev/null`), exit rusage
  per run. Full wake before every run, 40 W through the hub, 80 % at 0 mA throughout, load 1.5–2.9, internal panel. Analysis
  `pm_compare.py pm_1025_0708` → `compare.txt` (samples inside each run window, 2 s trimmed per end; M5 clusters = E (6) + S (4)).
- **Clock:** I runs: S-cluster 2.67–2.74 GHz, the busiest S-core 2.67–2.72 GHz (p10 1.9) at only ≈ 0.3 active: the spinning
  thread hops across the 4 S-cores (S-cluster total ≈ 0.8 cores busy), P-share 96–99 %, rusage 2.77–2.79 GHz. L runs: S-cluster
  4.26 GHz, busiest S-core 4.46 GHz (≈ 0.69 active, S total ≈ 2.4), rusage 4.13–4.14 GHz. GPU: lowest DVFS state 338 MHz and
  never idle in all four runs (idle ratio 0; 0.98 before, 0.84 after), so the GPU clock does not separate I from L.
- **rt p50 vs 09-24 (I / L):** spin cells +65…+268 / +4…+34 µs, except the 4K perpass/s2_direct spin cells in L at +80…+88. The
  1 MiB spin cells return to 09-24 under L too (+4…+20; on 10-22 at 2.8–3.2 GHz they had not). Block cells: I matches 09-24 at
  4K (±4) and beats it at 1M (−98…−144), as since 10-20; **L makes block cells slower** (4K +85…+96, 1M +6…+18, empty +15…+17).
- Reading: a lone spin-waiting thread on an otherwise idle Mac gets ≈ 2.7 GHz; with two busy companion threads the S-cluster
  runs ≈ 4.3–4.5 GHz and the spin cells return to the 09-24 levels. Neither state reproduces 09-24 whole (09-24 block 4K ≈ I,
  block 1M and spin ≈ L), consistent with 09-24 having run under some uncontrolled background load (thermal "fair"). The
  microbench result thus depends on the CPU-side load, which docs/06 §2 does not fix. Decision (a)/(b) → STATUS to-do 2.
