# M1 microbenchmark report

Runs: `micro_P-M5_run1` (P-M5, AC Power, thermal fair→fair, failures 0), `micro_P-M5_run2` (P-M5, AC Power, thermal fair→fair, failures 0), `micro_P-M5_run3` (P-M5, AC Power, thermal fair→fair, failures 0)

## P-M5 — round trip p50 / p99 (µs), chain / spin

| path | 0 | 4K | 64K | 256K | 1M | 4M | 16M |
|---|---:|---:|---:|---:|---:|---:|---:|
| empty | 214 / 316 | — | — | — | — | — | — |
| s2_direct | — | 201 / 317 | 276 / 319 | 283 / 327 | 305 / 349 | 316 / 493 | 842 / 1814 |
| s1_copy | — | 292 / 344 | 294 / 359 | 305 / 379 | 352 / 442 | 393 / 739 | 1587 / 3001 |

- raw copy bw_d2h 16M: 303.1 µs = 55.36 GB/s
- raw copy bw_h2d 16M: 289.9 µs = 57.87 GB/s

## Run-to-run variation (p50 across process restarts)

- P-M5: 3 runs, 52 cells; cells ≥ 10 %: 26; worst 73.4 % (s1_copy perpass/block 4M); worst in primary condition 29.0 % → FAIL

## Latency modes (empty round trip, all runs pooled)

A single p50 hides multimodal sync latency; histogram peaks (5 µs bins, ≥ 1 % of samples):

- P-M5: g2c: 35–40 (24 %); c2g: 50–55 (8 %), 235–240 (9 %); rt: 85–90 (7 %), 280–285 (11 %)

## Go criterion (protocol §6)

**PENDING** — needs data from P-3060, P-4060.

Figures: `fig1_microbench.png` (primary condition), `fig1_modes.png` (all submit × wait modes).
