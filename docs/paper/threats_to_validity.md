# §12 Threats to validity (partial draft, 2026-10-17; §12.2 revised 2026-10-23)

<!-- Sources: docs/06 §9 (the protocol's threat list), DECISIONS #17, #23, #30, #32, #33, #35,
     results/m1_report*/report.md, docs/PC_REPORT.md, results/m1_mac_20w_check/README.md (10-17 diagnostic),
     results/m1_mac_40w_check/README.md (10-20 diagnostics: 40 W A/B, display/wake test, launch-context attempt;
     10-21 launch-context retest; 10-22 QoS/priority retest, launch_1948/), DECISIONS #36, docs/STATUS.md 10-17 to 10-22. Game-side threats (bot workload, behaviour design, float equivalence) wait for M3–M8. -->

## 12.1 Platform and API differences

- **Different APIs.** The M5 runs Metal and the RTX cards run Vulkan, so an API's own overhead is part of every
  platform difference. Mitigations:
  - One Slang source per kernel, identical buffer layouts, and one algorithm per path (R1).
  - Each API's thinnest synchronization primitive: `MTLSharedEvent` and timeline semaphores.
  - A MoltenVK control on the M5, which runs Vulkan on top of Metal: optional, not yet run.

  We report the M5–RTX differences as differences between platforms (hardware plus API), not between memory
  architectures alone.
- **Different raw power.** The platforms differ in CPU, GPU and memory, not only in the link. We therefore report
  hand-off costs (`g2c`, `c2g`) and within-platform ablations (S1 vs S2, payload, GPU state) alongside absolute
  round trips.
- **PCIe gen3.** The PC's slot runs at gen3, not the gen4 the protocol planned. The measured link is the platform
  definition (DECISIONS #33). Gen4 results may differ, especially in the payload-dependent part.
- **ReBAR.** ReBAR moves the cost of the CPU→GPU direct path from the GPU's read into the CPU's write. We report
  `rt` as defined, plus `rt` + GPU read, and keep the ReBAR-off series separate (§5).
- **One device per class.** Each platform is a single machine: one M5 and one 3060 Ti, with the 4060 Ti pending.
  We claim nothing about variation between units of the same model.

## 12.2 Measurement hygiene on a shared, fanless laptop

The M5 is the author's daily machine, not a dedicated benchmark host. Its round trips are latency-bound and sit
close to the GPU's power-management decisions (§5), so they are far more sensitive to the environment than
bandwidth figures. We found five ways the environment can bias a series or cast doubt on one, each with evidence
from our own runs:

1. **Power source.** A series on battery was excluded, and the protocol requires the Mac to be plugged in.
   "Plugged in" is not a measurement, though: macOS reports "AC Power" on an under-powered 20 W adapter as well.
   Every series therefore logs the adapter's wattage (`pmset -g ac`) and the battery current, and starts only on a
   supply of at least 35 W with the battery neither charging nor draining. We first suspected the 20 W adapter of
   the shift in item 5; a check on a 40 W supply with the battery at 0 mA showed the same shift, so the adapter
   was not its cause.
   <!-- results/m1_mac_20w_check/README.md (20 W, 10-17); results/m1_mac_40w_check/README.md + power_log.csv
        (40 W, 0 mA, 10-20 02:32); DECISIONS #35. -->
2. **Other processes using the GPU.** A browser tab that animates kept the GPU 17–30 % busy (IOAccelerator
   "Device Utilization") with no benchmark running. Under that load, idle cells were 1.5–3× slower and
   erratic: `s1_copy` 1 MiB p50 525 vs 351 µs, p99 1780 vs 516 µs. Rule: no other GPU client; a series starts
   only once GPU utilization has stayed ≤ 5 % and the 1-minute load average below 3 for two minutes.
3. **CPU contention.** A concurrent 8-core analysis job (load average 14–16) inflated the p99 of the first cells
   about twofold. The same start rule catches it.
4. **Display and sleep state.** The protocol keeps the display on. Preventing only system sleep
   (`caffeinate -is`) lets the display sleep after 10 minutes, so runs use `caffeinate -dis`. A sleep-prevention
   assertion does not end a maintenance *dark wake* (display off, system partly awake), though, and one of our
   checks ran in a dark wake without our noticing. Runs now start only in full wake (`pmset -g systemstate`
   lists the graphics capability) and re-check it before every process. In that check, dark wake made no
   difference beyond item 5: full-wake runs three hours later were as slow.
   <!-- results/m1_mac_40w_check/README.md: 02:32 check in DarkWake (pmset log), 05:45 full-wake test. -->
5. **A persistent shift, traced to the processor clock on an idle machine.** Since at least 2026-10-17, every check
   on the M5 has measured *spin-wait* round trips ≈ 100–280 µs slower than the accepted series of 2026-09-24, for
   example `s1_copy` 1 MiB (chained submit) p50 ≈ 575 vs 352 µs. *Block-wait* cells are unaffected: within 13 µs at
   4 KiB, and 62–140 µs faster at 1 MiB. In spin mode, the work on both processors slows down: the CPU's own read of
   1 MiB takes 45 vs 14 µs and the GPU's 1 MiB copy 65 vs 21 µs. Each of the following was ruled out by a direct
   comparison: the binary (the accepted series' commit, rebuilt and alternated with the current binary, agrees within
   2 %); the power supply (20 W and 40 W, item 1); the external display and display activity (internal panel only, and
   all displays asleep); dark versus full wake (item 4); the process's nice level (0 vs 5); the launch context (a
   terminal application, as for the accepted series, versus a detached terminal-session service: four alternating runs
   each, spin cells +109–243 vs +111–262 µs); and the benchmark thread's scheduling class and priority (a
   user-interactive QoS request plus a base priority of 47 against the default 31, alternated with the default over
   eight runs: the two agree within 12 µs on 16 of 20 cells, and the other four follow the run order, not the flag).
   What did move the cells was the clock. In that last series the first three runs executed at 2.2 GHz averaged over
   the process (exit resource counters: performance cores 2.6–2.8 GHz, efficiency cores 1.4–1.5 GHz) and showed the
   usual +170–290 µs. From the fourth run on, a macOS housekeeping process (iCloud file sync after a reboot, about half
   a core for four minutes) raised the load average from 1–2 to about 4 and the process clock to 2.8–3.2 GHz, and the
   spin cells sped up without any change on our side: the empty cells fell to the accepted levels or below (chained,
   101–130 µs in the two fastest runs against 249), the `s2_direct` 4 KiB cells reached them in the last runs (210–214
   against 201), and the `s1_copy` 4 KiB cells closed only part of the gap. Across the eight runs the spin round trips
   correlate with the clock (r = −0.81 to −0.95 for five of the six empty and 4 KiB cells, −0.58 for the sixth) and
   only weakly with the share of CPU time spent on performance cores (−0.52 to +0.16 for the same cells), which an
   earlier series had suggested as the cause. The 1 MiB spin cells stayed +180–230 µs in all but one run, so they
   carry a second component, the slow 1 MiB read in spin mode, that the clock alone does not remove.
   Our reading, not yet confirmed by a per-thread clock trace (root-level power sampling during a run is planned): on
   an otherwise idle M5 the governor leaves a lone spin-waiting thread at about 2.2 GHz, and another process's load
   raises the clock that the benchmark thread then shares. The accepted series ran with the chassis at thermal state
   "fair", that is with the machine busier than during any of our checks, which would make "what changed" the machine's
   background activity rather than a setting. Consequences: the M5's idle spin-wait numbers (§5) are governor-dependent,
   and a quiet machine is not the fastest machine for them. §5's verdict does not depend on it: in the present state
   the M5's block-wait cells are still slower than the 3060 Ti's best path (`s2_direct` 1 MiB, chained, p50 205–222 vs
   145–161 µs; on the PC, block and spin wait differ by ≤ 6 µs). The interpolated crossover does depend on it: it rests
   on the M5's 1 MiB level and would move to smaller payloads if the present block-wait level held. Two responses are
   open: re-measure the idle reference in the present state and report the spin-wait cells as clock-dependent, or make
   a defined CPU-side companion load part of the microbenchmark condition, which is closer to the game, whose
   simulation thread never spins alone. No M5 extension series is measured before that decision.
   <!-- results/m1_mac_40w_check/display_0545/compare.txt (spin and block deltas, legs), ab_compare.txt (binary A/B),
        launch_0558/ (nice 5 runs; Terminal arm failed), launch_1253/ (2026-10-21 retest: launch context ruled out;
        ru_*.json = exit rusage per run), launch_1948/compare.txt + README section "QoS/priority retest" (2026-10-22:
        H − D per cell, GHz per run from exit rusage, r(rt, GHz) and r(rt, P-core share) per spin cell; the load/FPCKService
        timing is Main's process log quoted in the README), DECISIONS #36. Block deltas: 4K +1…+13, 1M −62…−127 (10-22) and
        within 11 / −84…−140 (10-20). OPEN (user, STATUS to-do 2): the root powermetrics check, then (a) re-measure the idle
        reference or (b) a defined CPU-side load as a bench condition. PC block vs spin: results/m1_report*/summary.csv
        (chain, idle). -->

In each case the first minute of cells showed the problem against the accepted levels, and the attempt was
stopped and discarded, never repaired or pooled. A practical lesson for latency studies on laptops: "on AC",
"idle" and "awake" have to be checked by measurement, not assumed, and a reference check before every series
catches what the checklist misses. "Idle" also cuts both ways on a laptop-class SoC: a lone busy thread on a quiet
machine can run at a lower clock than the same thread beside a background load, so single-thread latencies need the
effective clock recorded next to them.

## 12.3 Run-to-run variation

- **The M5 fails the protocol's run-to-run criterion** (26 of 52 cells vary by ≥ 10 % across process
  restarts; worst primary cell 29 %). The reason is the multimodal CPU→GPU hand-off, whose dominant mode persists
  for minutes. We report this as a result, not as noise to be averaged away: p50 and p99 are pooled over three
  restarts, and histograms show the modes. The PC passes (0 of 124 cells; worst 8.4 %).
  <!-- results/m1_report/report.md -->
