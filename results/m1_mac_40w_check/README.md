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
