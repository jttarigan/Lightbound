# Decisions log

Ambiguities resolved while building, per CLAUDE.md rule 4. Newest at the bottom.

## 2026-09-23 — M0 scaffold

1. **Perception field order changed (spec §05 lists `lit` first).** Under std430 (SPIR-V) a
   `float2` is 8-byte aligned, so `float lit; float2 lit_grad;` yields 40 B, while Slang's
   Metal "natural" layout packs `float2` at 4 B and yields 32 B — a cross-backend layout
   mismatch (violates R1). Moving `lit_grad` to offset 0 gives 32 B with identical offsets on
   both targets. Field set and semantics unchanged. Verified with slangc reflection for both
   targets (the build now fails on any such mismatch, see `tools/gen_layouts.py`).
2. **GPU struct layout rules** (in `shaders/shared/layouts.slang`): no 3-component vectors;
   float2/uint2 at 8-aligned offsets; float4/uint4 at 16-aligned offsets; constant-buffer
   structs use only 16-byte fields so std140 == natural. `FrameConsts` is therefore four
   16-byte vectors for now (grows in M4). C++ `float2`/`float4` carry `alignas(8/16)`.
3. **GpuEvent** (unspecified in §05 beyond "contacts, beam→sigil hits") defined as
   `{float2 pos; uint kind, a, b, pad}` = 24 B; `GpuEvents` header uses four scalar uints.
4. **Toolchain on the Mac:** no Homebrew present; CMake 4.4.3 and Ninja 1.13 installed with
   `pip3 install --user cmake ninja` (in `~/Library/Python/3.9/bin`, must be on PATH).
5. **metal-cpp** fetched from Apple's `metal-cpp_macOS15_iOS18.zip` (no macOS 26 archive is
   published); compiles against the macOS 26 SDK / Xcode 27. Slang pinned at v2026.18.2
   (binary release), SDL3 release-3.4.16 (built static), doctest v2.5.3, miniaudio 0.11.25,
   Vulkan-Headers vulkan-sdk-1.4.357.0, VMA v3.4.0, MoltenVK v1.4.2.
6. **Vulkan on macOS:** when no Vulkan SDK is found, the vulkan backend links
   `libMoltenVK.dylib` directly (MoltenVK release tar) and passes its path to
   `SDL_Vulkan_LoadLibrary`. This is the optional P-M5-VK control condition. Validation
   layers are only available with the SDK (PC).
7. **Shader blobs** are compiled offline to `<build>/shaders/<pass>.spv` (always) and
   `<pass>.metallib` + `.metal` (macOS) and loaded at runtime from `<exe dir>/shaders/`.
8. **Extra dev-only CLI flags** beyond protocol §11: `--vsync`, `--validation`, `--width`,
   `--height`, `--exit-after=N` (play mode auto-quit for automated checks), `--log`,
   `--help`, `--version`. Protocol flags are unchanged. `normalizeOptions` forces K=1 for
   S0/S3 and defaults vsync to on only in play mode.
9. **Warnings:** `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion ... -Werror`
   on project targets; third-party code is included as SYSTEM. `-Wold-style-cast` is not used
   because Vulkan/CGSize macros expand to C casts in our TUs.
10. **Job system** is a fixed pool with a single active `parallelFor` at a time (caller
    participates as worker 0, stable chunk indices). Sufficient for pass B/D; no work-stealing.

## 2026-09-24 — M0 build & verification

11. **CLI enum-name helpers renamed `toString` → `cliName`.** doctest looks up `toString(x)`
    by ADL to stringify operands in failed assertions; our `const char* toString(Mode)` hijacked
    that lookup and broke compilation of `CHECK(o.mode == Mode::Play)`. The new name avoids
    the clash; behaviour is unchanged.
12. **`--validation=on` on Metal re-executes the process** with `MTL_DEBUG_LAYER=1` and
    `MTL_DEBUG_LAYER_ERROR_MODE=assert`. Metal reads these at framework load, so setting them
    from inside `init()` has no effect. The re-exec lives entirely in `MetalGfx.cpp` (uses
    `_NSGetArgv`), so `Gfx.h` is unchanged. An externally set `MTL_DEBUG_LAYER` is respected
    (no re-exec). With `assert` mode any API validation error aborts the process, so "exit 0"
    means "zero validation errors".
