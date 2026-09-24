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
