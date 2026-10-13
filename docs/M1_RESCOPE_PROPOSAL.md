# M1 re-scope proposal (draft for the user's approval, 2026-10-13)

_Written before any new measurement, as the protocol asks (§1: hypotheses are stated in advance)._
_Nothing here is in force until the user says so. Decisions are listed first; the reasoning follows._

## 1. Decisions needed (with recommendations)

0. **M1 go/no-go.** By protocol §6 the combined report says **NO-GO**: with a cold GPU the M5's empty and
   1 MiB round trips (214 / 305 µs) are 2–3.5× *slower* than the RTX 3060 Ti's (60 / 142–160 µs), not
   lower. **Recommendation:** record NO-GO on H1 as written and re-scope to the payload crossover (section 3),
   then continue with M2 under the new hypothesis. Alternative: wait for the hot-GPU cells (a) first.
   a. **Hot-GPU cells.** Measure every microbench cell again with the GPU kept busy by a second queue, as
      in a real frame, on both backends (built, DECISIONS #32; off by default). **Recommendation: yes.**
      Mac ≈ 70 min on AC, PC ≈ 3 h (section 4).
   b. **PCIe gen3.** The PC's slot is gen3, not gen4 as §2 says. **Recommendation:** accept the measured
      gen3 x16 (3060 Ti) and gen3 x8 (4060 Ti, expected) as the platform definition; §2 already says
      "measured link", and the x16-vs-x8 ratio that H5 needs is unchanged.
   c. **ReBAR on, `s2_direct`:** report `rt` as the protocol defines it, with `rt` + GPU read as a secondary
      figure. **Recommendation: yes.** ReBAR moves ≈ 80 µs from the GPU's read into the CPU's write.
   d. **Repository visibility.** The GitHub repo is public although the PC task file said private.
      **Recommendation:** keep it public for now; anonymous HTTPS is the Mac's only way to fetch the PC's
      results while SSH is blocked.

## 2. What M1 showed (facts, from `results/m1_report*`)

- **Where the M5 loses: the hand-off.** With nothing else running, the M5's CPU→GPU hand-off takes
  ≈ 45 / 155 / 235 µs depending on the GPU's power state (finding 1); the PC's takes ≈ 30 µs and is stable.
  This is the whole deficit at small payloads.
- **Where the M5 wins: moving bytes.** The CPU's read + write of the payload costs the M5 0 / 2 / 7 / 30 /
  124 / 647 µs for 4K … 16M, the 3060 Ti 1 / 6 / 21 / 82 / 322 / 1300 µs; raw copy bandwidth is 55–58 vs
  12–13 GB/s. At 16 MiB the M5's round trip is 842 µs against 1392–2395 µs.
- **The crossover (finding 9).** Cold, the M5's round trip drops below the 3060 Ti's at ≈ 2.0–2.6 MiB per
  direction (log-log interpolation between the 1 and 4 MiB points; nothing was measured in between).
  With the GPU hot (probe, finding 7: hand-offs 23 + 31 µs), the estimate is ≈ 1.1× faster up to 256K
  (within noise), ≈ 1.3× at 1 MiB and ≈ 1.8× at 4–16 MiB. Unmeasured.
- **In game terms:** ≈ 28 B per agent per direction on average (Perception 32 + SortedIds 4 down,
  Intent 16 + RefineRequest up). 1 MiB ≈ 37k agents; the cold crossover ≈ 75k–100k agents. Planned N goes
  to 100k (capacity 131,072). Every round trip is paid K times per frame (the H3 mechanism).

So H1 as written ("lower for all payload sizes") is refuted, and the data shows exactly what §1 calls the
expected honest outcome: a crossover, not a universal win.

## 3. Proposed re-scope

**H1′ (replaces H1).** On unified memory the per-round-trip cost is nearly flat in payload up to ≈ 1 MiB and
grows slowly beyond; over PCIe it grows linearly from ≈ 256 KiB. The two curves cross, and the crossover
moves to smaller payloads when the GPU is busy. **Predictions, written now:** cold crossover 2.0–2.6 MiB per
direction; hot M5 ≈ 1.3× faster at 1 MiB and ≈ 1.8× at 4–16 MiB; hot crossover at or below 256 KiB.
The paper's contribution becomes: the measured crossover, its dependence on GPU state, and the cost model
that predicts the game's frame time from it (H2, H3, H5 unchanged; §7 cost model unchanged).

**Microbench extension (one change to §6):**
1. **Hot-GPU condition** (DECISIONS #32): every cell also with the GPU busy. Text to add to §6:
   > *GPU state.* Each cell is measured twice: with the GPU otherwise idle (as above), and with a second
   > queue running a calibrated ≈ 200 µs compute kernel back to back during the whole cell ("hot", cells
   > named `<path>_hot`), the regime of a real frame (docs/05 §3). Idle cells run first. The go criterion
   > is evaluated on [the user decides: idle cells / hot cells / both].
2. **Payload grid for the crossover:** `4K, 64K, 256K, 1M, 1536K, 2M, 3M, 4M, 8M, 16M, 32M, 64M`, paths
   `empty, s1_copy, s2_direct, s2_hostcached` (no `s2_rebar`: 7 s per iteration at 16 MiB; no `s2_coherent`:
   uncached CPU reads, minutes per large cell). Needs no code: `--micro-payloads` and `--micro-paths`.
3. Report: the crossover per platform pair (idle and hot), with the run-to-run spread as the error bar.

**Then M2** (the agent system) under H1′, with N levels chosen to bracket the crossover.

Research invariants stay as they are. In particular R1: the gain is exploited through the workload
(N, bytes per agent, K), never through M5-only code.

## 4. What the runs need

Mac (`results/m1_mac_ext`): on the charger, lid open, Low Power Mode off, no other session using the GPU,
under `caffeinate -is`; ≈ 70 min for 3 runs (idle + hot, 12 payloads):
```
export PATH="$HOME/Library/Python/3.9/bin:$PATH"
caffeinate -is python3 tools/run_microbench.py --exe build/lightbound --out results/m1_mac_ext -- \
    --micro-gpuload=both --micro-paths=empty,s1_copy,s2_direct \
    --micro-payloads=4K,64K,256K,1M,1536K,2M,3M,4M,8M,16M,32M,64M
```
PC (`results\m1_3060_ext`, ReBAR on, same setup rules as before; ≈ 3 h, rough): the same command with
`--micro-paths=empty,s1_copy,s2_direct,s2_hostcached`, after the bundle with this code is merged and rebuilt
(the PC message `docs/PC_MESSAGE_2026-10-13.md`). The extension directories stay separate from the protocol
series (`m1_mac_ac`, `m1_3060`); the report pools runs per platform.
