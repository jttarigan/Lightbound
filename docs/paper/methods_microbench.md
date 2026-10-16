# §4 Microbenchmark method (draft, 2026-10-16)

<!-- Sources: docs/06 §6, docs/05 §4 and §8, DECISIONS #17–#32, docs/PC_REPORT.md §1, Mac CSV headers
     (results/m1_mac_ac). Every fact below is from those; nothing is measured here. -->

## 4.1 What one iteration measures

One iteration is a CPU–GPU round trip of payload *P* bytes:

1. A GPU kernel writes *P* bytes (one thread per 16 B, trivial arithmetic) and signals a timeline value.
2. The CPU waits for that value, reads all *P* bytes (summing them so the read cannot be elided), writes *P*
   bytes back and signals the next value.
3. A second GPU segment, which was submitted ahead and waits on that value, reads the *P* bytes.

The **round trip** `rt` is the time from the end of the GPU write pass to the start of the GPU read pass, both GPU
timestamps converted to the CPU clock. It therefore contains the GPU→CPU hand-off (`g2c`: GPU signal → CPU wake),
the CPU's read and write, any copy the path needs, and the CPU→GPU hand-off (`c2g`: CPU signal → consuming pass
starts). The GPU's own read pass is outside `rt` and is reported separately (`gpu_read_us`). `P = 0` gives the
empty round trip, a pure signal–wait exchange.

Each cell (path × payload × submit mode × CPU-wait mode) runs 200 warm-up and 2,000 measured iterations; a
separate cell measures raw copy bandwidth. Every iteration is verified: the CPU's sum must equal the pattern the
GPU wrote, the GPU's checksum must equal the pattern the CPU wrote, and the count of research-buffer copies must
be 2 for the copy path and 0 for the direct paths (invariant R7). Every reported series had 0 failures.
<!-- DECISIONS #25; the CSV header records `failures:`. -->

## 4.2 One algorithm, two APIs

Both backends implement the same thin interface: timeline objects with GPU signal and wait, CPU signal and wait,
memory classes, GPU timestamps and clock calibration. Kernels come from a single Slang source, compiled to MSL
and SPIR-V, with identical thread-group sizes and buffer layouts checked by a unit test (R1).

| | Metal (M5) | Vulkan 1.3 (RTX) |
|---|---|---|
| timeline object | `MTLSharedEvent` | timeline semaphore |
| GPU waits / signals | `encodeWait` / `encodeSignalEvent` | submit wait / signal values |
| CPU waits | spin on `signaledValue`, or blocking `waitUntilSignaledValue` | spin on `vkGetSemaphoreCounterValue`, or `vkWaitSemaphores` |
| CPU signals | `setSignaledValue` after a release fence | `vkSignalSemaphore` after flushing non-coherent ranges |

Each GPU segment is one command buffer on both backends (DECISIONS #22). Two submission modes are measured:
`chain` (all segments of an iteration submitted ahead, each waiting on the previous value) and `perpass` (each
segment submitted when its turn comes). CPU waiting is either `spin` or `block`. The primary condition is
chain/spin.

## 4.3 Memory paths

| path | GPU-written, CPU-read | CPU-written, GPU-read |
|---|---|---|
| `s1_copy` | device memory + GPU copy into host-visible staging | host-visible staging + GPU copy into device memory |
| `s2_direct` (Metal) | `MTLStorageModeShared`, never blitted | `MTLStorageModeShared` |
| `s2_direct` (Vulkan) | `HOST_VISIBLE \| HOST_CACHED` system memory | `DEVICE_LOCAL \| HOST_VISIBLE` (ReBAR) when available, else host-coherent system memory |
| `s2_hostcached` (Vulkan) | host-cached system memory | host-cached system memory |
| `s2_rebar`, `s2_coherent` (Vulkan) | sensitivity paths with both directions in that class | same |

The CPU never reads ReBAR memory on the default path: uncached CPU reads across PCIe would be a strawman
(DECISIONS #17). `s2_rebar` measures that slow path deliberately. Which variant was in effect is recorded in each
CSV header.

## 4.4 Timing and clock correlation

GPU timestamps are taken at encoder boundaries on Metal (the M5 does not support dispatch-boundary sampling) and
with `vkCmdWriteTimestamp2` directly around each command on Vulkan. GPU time is mapped to the CPU's monotonic clock
by `sampleTimestamps` (Metal, `mach_absolute_time` domain) or calibrated timestamps (Vulkan, QPC on Windows), plus a
bracketed offset to `steady_clock`. The mapping is refreshed at the start of every cell and by elapsed time during
it (about every 130 iterations for fast cells): with a mapping refreshed only every 500 iterations, the PC's g2c and
c2g drifted while `rt` stayed valid (DECISIONS #23, #30). A drift check (g2c median of the first vs last 50
iterations of each block) shows no drift on the Mac, and on the PC the block drift fell from 2.1 to 0.2 µs (empty)
after the fix.

## 4.5 GPU state: idle and hot

As specified, the microbenchmark runs with the GPU otherwise idle. A real frame keeps the GPU busy with
independent work, and on the M5 the idle GPU's power state dominates `c2g` (Section 5). The extension therefore
measures every cell a second time with the GPU **hot**: a second queue runs a calibrated compute kernel
(one SIMD group, a dependent multiply-add chain, ≈ 200 µs per dispatch, two in flight) back to back for the whole
cell (DECISIONS #32). The load is the lightest one that holds the power state: a probe showed that a load filling
the whole GPU changes the result by only ≈ 3 µs. A hot cell is valid only if its load duty (dispatches ×
calibrated time / cell wall time) is ≈ 1. Idle cells always run before hot cells, because idle blocks measured
after loaded ones come out faster (hysteresis).

## 4.6 Payload grid, repetitions, acceptance

Protocol grid: 4 KiB, 64 KiB, 256 KiB, 1 MiB, 4 MiB, 16 MiB, plus the empty round trip. Extension grid for the
crossover (H1′): 4K, 64K, 256K, 1M, 1.5M, 2M, 3M, 4M, 8M, 16M, 32M, 64M, on `empty`, `s1_copy`, `s2_direct`
(and `s2_hostcached` on Vulkan), idle and hot. Each series is three process restarts. A series is accepted when
every cell's p50 varies by less than 10 % across the three runs. Medians and p99 are pooled over runs. Extension
series are kept apart from the protocol series and never pooled with them.

## 4.7 Platforms (cross-reference to §3)

- **P-M5:** MacBook Air 15" (Mac17,4), Apple M5 (4P + 6E CPU cores, 10 GPU cores), 24 GiB unified memory,
  macOS 26.6.2, on AC power, Low Power Mode off, fanless chassis.
- **P-3060:** Intel Core i7-12700F, 32 GB, RTX 3060 Ti 8 GB, driver 566.36, Windows 11, power plan Ultimate
  Performance, GPU "Prefer maximum performance". The slot runs at **PCIe gen3 x16** (measured under load in every
  run); the measured link is the platform definition. ReBAR on (BAR heap 8024 MB) is the primary condition, and
  ReBAR off is a separate series.
- **P-4060:** RTX 4060 Ti, same PC and slot, gen3 x8 expected (pending, November).

<!-- TODO before submission: state the 3060 Ti's measured raw bandwidth here or in §5; Mac thermal state was
     "fair" through the AC series; mention that the Mac is fanless (thermal threat, §12). -->
