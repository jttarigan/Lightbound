# 06 — Research Protocol

## 1. Research question & hypotheses

**RQ:** How does the cost of fine-grained, multiple-round-trip CPU–GPU cooperation within a frame differ between a unified-memory SoC (Apple M5) and discrete GPUs over PCIe, and at what point does it change which game-logic designs are viable?

| ID | Hypothesis |
|---|---|
| H1 | Per-round-trip synchronization latency (GPU→CPU and CPU→GPU) is lower on M5 than on either discrete GPU, for all payload sizes tested. |
| H2 | At K = 1, the discrete GPUs sustain equal or higher agent counts within the frame budget (more bandwidth and compute). |
| H3 | As K increases, the per-frame cost grows more slowly on M5; a crossover K* exists beyond which M5 sustains a higher N at 16.6 ms and 8.3 ms. |
| H4 | Frame-delayed readback (S0) produces measurable decision staleness (mismatch vs same-frame oracle, stimulus-to-reaction delay) that S1/S2 eliminate. |
| H5 | On M5, S2 (zero-copy) is cheaper than S1 (copy) by a margin that grows with N; on PCIe the S1–S2 gap depends on link width (x16 vs x8). |
| H6 (secondary) | Energy per frame at matched N and K is lower on M5 for same-frame strategies. |

The expected **honest** outcome is a crossover (H2 + H3), not a universal win. Report whatever the data shows, including null results.

## 2. Platforms

| ID | Hardware | API | Record |
|---|---|---|---|
| P-M5 | Apple M5 (record exact model, CPU/GPU core counts, RAM, chassis) | Metal | macOS version, power source, Low Power Mode off |
| P-3060 | RTX 3060 Ti, PCIe 4.0 x16 | Vulkan | CPU model, RAM, driver version, ReBAR on/off, **measured** link gen/width |
| P-4060 | RTX 4060 Ti, PCIe 4.0 x8 | Vulkan | same |
| P-M5-VK (optional control) | M5 via MoltenVK | Vulkan | MoltenVK version |

- Run both RTX cards in the **same PC** (same CPU, RAM, OS, driver), one card installed at a time, same slot.
- Verify link: `nvidia-smi --query-gpu=pcie.link.gen.current,pcie.link.width.current --format=csv` while under load (links downshift at idle). Log it in the run header.
- PC: Windows power plan "High performance", GPU in "Prefer maximum performance". Mac: plugged in, no other apps, display on, brightness fixed.
- Capture `bench/sysinfo` automatically at run start (CPU, GPU, driver/OS, memory, link, ReBAR, thermal state).

## 3. Workload control

- **Floors:** 5 fixed seeds from `bench/seeds.txt`, generated at d = 5 size (200×200) regardless of seed to keep geometry cost comparable; floor metrics logged.
- **Agents:** all N spawned at t = 0 (procgen §3 Stage 6 bench rule), nests off, **creatures do not die in bench mode** (hp clamped at 0.01 so N is constant; BURNING visual disabled). Damage is still computed (CPU cost stays).
- **Player:** driven by a deterministic **bench bot** (`src/bench/bot.cpp`): follows the critical path at 70 px/s, sweeps lantern ±40° at 0.5 Hz, and fires a fixed **stimulus schedule** by frame number:

  | Frame (after warm-up) | Stimulus |
  |---|---|
  | every 300 f from 150 | flare thrown 80 px ahead |
  | every 600 f from 450 | strobe |
  | every 400 f from 200 | focus beam 60 f |

  The bot ignores creatures and cannot die (invulnerable in bench mode).
- **Rendering:** fixed internal resolution, light caps, particles, post (R5). Window 1920×1080 windowed, vsync off.
- **Audio:** `--audio=null`.
- **Threads:** `--threads=4` on all platforms.
- **Warm-up:** 600 frames discarded, then 3600 measured frames.

## 4. Condition matrix

Factors:

| Factor | Levels |
|---|---|
| Strategy | S0, S1, S2, S3 |
| K | 1, 2, 3, 4 (S1, S2 only; S0/S3 fixed at 1) |
| N | 1k, 5k, 10k, 20k, 50k, 100k |
| Submit | chain (all); perpass (subset: S2, K ∈ {1, 4}, N ∈ {10k, 50k}) |
| CPU wait | spin (all); block (subset: same as perpass subset) |
| Seed | 5 floors |
| Repetition | 3 per seed (process restart between repetitions) |

