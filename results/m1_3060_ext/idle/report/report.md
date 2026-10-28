# M1 microbenchmark report

Runs: `micro_P-3060_rebar-on_run1` (P-3060, ?, thermal n/a→n/a, failures 0), `micro_P-3060_rebar-on_run2` (P-3060, ?, thermal n/a→n/a, failures 0), `micro_P-3060_rebar-on_run3` (P-3060, ?, thermal n/a→n/a, failures 0)

## P-3060 — round trip p50 / p99 (µs), chain / spin

| path | 0 | 4K | 64K | 256K | 1M | 1536K | 2M | 3M | 4M | 8M | 16M | 32M | 64M |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| empty | 60 / 103 | — | — | — | — | — | — | — | — | — | — | — | — |
| s2_direct | — | 59 / 87 | 69 / 82 | 95 / 129 | 204 / 250 | 281 / 350 | 355 / 428 | 511 / 600 | 656 / 731 | 1224 / 1382 | 2385 / 2682 | 4745 / 5261 | 9425 / 10461 |
| s1_copy | — | 74 / 111 | 90 / 132 | 131 / 183 | 311 / 394 | 431 / 507 | 551 / 651 | 791 / 900 | 1086 / 1243 | 2055 / 2338 | 4008 / 4346 | 7859 / 8447 | 15629 / 16683 |
| s2_hostcached | — | 62 / 78 | 69 / 100 | 87 / 132 | 159 / 230 | 205 / 281 | 252 / 324 | 358 / 435 | 459 / 582 | 867 / 1029 | 1754 / 2047 | 3514 / 4090 | 7046 / 8211 |

- raw copy bw_d2h 64M: 5108.7 µs = 13.14 GB/s
- raw copy bw_h2d 64M: 5091.3 µs = 13.18 GB/s

### P-3060 — hot GPU (second queue busy during the CPU step, DECISIONS #32)

| path | 0 | 4K | 64K | 256K | 1M | 1536K | 2M | 3M | 4M | 8M | 16M | 32M | 64M |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| empty | 682 / 730 | — | — | — | — | — | — | — | — | — | — | — | — |
| s2_direct | — | 681 / 729 | 681 / 730 | 683 / 730 | 681 / 730 | 681 / 781 | 680 / 879 | 675 / 1138 | 1129 / 1328 | 1581 / 1895 | 2784 / 3521 | 5196 / 6550 | 9742 / 13016 |
| s1_copy | — | 695 / 743 | 707 / 756 | 734 / 782 | 855 / 903 | 934 / 983 | 1013 / 1062 | 1171 / 1244 | 1377 / 2214 | 3821 / 4727 | 8290 / 9530 | 16600 / 18290 | 32827 / 35591 |
| s2_hostcached | — | 681 / 729 | 681 / 729 | 683 / 730 | 683 / 732 | 683 / 732 | 684 / 732 | 684 / 849 | 684 / 1148 | 1144 / 1578 | 1918 / 2935 | 3953 / 5143 | 7319 / 10222 |

- background load: micro_burn dispatches of 178–201 µs per run; duty min 0.54, median 0.72 over 444 cells (≈ 1 = the second queue never idled)

## Run-to-run variation (p50 across process restarts)

- P-3060 hot GPU: 3 runs, 148 cells; cells ≥ 10 %: 4; worst 62.5 % (s1_copy perpass/spin 4M); worst in primary condition 11.5 % → FAIL
- P-3060: 3 runs, 148 cells; cells ≥ 10 %: 4; worst 13.1 % (s2_direct perpass/block 2M); worst in primary condition 6.2 % → FAIL

## Latency modes (empty round trip, all runs pooled)

A single p50 hides multimodal sync latency; histogram peaks (5 µs bins, ≥ 1 % of samples):

- P-3060 hot GPU: g2c: 35–40 (49 %); c2g: 25–30 (3 %), 645–650 (39 %), 690–695 (19 %); rt: 55–60 (2 %), 680–685 (48 %), 725–730 (28 %)
- P-3060: g2c: 30–35 (92 %); c2g: 25–30 (49 %); rt: 60–65 (44 %)

## Go criterion (protocol §6)

**PENDING** — needs data from P-M5, P-4060.

**Hot-GPU comparison** — needs data from P-M5, P-4060.

Figures: `fig1_microbench.png` (primary condition), `fig1_modes.png` (all submit × wait modes), `fig1_hot.png` (hot-GPU cells).
