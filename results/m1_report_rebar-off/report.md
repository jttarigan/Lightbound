# M1 microbenchmark report

Runs: `micro_P-3060_rebar-off_run1` (P-3060, ?, thermal n/a→n/a, failures 0), `micro_P-3060_rebar-off_run2` (P-3060, ?, thermal n/a→n/a, failures 0), `micro_P-3060_rebar-off_run3` (P-3060, ?, thermal n/a→n/a, failures 0), `micro_P-M5_run1` (P-M5, AC Power, thermal fair→fair, failures 0), `micro_P-M5_run2` (P-M5, AC Power, thermal fair→fair, failures 0), `micro_P-M5_run3` (P-M5, AC Power, thermal fair→fair, failures 0)

## P-3060 — round trip p50 / p99 (µs), chain / spin

| path | 0 | 4K | 64K | 256K | 1M | 4M | 16M |
|---|---:|---:|---:|---:|---:|---:|---:|
| empty | 60 / 99 | — | — | — | — | — | — |
| s2_direct | — | 59 / 88 | 65 / 89 | 80 / 114 | 142 / 212 | 388 / 530 | 1392 / 1684 |
| s1_copy | — | 74 / 120 | 90 / 129 | 131 / 178 | 312 / 408 | 1074 / 1298 | 3983 / 4583 |
| s2_hostcached | — | 59 / 93 | 67 / 106 | 85 / 146 | 157 / 239 | 470 / 606 | 1806 / 2134 |
| s2_coherent | — | 117 / 152 | 991 / 1168 | 3810 / 4296 | 15063 / 16251 | 60172 / 64126 | 239800 / 254593 |

- raw copy bw_d2h 16M: 1337.3 µs = 12.55 GB/s
- raw copy bw_h2d 16M: 1273.9 µs = 13.17 GB/s

## P-M5 — round trip p50 / p99 (µs), chain / spin

| path | 0 | 4K | 64K | 256K | 1M | 4M | 16M |
|---|---:|---:|---:|---:|---:|---:|---:|
| empty | 214 / 316 | — | — | — | — | — | — |
| s2_direct | — | 201 / 317 | 276 / 319 | 283 / 327 | 305 / 349 | 316 / 493 | 842 / 1814 |
| s1_copy | — | 292 / 344 | 294 / 359 | 305 / 379 | 352 / 442 | 393 / 739 | 1587 / 3001 |

- raw copy bw_d2h 16M: 303.1 µs = 55.36 GB/s
- raw copy bw_h2d 16M: 289.9 µs = 57.87 GB/s

## Run-to-run variation (p50 across process restarts)

- P-3060: 3 runs, 100 cells; cells ≥ 10 %: 0; worst 9.6 % (s2_direct perpass/spin 16M); worst in primary condition 6.9 % → PASS
- P-M5: 3 runs, 52 cells; cells ≥ 10 %: 26; worst 73.4 % (s1_copy perpass/block 4M); worst in primary condition 29.0 % → FAIL

## Latency modes (empty round trip, all runs pooled)

A single p50 hides multimodal sync latency; histogram peaks (5 µs bins, ≥ 1 % of samples):

- P-3060: g2c: 30–35 (81 %); c2g: 25–30 (48 %), 35–40 (29 %); rt: 60–65 (41 %)
- P-M5: g2c: 35–40 (24 %); c2g: 50–55 (8 %), 235–240 (9 %); rt: 85–90 (7 %), 280–285 (11 %)

## Go criterion (protocol §6)

**NO-GO** (protocol §6: stop and re-scope) — criterion: empty and 1 MB round-trip p50 on M5 lower than both RTX cards under their best Vulkan path by more than run-to-run noise.

- empty round trip: P-M5 213.5 µs vs P-3060 best (empty) 60.4 µs, noise margin ±24.9 % → NOT clearly lower
- 1 MB round trip: P-M5 305.5 µs vs P-3060 best (s2_direct) 142.3 µs, noise margin ±1.9 % → NOT clearly lower

Figures: `fig1_microbench.png` (primary condition), `fig1_modes.png` (all submit × wait modes).