Core matrix per platform: (S1,S2)×4 K×6 N + (S0,S3)×6 N = 60 conditions × 15 runs = 900 runs (~1 min each ⇒ ~15–18 h per platform). `tools/analysis/run_matrix.py` executes it resumably, randomizing condition order per repetition block (to decorrelate thermal drift from condition), with a 20 s idle cooldown between runs.

A **reduced matrix** (`--preset=quick`: N ∈ {10k, 50k}, 1 seed, 1 rep) must exist for development sanity checks.

## 5. Metrics

### 5.1 Timing (every run, per frame)
- `frame_ms` (CPU wall clock between frame starts), p50 / p95 / p99 / max, and variance.
- Per GPU pass ms (A, C, C', C'', E, albedo, post).
- Per CPU pass ms (B, D, D', D'', alarm BFS, audio binning).
- Per round trip: `lat_g2c_us`, `lat_c2g_us` (see architecture §8), copy ms (S1).
- `sync_total_ms` = Σ over round trips of wait time on CPU + GPU idle waiting on CPU.
- **Max viable N**: largest N whose p95 frame_ms ≤ budget (16.67 ms and 8.33 ms), interpolated on log N.

### 5.2 Decision quality (audit runs, separate from timing runs)
`--audit=on` runs an **oracle** each frame: after the frame's CPU passes, it performs an extra blocking same-frame readback of full-precision perception (all agents, supersampled light + precise LOS) and re-runs `decide()` on it. Audit overhead distorts timing, so audit runs are never used for timing metrics.

- `decision_mismatch` — fraction of agents whose applied Intent state differs from the oracle's.
- `lit_abs_err` — mean |lit_used − lit_oracle|.
- `wrong_burn` — agents taking damage while the oracle says they are below burn threshold (and vice versa).
- **Stimulus-to-reaction** — for each stimulus, frames from stimulus frame until ≥ 50% of agents inside the stimulus radius have entered FLEE (or STUNNED for strobe). Oracle value is the lower bound.

Audit matrix: all strategies, K ∈ {1..4}, N ∈ {10k, 50k}, 5 seeds × 1 rep.

### 5.3 Energy (secondary)
- Mac: `sudo powermetrics --samplers cpu_power,gpu_power -i 100 -o results/power_<run>.txt`, started/stopped by the harness; report combined package power.
- PC: `nvidia-smi --query-gpu=power.draw --format=csv -lms 100` plus CPU package power (e.g. LibreHardwareMonitor CLI / HWiNFO log) and, if available, a wall meter.
- Metric: mJ per frame = mean power × mean frame time. Compare only at matched (N, K, strategy) and state that platform power domains differ.

## 6. Microbenchmark (M1, go/no-go)

`--mode=microbench`: no game. For each payload size P ∈ {4 KB, 64 KB, 256 KB, 1 MB, 4 MB, 16 MB} and each memory path (S1 copy, S2 direct, plus Vulkan variants `hostcached`, `rebar`, `coherent`):

1. GPU kernel writes P bytes (1 thread per 16 B, trivial arithmetic), signals.
2. CPU waits, reads all P bytes (sums them, to force the read), writes P bytes back, signals.
3. GPU waits, kernel reads P bytes, signals end.

Repeat 2,000 iterations after 200 warm-up; both `--cpuwait=spin|block`; both `--submit=chain|perpass`. Also measure: empty round trip (P = 0, just signal/wait) and raw copy bandwidth.

Report p50 / p99 of round-trip time, g2c and c2g separately, and effective bandwidth. **Go criterion:** the empty-round-trip and 1 MB round-trip p50 on M5 are clearly lower (by more than run-to-run noise) than on both RTX cards under the best Vulkan path. If not, stop and re-scope (paper pivots to staleness + energy + design-pattern contribution).

## 7. Analytic cost model

Fit per platform:

```
T_frame(N, K, S) ≈ a_gpu·N + b_gpu + a_cpu·N + b_cpu
                   + K · ( T_sync + (P_g2c(N) + P_c2g(N)) / BW_eff(S) )
                   + [S1] K · T_copy_setup
```

- `T_sync`, `BW_eff` estimated from the microbenchmark only.
- `a_*`, `b_*` from K = 1 game runs.
- Predict frame time for K = 2..4 and **K\*** (crossover) and max viable N; validate against measured runs (report MAPE). A model that predicts the game from microbenchmarks is a core contribution.

## 8. Analysis & reporting

- `tools/analysis/` (Python): load CSVs → tidy DataFrame → figures + tables.
- Statistics: medians with 95% bootstrap CIs (10k resamples) over runs; Mann–Whitney U / Cliff's delta for pairwise platform comparisons at matched conditions; Holm correction across comparisons.
- Required figures:
  1. Microbench: round-trip latency vs payload (log–log), per platform & path.
  2. **Frame time vs K** at N = 10k / 50k, per platform (the crossover figure).
  3. Max viable N vs K at 16.6 ms and 8.3 ms.
  4. Stacked frame breakdown (GPU passes, CPU passes, sync, copy) per platform at K = 1 and 4.
  5. Decision mismatch & stimulus-to-reaction by strategy (audit).
  6. Cost model predicted vs measured.
  7. (Secondary) energy per frame.
- Every figure regenerates from `results/` with one command: `python tools/analysis/make_figures.py results/ out/`.

## 9. Threats to validity (must be addressed in the paper)

| Threat | Mitigation |
|---|---|
| Different APIs | same Slang shaders & algorithms; optional MoltenVK control run on M5 |
| Different raw GPU/CPU power | report sync fraction and within-platform ablations (S1 vs S2, K scaling), not only absolute fps |
| Thermal throttling (Mac chassis) | warm-up, randomized order, cooldowns, log clocks/thermal state, flag throttled runs |
| OS scheduling jitter | many reps, distributions (p95/p99), process restart per rep |
| ReBAR availability / driver | record, run with ReBAR on and off on PC |
| Behavior design choice | publish `decide()` and all tuning; S3 shows the GPU-only alternative |
| Cross-backend float differences | statistical-equivalence check of simulation outcomes (§10) |
| Bot-driven workload not "real play" | include a short human-play log set (not for statistics, for face validity) |

## 10. Equivalence check across backends

For S2, K = 1, N = 20k, 5 seeds: compare per-frame state occupancy (fraction in each state), mean agent speed, and cumulative damage between P-M5 and P-3060. Pass if the median absolute difference over frames is < 2% of the mean for each. Report it; if it fails, investigate before timing claims.

## 11. CLI (bench-relevant flags)

```
--mode=play|bench|microbench|audit|procgen-dump
--strategy=S0|S1|S2|S3        --k=1..4             --delay=2 (S0)
--agents=N                    --seed=S             --frames=3600   --warmup=600
--submit=chain|perpass        --cpuwait=spin|block --threads=4
--memvariant=default|rebar|hostcached|coherent
--audit=on|off                --audio=on|null|off  --preset=quick|core|audit
--out=path.csv                --tag=free_text      --power=on|off
```

## 12. CSV schemas

Every CSV starts with `#`-prefixed header lines: git commit, `GENERATOR_VERSION`, tuning hash, full CLI, sysinfo (platform, OS, driver, CPU, GPU, RAM, PCIe gen/width, ReBAR), floor metrics, start timestamp.

**`frames.csv`** (one row per measured frame)
```
run_id,frame,strategy,k,n,submit,cpuwait,seed,rep,
frame_ms,cpu_B_ms,cpu_D_ms,cpu_D2_ms,cpu_D3_ms,cpu_alarm_ms,
gpu_A_ms,gpu_C_ms,gpu_C2_ms,gpu_C3_ms,gpu_E_ms,gpu_render_ms,gpu_post_ms,
copy_ms,sync_total_ms,refine_count,alarm_events,throttled
```

**`roundtrips.csv`** (one row per round trip per frame)
```
run_id,frame,rt_index,payload_g2c_bytes,payload_c2g_bytes,lat_g2c_us,lat_c2g_us,cpu_wait_us,gpu_idle_us
```

**`micro.csv`**
```
platform,path,submit,cpuwait,payload_bytes,iter,rt_us,g2c_us,c2g_us,cpu_read_us,cpu_write_us
```

**`audit.csv`** (per frame) and **`stimuli.csv`** (per stimulus)
```
run_id,frame,decision_mismatch,lit_abs_err,wrong_burn_pos,wrong_burn_neg
run_id,stimulus_id,kind,frame,agents_in_radius,reaction_frames,oracle_reaction_frames
```

**`runs.csv`** (one row per run, summary): all conditions + p50/p95/p99 frame_ms, means of every component, max-viable flag at 16.6/8.3 ms, energy mJ/frame if available.

## 13. Data & reproducibility

- Tag the exact commit used for the paper; archive `results/`, analysis scripts and a built binary per platform (e.g., Zenodo).
- `docs/REPRODUCE.md` (create at M8): hardware list, setup steps, the exact commands to regenerate every figure.