13. **Link lines:** executables link only their direct dependencies (`lb_core`/SDL3 come
    transitively); listing `lb_core` twice made Apple's ld print "ignoring duplicate
    libraries", which breaks the warning-free requirement.
14. **Vulkan-Headers and VMA are fetched as tag tarballs with `URL_HASH SHA256`** instead of
    `GIT_SHALLOW` clones. CMake's shallow clone uses `--no-single-branch`, which for
    Vulkan-Headers pulls every tag tip and ran for 15+ minutes; the tarball is 3.4 MB. Same
    tags as before (vulkan-sdk-1.4.357.0, v3.4.0), and now integrity-pinned as well.
15. **Vulkan loader loading:** `SDL_CreateWindow(SDL_WINDOW_VULKAN)` already loads the default
    loader, so `VulkanGfx::init` only passes its own library path (MoltenVK on macOS without
    SDK) when nothing is loaded yet, and otherwise takes a reference on the loaded one. The
    resolved loader file is logged (`Vulkan loader: ...`) so every run records which loader/ICD
    it used. On macOS the default search resolves to the linked MoltenVK dylib.

## 2026-09-24 — Milestone ordering

16. **M0 [PC] acceptance deferred (human decision).** M0 is accepted on [Mac] for Metal, plus the
    optional Vulkan-via-MoltenVK build. The [PC] checks (MSVC build, ctest, `--mode=play
    --validation=on` with the Khronos validation layer) are deferred and will run in the same PC
    session as M1's microbenchmark runs. M1 can be built and run on the Mac meanwhile, but M1
    cannot be closed (and the go/no-go decision cannot be made) until both PC runs, M0 and M1,
    have passed.

## 2026-09-24 — M1 round-trip microbenchmark

17. **Vulkan S2 never lets the CPU read ReBAR memory by default** (docs/05 §4.2 asks for this
    note). `DEVICE_LOCAL|HOST_VISIBLE` memory is uncached write-combined memory behind PCIe; CPU
    reads from it run at a small fraction of normal bandwidth, so making the default S2 path read
    Perception from ReBAR would be a strawman. The default S2 path is: GPU→CPU in
    `HOST_VISIBLE|HOST_CACHED` system memory, CPU→GPU in ReBAR when available (≥ 256 MB BAR heap),
    else `HOST_VISIBLE|HOST_COHERENT`. The slow path stays measurable as the `s2_rebar` variant.
    One table (`src/frame/MemoryPolicy.cpp`) is used by both the microbench and (M4) the frame
    orchestrator.
18. **Shader binding convention:** a pass declares its RW/structured buffers first (Vulkan
    set 0 binding i, Metal `[[buffer(i)]]`, i.e. declaration order), then at most one
    `[[vk::push_constant]]` block. Slang maps the block to Vulkan push constants and to Metal
    `[[buffer(bufferCount)]]`, which the Metal backend fills with `setBytes` (verified from the
    generated MSL and SPIR-V reflection). Per-dispatch parameters therefore need no ring buffers.
19. **Microbench runs headless** (no window, no swapchain, no present): protocol §6 says "no
    game", and presenting would add unrelated GPU work and compositor interaction.
20. **micro.csv**: the 11 columns of protocol §12 come first and unchanged; appended columns are
    `copy_g2c_us, copy_c2g_us, gpu_write_us, gpu_read_us` (needed for bandwidth and cost-model
    analysis). Extra row types: `path=empty` (P = 0, "just signal/wait") and `path=bw_d2h|bw_h2d`
    (raw GPU copy bandwidth; `rt_us` = copy time, submit/cpuwait = `na`). Definitions:
    `rt` = GPU timestamp at start of the consuming kernel − GPU timestamp at end of the producing
    kernel (both GPU clock, calibration-independent; includes S1 copies, CPU read + write and both
    sync hand-offs); `g2c` = CPU wake − producing-kernel end; `c2g` = consuming-kernel start − CPU
    signal (both via the calibrated clock mapping); `cpu_read`/`cpu_write` = CPU time to sum /
    write all P bytes. Payload sizes are binary (4 KiB … 16 MiB).
21. **Vulkan sensitivity paths** for the microbench: `s2_hostcached`, `s2_rebar`, `s2_coherent`
    put *both* directions in that memory class (protocol §6 names the variants but not their
    per-direction placement). On unified-memory Vulkan (MoltenVK) there is no uncached host memory,
    so `HostVisibleCoherent` maps to the coherent (cached) type.
