# M1 microbenchmark report

Runs: `micro_P-3060_rebar-on_run1` (P-3060, ?, thermal n/a→n/a, failures 0), `micro_P-3060_rebar-on_run2` (P-3060, ?, thermal n/a→n/a, failures 0), `micro_P-3060_rebar-on_run3` (P-3060, ?, thermal n/a→n/a, failures 0)

## P-3060 — round trip p50 / p99 (µs), chain / spin

| path | 0 | 4K | 64K | 256K | 1M | 4M | 16M |
|---|---:|---:|---:|---:|---:|---:|---:|
| empty | 60 / 83 | — | — | — | — | — | — |
| s2_direct | — | 60 / 79 | 69 / 104 | 96 / 135 | 207 / 279 | 646 / 804 | 2387 / 2652 |
| s1_copy | — | 73 / 98 | 89 / 113 | 131 / 166 | 311 / 383 | 1054 / 1259 | 3935 / 4395 |
| s2_hostcached | — | 62 / 91 | 69 / 102 | 87 / 124 | 159 / 231 | 463 / 614 | 1742 / 2274 |
| s2_rebar | — | 1748 / 1856 | 27163 / 27960 | 108712 / 110651 | 436651 / 442876 | 1758034 / 1769897 | 7032390 / 7064631 |
| s2_coherent | — | 119 / 148 | 991 / 1217 | 3817 / 4324 | 14960 / 16269 | 59800 / 64968 | 239183 / 256932 |

- raw copy bw_d2h 16M: 1357.8 µs = 12.36 GB/s
- raw copy bw_h2d 16M: 1264.6 µs = 13.27 GB/s

## Run-to-run variation (p50 across process restarts)

- P-3060: 3 runs, 124 cells; cells ≥ 10 %: 0; worst 8.4 % (s2_hostcached perpass/block 16M); worst in primary condition 7.4 % → PASS

## Latency modes (empty round trip, all runs pooled)

A single p50 hides multimodal sync latency; histogram peaks (5 µs bins, ≥ 1 % of samples):

- P-3060: g2c: 30–35 (78 %); c2g: 25–30 (48 %), 35–40 (39 %); rt: 60–65 (37 %)

## Go criterion (protocol §6)

**PENDING** — needs data from P-M5, P-4060.

Figures: `fig1_microbench.png` (primary condition), `fig1_modes.png` (all submit × wait modes).
