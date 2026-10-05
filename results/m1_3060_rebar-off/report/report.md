# M1 microbenchmark report

Runs: `micro_P-3060_rebar-off_run1` (P-3060, ?, thermal n/a→n/a, failures 0), `micro_P-3060_rebar-off_run2` (P-3060, ?, thermal n/a→n/a, failures 0), `micro_P-3060_rebar-off_run3` (P-3060, ?, thermal n/a→n/a, failures 0)

## P-3060 — round trip p50 / p99 (µs), chain / spin

| path | 0 | 4K | 64K | 256K | 1M | 4M | 16M |
|---|---:|---:|---:|---:|---:|---:|---:|
| empty | 61 / 109 | — | — | — | — | — | — |
| s2_direct | — | 61 / 102 | 67 / 100 | 82 / 129 | 142 / 198 | 408 / 537 | 1411 / 1688 |
| s1_copy | — | 75 / 124 | 92 / 128 | 134 / 197 | 314 / 420 | 1103 / 1318 | 4014 / 4590 |
| s2_hostcached | — | 61 / 101 | 67 / 107 | 85 / 134 | 157 / 241 | 468 / 665 | 1768 / 2212 |
| s2_coherent | — | 118 / 166 | 991 / 1253 | 3801 / 4392 | 15085 / 16457 | 60376 / 64769 | 240359 / 259409 |

- raw copy bw_d2h 16M: 1367.6 µs = 12.27 GB/s
- raw copy bw_h2d 16M: 1273.9 µs = 13.17 GB/s

## Run-to-run variation (p50 across process restarts)

- P-3060: 3 runs, 100 cells; cells ≥ 10 %: 0; worst 9.3 % (s2_direct perpass/block 16M); worst in primary condition 8.4 % → PASS

## Latency modes (empty round trip, all runs pooled)

A single p50 hides multimodal sync latency; histogram peaks (5 µs bins, ≥ 1 % of samples):

- P-3060: g2c: 30–35 (74 %); c2g: 35–40 (38 %); rt: 60–65 (37 %)

## Go criterion (protocol §6)

**PENDING** — needs data from P-M5, P-4060.

Figures: `fig1_microbench.png` (primary condition), `fig1_modes.png` (all submit × wait modes).