22. **Segments are one command buffer each on both backends** (Metal: `encodeWait` at the start,
    `encodeSignalEvent` at the end; Vulkan: one `VkSubmitInfo2` with wait/signal). Chain mode
    commits all of a frame's segments before the CPU waits; perpass encodes and submits a segment
    after the CPU pass and its signal. A probe (session scratch) showed that putting both Metal
    segments in one command buffer does not change the CPU→GPU latency, so identical segmenting
    costs Metal nothing.
23. **Timestamps & clock correlation.** Metal: counter sample buffers with stage-boundary sampling
    (start/end of each encoder; the M5 does not support dispatch-boundary sampling). Vulkan:
    `vkCmdWriteTimestamp2` at the pass's own stage (COMPUTE_SHADER / COPY) immediately before and
    after the command; submissions wait with `ALL_COMMANDS` so the first timestamp also waits.
    GPU→CPU mapping: Metal GPU timestamps and `sampleTimestamps` CPU values are
    mach_absolute_time nanoseconds; `steady_clock` on macOS is a different domain (it counts sleep
    time: ~52 s apart on this machine), so the mach→steady offset is measured by bracketed reads.
    Vulkan uses `VK_KHR/EXT_calibrated_timestamps` (QPC on Windows, CLOCK_MONOTONIC_RAW elsewhere)
    and the same bracketed host-domain→steady offset. Recalibrated at the start of every cell and
    every 500 iterations.
24. **Vulkan backend requires Vulkan 1.3** with `timelineSemaphore`, `synchronization2` and
    `hostQueryReset` (all RTX 30/40 drivers and MoltenVK 1.4 have them). 1.2/1.3 entry points are
    fetched with `vkGetDeviceProcAddr` so the MoltenVK-direct build does not depend on exports.
25. **Verification every iteration** (M1 acceptance): the CPU sums all words it read and compares
    with the closed pattern of `micro_write`; `micro_read` writes per-group partial sums (no
    atomics, deterministic) that must equal the CPU pattern's checksum; the R7 research-copy
    counter must advance by exactly 2 per iteration on `s1_copy` and 0 elsewhere. The expected
    values are computed after the measured intervals. Any mismatch fails the run (exit 1).
26. **`--frames`/`--warmup` in microbench mode** mean measured / warm-up iterations per cell and
    default to 2000 / 200 (protocol §6). Submit and CPU-wait modes are swept unless `--submit` /
    `--cpuwait` are given. Dev filters `--micro-paths`, `--micro-payloads` select subsets.

## 2026-10-02 — M0 [PC] MSVC build (Windows 11, VS Build Tools 2022 17.14, MSVC 19.44)

27. **`alignUp` is one constrained template** (`requires std::is_unsigned_v<T>`) instead of the
    `u32` / `usize` / `u64` overloads. On MSVC x64 `size_t` and `uint64_t` are both
    `unsigned long long`, so the `usize` and `u64` overloads were the same function (C2084,
    only in the test TUs that include `Math.h` via `Arena.h`). Clang on macOS keeps them distinct
    (`unsigned long` vs `unsigned long long`), which is why it never showed there. Both callers
    (`Arena::alloc`, `test_math`) pass same-typed unsigned arguments; behaviour unchanged. This
    was the only MSVC `/W4 /WX` failure: the rest of the tree (Vulkan backend included) compiled
    warning-free on the first MSVC build.

## 2026-10-05 — M1 [PC] interruptible runs (human request)

