# §9 Cost model (formulation; draft 2026-10-17, nothing fitted yet)

<!-- Sources: docs/06 §7 (the model as specified), docs/05 §3 and §5 (pipeline, buffer sizes),
     docs/STATUS.md "M1 combined report" (bytes per agent), platforms_and_m1_results.md (§5 numbers).
     Fitting waits for: m1_*_ext (T_sync, BW_eff by GPU state) and K = 1 game runs (a_*, b_*), M4+. -->

## 9.1 The model

The protocol fixes the model's form before any game run, per platform *p* and strategy *S*:

```
T_frame(N, K, S) ≈ a_gpu·N + b_gpu + a_cpu·N + b_cpu
                   + K · ( T_sync + (P_g2c(N) + P_c2g(N)) / BW_eff(S) )
                   + [S1] K · T_copy_setup
```

- **The first line** is the work that does not depend on K: GPU passes A and E, and CPU pass B. It grows linearly in
  the agent count N. Its coefficients come only from the game at K = 1.
- **The second line** is the cost of K round trips. `T_sync` is the payload-independent hand-off (GPU→CPU plus
  CPU→GPU), and the payload term is the bytes moved per round trip divided by an effective bandwidth. Both come
  only from the microbenchmark.
- **The third line** adds, for the copy strategy, the fixed cost of setting up its copies.

Fitting the two halves on disjoint data is the point: if a model fitted on K = 1 runs and a microbenchmark
predicts K = 2–4, the microbenchmark is a usable design tool for engine developers. We report the prediction
error as MAPE per platform, and the predicted against measured crossover K\*.

## 9.2 Payload per round trip, in agents

From the buffer layouts, one round trip moves the bytes of Table 7.
<!-- docs/05 §5 -->

**Table 7** Bytes per round trip

| direction | buffers | bytes |
|---|---|---|
| GPU→CPU | `Perception` 32 B + `SortedIds` 4 B per agent; `CellRange` 8 B per spatial-hash cell; `GpuEvents` ≤ 16 KiB | ≈ 36 B·N + 8 B·cells |
| CPU→GPU | `Intent` 16 B per agent; `RefineRequest` 16 B for ≤ N/4 agents (K ≥ 2) | 16 B·N, up to ≈ 20 B·N |

With a 16 px cell and 16 px tiles, a 200 × 200-tile floor has 40,000 cells, so `CellRange` adds ≈ 312 KiB per
GPU→CPU hand-off, independent of N. This is a fixed term the microbenchmark does not have. It enters
`P_g2c(N)` as an intercept, and the model must keep it rather than fold it into `T_sync`.
<!-- kTilePx = 16 (src/procgen/FloorData.h); cell = 16 px (docs/05 §5). Bench floors are 200×200 at depth 5. -->

Averaged over the two directions, a round trip moves ≈ 28 B per agent, so 1 MiB per direction is ≈ 37k agents.
The idle-GPU crossover interpolated in §5 is therefore ≈ 89k agents with ReBAR on (≈ 2.4 MiB per direction),
at the top of the protocol's N grid (1k–100k), and ≈ 112k with ReBAR off (≈ 3.0 MiB), beyond the grid but below
the capacity of 131,072. The model thus makes a testable prediction about the game, not only about the benchmark.
<!-- results/m1_crossover/crossover.md, best path; agents = bytes / 28. Was "2.0–2.6 MiB ≈ 75k–100k" until
     2026-10-21. The ReBAR-off crossover lies outside the N grid: whether to extend the grid is open (ask). --> If the hot-GPU crossover moves down as H1′ predicts
(to ≤ 256 KiB), the game's crossover in N moves to ≲ 10k agents.

These agent counts ignore the `CellRange` intercept. With it, the GPU→CPU direction reaches a given payload with
≈ 9k fewer agents (312 KiB / 36 B), which moves the crossover lower still. If the hot crossover is at or below
≈ 312 KiB, the game's GPU→CPU hand-off is past it at every N.

## 9.3 Estimating the round-trip terms

- **`T_sync(p, state)`:** the empty round trip p50 (chain/spin), separately for idle and hot GPU. Idle values are
  known: 214 µs (M5) and 60 µs (3060 Ti). Hot values come from the extension.
- **`BW_eff(S, p, state)`:** the slope of round trip against payload over the extension grid, fitted on log-spaced
  points with `T_sync` held fixed. For S1 the slope includes both copies, and `T_copy_setup` is the S1–S2
  difference at the smallest payload.
- **Piecewise behaviour.** The M5's curve is flat to ≈ 1 MiB and steepens after 4 MiB (figure 1); a single
  `BW_eff` will not fit both regimes. The model uses the measured curve, interpolated log–log at the game's actual
  `P(N)`, instead of a straight line. This is a deviation from the protocol's linear form, to be recorded as a
  decision when the fit is made, not now.
- **Which GPU state applies.** In the game the GPU is busy between hand-offs, because pass A and the independent
  work run there. So the *hot* microbenchmark values are the a priori choice. The idle values give the bound for a
  GPU that finishes early. We report both predictions; neither is chosen after seeing the game data.

## 9.4 What would falsify it

- The model under-predicts K = 2–4 by more than the run-to-run spread. Then something per round trip is missing,
  for example cache effects on the CPU's read of freshly written GPU data, or the alarm BFS's random access to
  `CellRange`.
- The predicted K\* is on the wrong side of the measured one.
- On the M5, the multimodal hand-off (§5) makes `T_sync` a mixture, so per-frame times may be bimodal. The model
  predicts the median frame; p95/p99 frame times are reported but not modelled.
