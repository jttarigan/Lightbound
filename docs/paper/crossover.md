# §6 Payload crossover and GPU state (pre-registration and method drafted 2026-10-21; results pending)

<!-- Sources: docs/06_RESEARCH_PROTOCOL.md §1 (H1′ row, written 2026-10-13, adopted 2026-10-16) and §6 "Extension for H1′";
     DECISIONS #32, #33; platforms_and_m1_results.md (§5 interpolation, results/m1_crossover/crossover.md).
     6.3 waits for results/m1_mac_ext (blocked by the M5 spin-wait shift, §12.2 item 5) and results/m1_3060_ext (PC). -->

## 6.1 The hypothesis, as written before the measurement

H1′ was formulated on 2026-10-13, after the idle-GPU results of §5 were known and before any of the cells that test
it were measured, and adopted on 2026-10-16 when H1 was given up. It reads: on shared physical memory the cost of a
round trip is nearly flat in payload up to ≈ 1 MiB per direction and grows slowly beyond; over PCIe it grows
linearly from ≈ 256 KiB. The two curves cross, and the crossover moves to smaller payloads when the GPU is busy. Its
predictions, fixed at the same time, are:
1. with the GPU idle, the M5 and the RTX 3060 Ti cross at 2.0–2.6 MiB per direction;
2. with the GPU busy, the M5 is ≈ 1.3× faster at 1 MiB and ≈ 1.8× faster at 4–16 MiB, and the crossover lies at
   or below 256 KiB.

The interpolation of §5, made on the protocol's coarse grid, puts the idle crossover at ≈ 2.4 MiB with ReBAR on,
inside the predicted interval, and at ≈ 3.0 MiB with ReBAR off, above it. Nothing was measured between 1 and 4 MiB
there; this section measures that range directly.

## 6.2 Measurement

The extension of §4.6 measures the paths `empty`, `s1_copy` and `s2_direct` (and `s2_hostcached` on Vulkan) on
twelve payloads from 4 KiB to 64 MiB, with five points between 1 and 8 MiB where the idle curves are expected to
cross. Every cell is measured twice, with the GPU otherwise idle and with the GPU hot (§4.5), idle cells first, and
each series is three process restarts. The crossover is read per platform pair and GPU state by log–log
interpolation between the two neighbouring payloads, and its error bar is the spread of that estimate over the
pairs of runs. Idle and hot results are reported separately and never pooled. The extension series are kept apart
from the §5 series, and a two-minute reference check of the empty, `s1_copy` and `s2_direct` cells against the
accepted §5 levels precedes every M5 series (§12.2).

## 6.3 Results

[Figure: round trip against payload per direction, idle and hot, both platforms, with the measured crossover bands.
Table: crossover per path, GPU state and ReBAR setting with its run-to-run spread, against predictions 1 and 2. Text:
whether holding the GPU busy collapses the M5's multimodal CPU→GPU hand-off (§5), the hot-GPU ratios at 1 MiB and
4–16 MiB, and the verdict on each prediction. Waits for `m1_mac_ext` and `m1_3060_ext`.]