28. **Microbench runs can be interrupted and resumed** (`lightbound --resume`,
    `tools/run_microbench.py --resume`). With the full `s2_rebar` protocol one run on the PC takes
    about a day (the 16 MiB `s2_rebar` cells alone are ≈ 4.3 h each), and the human needs to be able
    to switch the PC off. What a resumed run keeps and what it changes:
    - The unit of resumption is the **cell** (path × submit × cpuwait × payload). A cell was already
      measured independently (own warm-up, own clock calibration) and written to the CSV as one
      block followed by a flush; nothing inside a cell or inside a timed interval changed.
    - `--resume` reads the CSV in `--out`, keeps the cells that have exactly `iterations` rows,
      drops the rows of a cell that was being written when the process died, and measures only the
      missing cells, in the usual order. It refuses to continue (exit 2) if the binary
      (`version`, `git_commit`), the options, the tag or any `sysinfo.*` value (GPU, driver, ReBAR,
      BAR heap, power plan, …) differs from the header of the interrupted run; only a different
      PCIe link reading is a warning. A file without a complete header is started over.
    - Every process still starts with the raw copy bandwidth pass (it ramps the PCIe link up for
      the link query); a resuming process discards those rows because the file already has them.
      Path buffers are created as usual (same sizes) for a path that still has cells to measure.
    - The CSV gets one `# resumed: <UTC time>, <n> complete cells kept, …` line per restart, at the
      position where the restart happened, so analysis can tell which cells ran after a restart
      (cold machine, new process). The header (`start`, `cli`) stays that of the first process.
    - Verification/R7 failures of a cell are written as `# cell_failures: path,submit,cpuwait,
      payload,count` in front of that cell's rows (only when non-zero), and a resuming process
      adds them to its own count, so `# failures:` and the exit code still cover the whole run.
      API errors are per process: check every part of the log for `ERROR` lines.
    - Runner: an unfinished run stays in `_running_run<i>.csv/.log`; the runner then stops with
      exit 3 instead of renaming a partial CSV to its final name. Without `--resume` it refuses to
      start in a directory that holds such a file. With `--resume`, finished runs are skipped.
    - A resumed run is therefore one protocol "process run" split over several processes at cell
      boundaries. Run-to-run variation is still computed per run file.
29. **Runner log and report encoding.** `run_microbench.py` now sends lightbound's output straight
    to the run's log file instead of collecting it in memory (the log of an interrupted run was
    lost otherwise); `printSummary` flushes stdout so the table still precedes the closing
    `microbench done` line. `micro_report.py` reads and writes UTF-8 explicitly: on Windows the
    default code page (cp1252) cannot encode "→", which left `report.md` empty after the first PC
    series.

## 2026-10-05 — M1 clock mapping (human decision; amends #23)

30. **The GPU→host clock mapping is refreshed by elapsed time, not only by iteration count.**
    Finding (first PC series, RTX 3060 Ti, Vulkan calibrated timestamps, QPC): the GPU timestamp
    clock and the host clock drift apart by ≈ 32 ppm, and the Vulkan mapping uses the nominal
    `timestampPeriod`. With a recalibration only every 500 iterations (#23), `g2c` fell and `c2g`
    rose linearly inside every calibration block by the same amount and jumped back at the next
    recalibration. `rt` (GPU clock only) and `g2c + c2g` were not affected, but the split was:
    ≈ 1 µs off for `empty`, ≈ 3 µs at 1 MiB, wrong at 16 MiB (`s2_direct` g2c p50 16 µs instead of
    57 µs, 34 % of samples negative), milliseconds off for `s2_coherent`, and it would have been
    ≈ 100 ms off for `s2_rebar` 16 MiB (7 s per iteration).
    Change, in `Microbench::runCell` only (shared code, nothing inside a timed interval):
    - before an iteration the clocks are recalibrated when `it % 500 == 0` (as before) **or** the
      last calibration is older than 20 ms (`kMaxCalibrationAgeNs`; 20 ms × 32 ppm = 0.64 µs);
    - `g2c` uses that mapping (the hand-off is at the start of the iteration); if the calibration
      is older than 20 ms when the iteration ends (long CPU read/write), the clocks are calibrated
      again and the consuming kernel's start timestamp is read again for `c2g` (the hand-off at
      the end). `rt` is still computed from one mapping, i.e. purely on the GPU clock. For such
      iterations `rt = g2c + cpu_read + cpu_write + c2g` no longer holds to the nanosecond.
    Check on the PC (old schedule vs new, three interleaved process runs each, chain/spin):
    `rt` p50 within ±1.3 % for every cell (run-to-run noise), block drift of g2c 2.1 → 0.2 µs
    (`empty`) and 74 → 0 µs (16 MiB), no negative samples. Fast cells now recalibrate about every
    130 iterations instead of 500; cells slower than 20 ms per iteration recalibrate every
    iteration.
    Metal: GPU timestamps and `sampleTimestamps` share mach_absolute_time and the backend already
    fits a slope, so no drift is expected there and the Mac results should not move; to check any
    CSV, compare the g2c median of the first and last 50 iterations of each block
    (`(iter + warmup) % 500`). The first PC series (binary `6a2d02e`, kept as
    `results/m1_3060_rebar-off_old-calibration`) has valid `rt` and biased g2c/c2g.

## 2026-10-13 — M1 [PC] report pooling

