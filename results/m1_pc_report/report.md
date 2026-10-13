# M1 microbenchmark report

Runs: `micro_P-3060_rebar-on_run1` (P-3060, ?, thermal n/a→n/a, failures 0), `micro_P-3060_rebar-on_run2` (P-3060, ?, thermal n/a→n/a, failures 0), `micro_P-3060_rebar-on_run3` (P-3060, ?, thermal n/a→n/a, failures 0)

## P-3060 — round trip p50 / p99 (µs), chain / spin

| path | 0 | 4K | 64K | 256K | 1M | 4M | 16M |
|---|---:|---:|---:|---:|---:|---:|---:|
| empty | 60 / 85 | — | — | — | — | — | — |
| s2_direct | — | 60 / 80 | 69 / 103 | 97 / 134 | 207 / 278 | 648 / 810 | 2395 / 2650 |
| s1_copy | — | 73 / 105 | 89 / 119 | 131 / 179 | 312 / 412 | 1053 / 1296 | 3940 / 4437 |
| s2_hostcached | — | 62 / 94 | 69 / 102 | 87 / 123 | 160 / 230 | 466 / 612 | 1759 / 2343 |
| s2_rebar | — | 1748 / 1853 | 27164 / 27964 | 108717 / 110729 | 436664 / 442832 | 1757962 / 1769825 | 7031791 / 7063888 |
| s2_coherent | — | 119 / 145 | 991 / 1211 | 3813 / 4365 | 14960 / 16343 | 59824 / 65164 | 239149 / 260482 |

- raw copy bw_d2h 16M: 1354.8 µs = 12.38 GB/s
- raw copy bw_h2d 16M: 1264.6 µs = 13.27 GB/s

## Run-to-run variation (p50 across process restarts)

- P-3060: 3 runs, 124 cells; cells ≥ 10 %: 0; worst 8.4 % (s2_hostcached perpass/block 16M); worst in primary condition 7.4 % → PASS

## Latency modes (empty round trip, all runs pooled)

A single p50 hides multimodal sync latency; histogram peaks (5 µs bins, ≥ 1 % of samples):

- P-3060: g2c: 30–35 (73 %); c2g: 25–30 (48 %), 35–40 (40 %); rt: 60–65 (36 %)

## Go criterion (protocol §6)

**PENDING** — needs data from P-M5, P-4060.

Figures: `fig1_microbench.png` (primary condition), `fig1_modes.png` (all submit × wait modes).
