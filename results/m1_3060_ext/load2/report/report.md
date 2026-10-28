# M1 microbenchmark report

Runs: `micro_P-3060_rebar-on_run1` (P-3060, ?, thermal n/a→n/a, failures 0), `micro_P-3060_rebar-on_run2` (P-3060, ?, thermal n/a→n/a, failures 0), `micro_P-3060_rebar-on_run3` (P-3060, ?, thermal n/a→n/a, failures 0)

## P-3060 — round trip p50 / p99 (µs), chain / spin

| path | 0 | 4K | 64K | 256K | 1M | 1536K | 2M | 3M | 4M | 8M | 16M | 32M | 64M |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| empty | 60 / 76 | — | — | — | — | — | — | — | — | — | — | — | — |
| s2_direct | — | 59 / 69 | 70 / 81 | 96 / 112 | 206 / 256 | 280 / 350 | 355 / 428 | 504 / 594 | 661 / 761 | 1244 / 1410 | 2419 / 2726 | 4807 / 5336 | 9474 / 10543 |
| s1_copy | — | 72 / 90 | 90 / 107 | 133 / 160 | 312 / 355 | 431 / 496 | 552 / 626 | 805 / 868 | 1059 / 1258 | 2042 / 2309 | 3985 / 4408 | 7887 / 8549 | 15721 / 16759 |
| s2_hostcached | — | 61 / 74 | 69 / 86 | 87 / 110 | 160 / 203 | 209 / 267 | 260 / 332 | 370 / 437 | 478 / 574 | 902 / 1101 | 1835 / 2139 | 3627 / 4224 | 7239 / 8249 |

- raw copy bw_d2h 64M: 5088.3 µs = 13.19 GB/s
- raw copy bw_h2d 64M: 5091.3 µs = 13.18 GB/s

### P-3060 — hot GPU (second queue busy during the CPU step, DECISIONS #32)

| path | 0 | 4K | 64K | 256K | 1M | 1536K | 2M | 3M | 4M | 8M | 16M | 32M | 64M |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| empty | 728 / 735 | — | — | — | — | — | — | — | — | — | — | — | — |
| s2_direct | — | 727 / 729 | 727 / 730 | 728 / 731 | 724 / 730 | 727 / 733 | 725 / 762 | 721 / 1065 | 1060 / 1762 | 1560 / 1900 | 2727 / 3562 | 5217 / 6560 | 9871 / 12223 |
| s1_copy | — | 741 / 749 | 753 / 761 | 779 / 788 | 900 / 910 | 979 / 993 | 1057 / 1067 | 1215 / 1227 | 1383 / 2478 | 3838 / 4730 | 8717 / 9577 | 16897 / 18408 | 33091 / 35920 |
| s2_hostcached | — | 727 / 729 | 727 / 730 | 728 / 731 | 729 / 732 | 729 / 733 | 729 / 732 | 729 / 734 | 729 / 1431 | 1070 / 1578 | 1923 / 3122 | 3964 / 4922 | 7344 / 10095 |

- background load: micro_burn dispatches of 179–201 µs per run; duty min 0.55, median 0.73 over 444 cells (≈ 1 = the second queue never idled)

## Run-to-run variation (p50 across process restarts)

- P-3060 hot GPU: 3 runs, 148 cells; cells ≥ 10 %: 5; worst 26.2 % (s2_hostcached chain/block 16M); worst in primary condition 18.0 % → FAIL
- P-3060: 3 runs, 148 cells; cells ≥ 10 %: 4; worst 12.6 % (s2_hostcached chain/block 3M); worst in primary condition 10.6 % → FAIL

## Latency modes (empty round trip, all runs pooled)

A single p50 hides multimodal sync latency; histogram peaks (5 µs bins, ≥ 1 % of samples):

- P-3060 hot GPU: g2c: 30–35 (46 %), 50–55 (5 %); c2g: 645–650 (16 %), 675–680 (3 %), 695–700 (29 %); rt: 680–685 (24 %), 725–730 (30 %)
- P-3060: g2c: 30–35 (63 %); c2g: 25–30 (48 %), 35–40 (34 %); rt: 60–65 (37 %)

## Go criterion (protocol §6)

**PENDING** — needs data from P-M5, P-4060.

**Hot-GPU comparison** — needs data from P-M5, P-4060.

Figures: `fig1_microbench.png` (primary condition), `fig1_modes.png` (all submit × wait modes), `fig1_hot.png` (hot-GPU cells).