31. **`micro_report.py` counts a run once when it exists both as `.csv` and as `.csv.gz`.** The
    report pools every `micro*.csv` and `micro*.csv.gz` below a directory. On the PC the raw CSVs
    stay next to their gzipped copies (the `.gz` is what gets committed), so after the ReBAR-on
    series had been gzipped, the runner's report pooled runs 2 and 3 twice (`n` = 10000 instead
    of 6000 per cell); the per-run variation check was unaffected, the pooled p50/p99 were weighted
    towards those runs. `load()` now drops `x.csv.gz` when `x.csv` is present. The committed
    `results/m1_3060/report` and `results/m1_pc_report` were rebuilt with the fix and are
    identical to a build from the `.csv.gz` files alone (what a fresh checkout has). The other
    PC reports were built before gzipping and were never affected.

## 2026-10-13 — M1 hot-GPU cells (built behind a flag; protocol text unchanged until the human decides)

32. **`--mode=microbench` can measure every cell with the GPU kept busy** (`--micro-gpuload=idle|hot|both`,
    default `idle` = protocol §6 as written). Why: finding 7 (STATUS) showed that the microbench as
    specified measures a *cold* GPU, whose CPU→GPU hand-off on the M5 sits at power-state levels of
    ≈ 45 / 155 / 235 µs, while a real frame keeps the GPU busy with independent work (docs/05 §3), where
    the same hand-off is ≈ 31 µs and stable. Whether the hot cells count for the §6 go criterion is the
    human's decision (STATUS to-do 1a); until then the default run is unchanged and the report treats hot
    cells as information. What the option does:
    - **Load definition** (same on both backends, R1): a second, independent GPU queue runs `micro_burn`
      (one group of 32 threads, i.e. one SIMD group, a dependent multiply-add chain) back to back with 2
      dispatches queued ahead, from before a hot cell's warm-up until after its last iteration. The step
      count is calibrated per process so that one dispatch takes ≈ 200 µs (wall clock with 2 in flight;
      written to the header as `load.steps`, `load.dispatch_us`). The probe (finding 7) showed that a
      load filling the whole GPU changes the result by only ≈ 3 µs, so the lightest continuous load was
      chosen: it keeps the GPU's power state up and competes least with the measured kernels.
    - **Backends** provide only `Gfx::loadDispatch` / `loadWait` on a second queue (`src/gfx`); the
      pacing thread and the calibration are shared code (`src/bench/MicroLoad.*`). Metal: a second
      `MTLCommandQueue`. Vulkan: a second queue of the main family when it has one (NVIDIA: 16), else
      the first other compute-capable family (MoltenVK exposes one queue per family, so its compute
      family is used); same priority as the main queue, because Metal queues have no priorities either.
      The queue is recorded as `sysinfo.second_queue`. Without a second queue, `hot` exits 2 and `both`
      skips the hot cells with a warning (`load.available: no`).
    - **Cells and order:** hot cells are the same paths × payloads × submit × cpuwait, written with the
      path name suffixed `_hot` (`empty_hot`, `s2_direct_hot`, …), so the §12 columns, `--resume` and the
      pooling rules need no change. In `both` mode every idle cell runs first, then the hot cells: the
      probe showed that idle blocks measured next to loaded blocks come out faster (hysteresis), so idle
      must never follow hot.
    - **Validity record:** each hot cell gets a `# cell_load: path,submit,cpuwait,payload,dispatches=N,
      duty=D` note in front of its rows (dropped with the rows on `--resume` when the cell is incomplete).
      Duty = dispatches × calibrated dispatch time / cell wall time; ≈ 1 means the second queue never
      idled. A load failure counts as a cell failure (the cell was not hot); duty < 0.5 is a warning.
      The load thread wakes once per dispatch (≈ every 200 µs, one wait + one submit) on another core
      while the main thread spins; nothing inside a timed interval changed.
    - **Report:** `micro_report.py` splits the `_hot` suffix into a `gpuload` column (summary.csv,
      variation.csv), prints hot tables, `fig1_hot.png` and the load duty per platform, checks
      run-to-run per (platform, gpuload), evaluates the §6 criterion on the idle cells only and prints
      the same comparison for the hot cells as information. Idle-only reports are unchanged (checked on
      `results/m1_report`: identical `report.md`, `summary.csv` and `variation.csv` apart from the new column).
    - **Verified:** ctest 40/40 on Metal and MoltenVK (new `microbench_smoke_hot`: idle and hot cells of
      `empty` and `s2_direct`, validation on, checksums and R7 counts every iteration). In those
      30-iteration smoke runs (not measurements) the M5's empty round trip was ≈ 77 µs hot vs ≈ 204 µs
      idle on Metal, duty 1.00; MoltenVK 99 vs 218 µs, duty 1.01.
    - **Protocol:** docs/06 §6 is not edited. The text to add if the human approves, and the proposed
      payload grid for the crossover study, are in `docs/M1_RESCOPE_PROPOSAL.md`.