- **Thermal state.** The fanless chassis can throttle. Each series logs the thermal state at start and end. The
  accepted Mac series stayed at "fair" throughout (in hindsight also a sign that the machine was not idle then,
  §12.2 item 5), and the protocol flags throttled runs. Idle cells run before
  hot cells, because idle blocks measured after loaded ones come out faster (hysteresis, DECISIONS #32).
- **Clock correlation.** The GPU→CPU time mapping drifts on the PC unless it is refreshed by elapsed time.
  Before the fix, `g2c` and `c2g` drifted while `rt` stayed valid. The split between `g2c` and `c2g` is
  therefore reported only for series measured after the fix (DECISIONS #23, #30).

## 12.4 Interpretation

- **Power-state explanation.** That the M5's multimodal `c2g` comes from GPU idle states is an inference. We found
  no published idle-exit latencies for Apple or NVIDIA GPUs (§2), and macOS does not expose GPU power states to
  user code. The hot-GPU cells test the inference indirectly: if holding the GPU busy collapses the modes, the
  reading is supported. Until then the text says "consistent with".
- **Hypotheses after the fact.** H1′ was written after M1's idle results were known (2026-10-13), but before
  the extension that tests it was measured. We state this, and keep the refuted H1 and its criterion in the
  paper unchanged.
