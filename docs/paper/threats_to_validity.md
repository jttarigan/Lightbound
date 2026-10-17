# §12 Threats to validity (partial draft, 2026-10-17)

<!-- Sources: docs/06 §9 (the protocol's threat list), DECISIONS #17, #23, #30, #32, #33, #35,
     results/m1_report*/report.md, docs/PC_REPORT.md, results/m1_mac_20w_check/README.md (10-17 diagnostic),
     docs/STATUS.md 10-17. Game-side threats (bot workload, behaviour design, float equivalence) wait for M3–M8. -->

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
bandwidth figures. We found four ways the environment can bias a series, each with evidence from our own runs:

1. **Power source.** A series on battery was excluded, and the protocol requires the Mac to be plugged in.
   *Plugged in is not sufficient, though.* On a 20 W USB-C adapter, macOS reported "AC Power" and Low Power Mode
   was off. Even so, idle round trips came out ≈ 200 µs slower than in the accepted series
   (`s1_copy` 4 KiB p50 ≈ 500 vs 289–294 µs). The CPU was idle, the GPU was idle and the thermal state was
   nominal. The binary is ruled out: the original series' commit, rebuilt and run alternately with the current
   binary, gave the same numbers within 2 %. We suspect the adapter limits the SoC's power budget, but that is
   not proven. Rule adopted: before a Mac series, a two-minute reference check of the empty, `s1_copy` and
   `s2_direct` cells against the accepted levels; the adapter's wattage (`pmset -g ac`) goes into every series'
   notes. The accepted 10-13 series predates that rule, so its adapter is recorded as unknown.
   <!-- results/m1_mac_20w_check/README.md; DECISIONS #35. Re-check the claim once a 35 W+ adapter run exists:
        if it reproduces the 10-13 levels, state the adapter effect as measured; if not, rewrite this item. -->
2. **Other processes using the GPU.** A browser tab that animates kept the GPU 17–30 % busy (IOAccelerator
   "Device Utilization") with no benchmark running. Under that load, idle cells were 1.5–3× slower and
   erratic: `s1_copy` 1 MiB p50 525 vs 351 µs, p99 1780 vs 516 µs. Rule: no other GPU client; a series starts
   only once GPU utilization has stayed ≤ 5 % and the 1-minute load average below 3 for two minutes.
3. **CPU contention.** A concurrent 8-core analysis job (load average 14–16) inflated the p99 of the first cells
   about twofold. The same start rule catches it.
4. **Display and sleep state.** The protocol keeps the display on. Preventing only system sleep
   (`caffeinate -is`) lets the display sleep after 10 minutes, so runs now use `caffeinate -dis`.

In each case the first minute of cells showed the problem against the accepted levels, and the attempt was
stopped and discarded, never repaired or pooled. A practical lesson for latency studies on laptops: "on AC" and
"idle" have to be checked by measurement, not assumed.

## 12.3 Run-to-run variation

- **The M5 fails the protocol's run-to-run criterion** (26 of 52 cells vary by ≥ 10 % across process
  restarts; worst primary cell 29 %). The reason is the multimodal CPU→GPU hand-off, whose dominant mode persists
  for minutes. We report this as a result, not as noise to be averaged away: p50 and p99 are pooled over three
  restarts, and histograms show the modes. The PC passes (0 of 124 cells; worst 8.4 %).
  <!-- results/m1_report/report.md -->
- **Thermal state.** The fanless chassis can throttle. Each series logs the thermal state at start and end. The
  accepted Mac series stayed at "fair" throughout, and the protocol flags throttled runs. Idle cells run before
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