## 2026-10-16 — M1 go/no-go (human decision)

33. **M1 is NO-GO on H1; the study is re-scoped to the payload crossover (H1′).** The human approved
    `docs/M1_RESCOPE_PROPOSAL.md` §1 as recommended ("go as you recommend"):
    - **0.** H1 is refuted by the §6 criterion (idle GPU, ReBAR on and off). Instead of the pivot named in
      §6, H1 is replaced by H1′, written on 2026-10-13 before any new measurement, with its predictions;
      H2–H6 and the §7 cost model are unchanged. Protocol §1 and §6 are edited accordingly.
    - **a.** The hot-GPU cells (#32) and the 12-point payload grid are measured as an extension
      (`results/m1_mac_ext`, `results/m1_3060_ext`), in their own directories. The proposal left open which
      GPU state the go criterion uses; since the go/no-go is now decided, the criterion stays on the idle
      cells as written (what `micro_report.py` already does), and H1′ is evaluated on idle and hot
      separately.
    - **b.** The measured PCIe gen3 link is the platform definition (gen3 x16 for the 3060 Ti, gen3 x8
      expected for the 4060 Ti).
    - **c.** ReBAR on, `s2_direct`: `rt` as defined is primary, `rt` + GPU read is secondary.
    - **d.** The repository stays public for now (anonymous HTTPS is the Mac's only fetch path while SSH
      to GitHub times out).
    - **4060 Ti:** the human can't install it before November, so step D waits until then. M1's
      acceptance line "[PC with 4060 Ti]" stays open; that deviation is accepted, because the second card
      can't change the go/no-go (§6 needs the M5 below both cards) and is needed only for H5.
    - **Next milestone:** M2 (procedural floors, CPU only) may start before the extension is measured;
      the proposal's "M2 (the agent system)" meant the later agent milestones. The N levels that bracket
      the crossover matter from M4 on (§4 already spans 1k–100k, 1 MiB ≈ 37k agents).

## 2026-10-16 — M2 procedural floors: spec interpretations

34. **Where docs/02 leaves a choice, the generator does the following** (`src/procgen/`). All layout
    decisions are integer-only, and `lb_procgen` builds with FP contraction off (`-ffp-contract=off`,
    MSVC `/fp:precise`), so floors are bit-identical with clang and MSVC (§7 golden hashes).
    - **Rooms:** dart throwing on the integer grid, 14-tile margin, ≈ (w·h)/900 rooms (fewer if the
      darts run out; < 8 re-rolls). **Delaunay** by the empty-circumcircle test with exact integer
      predicates (n ≤ ≈ 50; cocircular ties keep every triangle). **MST** by Kruskal on (d², a, b);
      loops = round(15 % of the remaining Delaunay edges), chosen by a seeded shuffle.
    - **Roles:** START = lowest degree among the quarter of rooms nearest the map edge (seeded ties).
      EXIT = most hops from START (ties: farther in space). Antechamber = EXIT's neighbor nearest
      START. **On seal floors the antechamber is also the PUZZLE room**: EXIT is almost always a
      leaf, so it has no second neighbor. NEST: hops ≥ 2 (relaxed to 1 if short), degree first, then
      hops. CACHE: 3–5 random caves.
    - **Caves:** CA fill 45 % inside 0.8 R, rising linearly to 70 % at R ("radial falloff" is not
      quantified; a falloff to 100 % from 0.6 R left rooms ≈ 4–5 tiles across). Room cores
      (d² ≤ 5) and corridor centerlines are protected from the smoothing passes, so every room
      center stays connected. Corridor walk: 70 % one step toward the target (axis chosen in
      proportion to the remaining |dx|, |dy|), else one random 4-way step; after 4·manhattan + 64
      steps it finishes straight.
    - **Water:** 40 % of caves in Mossy Hollow ("many pools"), 20 % elsewhere. **Chasms** (Abyss):
      35 % of caves, radius 2–3, placed after the connectivity cleanup and kept only if every
      walkable tile stays reachable. The cleanup turns every open tile not connected to START into
      wall, which also covers "fill tiny enclosed islands".
    - **SDF:** signed (+ open, − inside walls), the distance from each sample center to the nearest
      tile square of the other class. Tile EDT argmin → samples (boundary tiles also try their
      neighbors) → jump flood over offsets 4, 2, 1. Wall samples more than 3 tiles from open space
      keep the EDT candidate. Tested against brute force: exact on > 99.9 % of samples, worst
      error < 1 px (jump flood far from walls).
    - **Lights:** brazier range sdf ∈ [1.5, 3] tiles at the tile center (else any floor tile of the
      room); corridor braziers at least 10 tiles from every brazier. Priorities: 0 START brazier
      and puzzle crystal, 1 room braziers, 2 crystals, 3 corridor braziers; the cap drops 3, then 2,
      then 1, last generated first. Pre-lit = round(fraction × braziers after the cap), START
      included. Crystal clusters (2–4 wall tiles, one 40 px light each): 50 % of rooms in the
      Crystal biomes, 0–3 elsewhere. Ember Deep's glowing cracks are deco, not static lights.
    - **Seal puzzle:** a mirror is a line with 4 orientations and rotates one way in +45° steps,
      so it needs 0–3 presses. A beam reflects to 2m − d, so mirrors make 90° turns. Diagonal steps
      are blocked only between two walls. The path is built backwards from a sigil wall of EXIT
      (farthest from the PUZZLE room first) with 2 (d3) or 3 (d5) mirrors, segments ≥ 2 tiles. The
      crystal source is a floor tile of the PUZZLE room. A failed search or > 8 rotations re-rolls
      the floor (≈ 5 % of seal floors, seeds 1..200).
    - **Validation:** rule 1 uses 4-connected walkable tiles. Rule 5 uses consecutive 60-tile chunks
      of that path, with a lit brazier ≤ 30 tiles (Euclidean) from any tile of the chunk. **Rule 6
      target** = the median open fraction (floor + water + chasm) of seeds 1..200 per GDD map size:
      21.4 / 22.2 / 22.8 / 23.4 / 23.5 % for 120…200, linear in between. ≈ 2 % of floors re-roll
      on it.
    - **Spawns:** dark pockets are walkable tiles of rooms ≥ 3 hops from START with no static
      light (lit or not) within 120 px. Nest zones are walkable tiles within 3 tiles of the nest.
      lit_area_fraction uses light radius only (no occlusion).
    - **Tooling:** `--depth=1..5` (in the options header only when given); `procgen-dump` prints one
      result line (hash + §6 metrics) and writes the PNG. stb has no release tags and is pinned to
      commit 2c980bb. `kGeneratorVersion` stays 1, since this is the first generator. Tests: determinism
      and validity cycle depth = 1 + seed % 5; the golden file holds the bench seeds × depths 1..5.

35. **2026-10-17 — A 20 W adapter counts as "plugged in" for Mac runs (protocol §2).**
    - **Question (Main, 06:37 WIB):** the only charger here is 20 W; under load the battery may slowly drain. Does
      `m1_mac_ext` still meet §2?
    - **Decision: yes, with guards.** §2 asks for "plugged in". With the 20 W adapter macOS reports
      `AC Power`, which is what the run records (`sysinfo.power_source`), so the OS power policy is the AC one; the
      battery only covers peaks above 20 W. What §2 protects against is the battery power state, i.e. running
      unplugged (declined 10-16), not the adapter's wattage.
    - **Guards:** start at ≥ 50 % battery; Low Power Mode off; lid open; `caffeinate -is`; log `pmset -g batt` and
      `pmset -g ac` (wattage) at the start and the end. A run whose power source leaves `AC Power` is invalid.
    - **Adapter for `m1_mac_ac` (10-13): unknown** (Main, 10-17: pmset logs don't record the wattage). The report
      lists it as unknown; from `m1_mac_ext` on, the wattage is logged.
    - **Lab scheduling (Main, 10-17 morning):** the run is on hold while Main asks the user whether to run it now on
      the 20 W adapter or wait for a 35 W+ charger (the user postponed RTBench's run on this adapter for
      comparability). #35 is the protocol reading either way.
    - **Update 10-17 07:05:** on the 20 W adapter the idle round trips came out ≈ +200 µs slower than on 10-13, with the
      code ruled out (the 10-13 binary gives the same numbers). So "AC Power" is not enough: the adapter can change the
      measurement. **Revised rule:** a Mac run first passes a 2-min reference check against the 10-13 levels, and the adapter
      wattage is recorded. The ext run waits for a 35 W+ charger.

## 2026-10-22 — M1 bench-thread scheduling flag (diagnostic for the M5 spin-wait shift)

36. **`--micro-qos=default|interactive|high` asks for a scheduling class for the microbench thread; the default
    changes nothing.** Since 10-17 the Mac's spin-wait cells run +100 to +280 µs slower than on 09-24, with the adapter,
    the displays, dark wake, the launch context and nice ruled out (STATUS; paper §12.2 item 5), and the 10-21 retest
    left P/E-core placement as the lead (68–75 % of the bench's CPU time on P-cores; the run with 90 % had the fastest
    empty spin round trip). macOS places threads by QoS class and priority, so the test is: does a higher class or
    priority restore the 09-24 levels? The cells run inline on the calling thread, which asks before the first
    measurement (the bandwidth rows included).
    - **What the OS allows (probe, 10-22, this session's process tree, nice 5):** `pthread_set_qos_class_self_np` succeeds
      but **a command-line process is capped at priority 31** for USER_INTERACTIVE (and reads that class by default on its
      main thread); the cap holds on the main thread, on a `pthread_attr` QoS thread and on a `std::thread`. Lowering works
      (BACKGROUND → 4). `thread_policy_set(THREAD_PRECEDENCE_POLICY)` works without root: +16 → 42 (task priority 26 =
      31 − nice 5), up to the 63 cap. So `interactive` = the QoS request as asked (recorded even when capped), and
      `high` = that plus **base priority 47 by precedence** (what a user-interactive thread has in an app; the request
      targets the absolute value, so nice does not change it; once the precedence policy is set the thread's requested
      class reads `unspecified`, the two being exclusive, and the priority is what counts). The retest compares
      `default` and `high`. Smoke-checked 10-22 10:3x: `high` → `thread_priority: cur=47 base=47` in the header.
    - **Not a protocol condition.** Off by default; `describeOptions` and the CSV header show it only when set
      (`# qos:`, `# qos.note:`), so such a run can never be pooled with the protocol's runs by mistake. R1 holds: the
      measured algorithms and buffers are unchanged, and Windows/Linux report the request as a no-op. If `high` restores
      the 09-24 levels, the human decides whether the protocol's Mac runs adopt it (then docs/06 §2 changes and both
      platforms re-measure) or whether the paper reports the shift instead.
    - **Every header now records what the thread ran as**, on both platforms: `sysinfo.thread_qos` (requested class),
      `sysinfo.thread_priority` (macOS `proc_pidinfo` cur/base/max: 47 = user-interactive in an app, 31 = default,
      26 = default at nice 5) and `sysinfo.process_nice`. CSVs before this entry lack the keys; `tools/rusage_wrap.py`'s
      P-core share stays the record for them.
    - **Retest driver:** `results/m1_mac_40w_check/launch_0558/lc4_qos.sh` (default/high interleaved, 8 runs ≈ 8 min,
      all launched from the session since the launch context is ruled out, `build/lightbound` because the frozen ext
      binary predates the flag). Waits for the user's go and a Main slot; gates on full wake and AC ≥ 35 W.
    - **Outcome (retest 2026-10-22 19:48 WIB, `results/m1_mac_40w_check/launch_1948/`): H = D.** Four `high` runs (header
      `cur=47 base=47`) interleaved with four `default` runs differ by ≤ 12 µs on 16 of 20 cells, and the other 4 follow the run
      order, not the flag; the P-core share did not move. **Scheduling class and priority are ruled out; `--micro-qos` stays a
      diagnostic, off, and is not adopted by the protocol.** The same series showed the spin cells tracking the process's
      effective clock (2.2 GHz in runs 1–3 → 2.8–3.2 GHz from run 4, when macOS's post-reboot iCloud/Spotlight housekeeping
      loaded the Mac; r(rt, GHz) −0.81…−0.95 on the empty/4K spin cells), so the lead moves to the SoC's DVFS state on an idle
      Mac (README of `m1_mac_40w_check`). What the protocol does about it is the human's call (STATUS to-do 2).
