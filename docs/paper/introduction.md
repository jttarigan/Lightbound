# §1 Introduction (draft, 2026-10-17)

<!-- Sources: docs/06 §1 (RQ, H1–H6, H1′), DECISIONS #33, platforms_and_m1_results.md (§5 numbers, from
     results/m1_report*/report.md), related_work.md. The contribution list is provisional: items 2–4 depend on
     runs not yet made (OUTLINE.md). Numbers in brackets [..] are placeholders until those runs exist. -->

Games and interactive simulations increasingly split one frame's work between the CPU and the GPU at a fine
grain. A swarm's positions and perception are computed on the GPU, decisions are taken on the CPU, and their
results go back to the GPU before the frame is drawn. Every such hand-off is a round trip:
- the GPU signals,
- the CPU wakes up, reads and writes the shared data, and signals back,
- the GPU resumes.

On a discrete GPU the data cross PCIe in both directions. On a unified-memory SoC such as Apple's M-series, CPU
and GPU share one physical memory, and no copy is needed. That suggests an architectural advantage for
cooperation patterns with several round trips per frame, which current engines avoid by reading GPU results one
or more frames late.

**We set out to test that advantage directly and found it does not hold as stated.** We built a game, Lightbound
(a top-down stealth roguelite with thousands of light-sensitive creatures), designed from the start as a
measurement instrument. Its round trips run on Metal and Vulkan from one shader source, with identical buffer
layouts, and the transfer strategy and the number of round trips per frame are runtime switches. Our first
hypothesis, stated in advance, was that the M5's round trip is cheaper than a PCIe GPU's at every payload size,
with a go/no-go criterion fixed before measuring. It was refuted. With the GPU otherwise idle, the M5's empty
round trip costs 214 µs against 60 µs on an RTX 3060 Ti, and at 1 MiB its best path costs 305 µs against
142–160 µs. The cost is not in the bytes. It lies in the CPU→GPU hand-off, which on the M5 is multimodal and
consistent with the GPU leaving an idle power state.

The same measurements show where unified memory does pay: the M5's round trip is nearly flat in payload size,
while the PCIe GPU's grows linearly with it. At 16 MiB the M5 is 1.7–2.8× faster on the direct path. The useful
question is therefore not *whether* unified memory wins, but **where the curves cross, and how that crossover
moves when the GPU is busy, as it is in a real frame**. We answer it with:

- an extended payload grid of 4 KiB–64 MiB, measured with the GPU idle and hot;
- a cost model that turns the microbenchmark into frame-time predictions;
- the game itself, run at increasing agent counts and round trips per frame.

**Contributions.**
1. A cross-backend round-trip microbenchmark (Metal and Vulkan, one Slang source, verified payloads and a
   copy-count invariant) that isolates the GPU→CPU and CPU→GPU hand-offs. *(done: §4)*
2. Its measured results on Apple M5 vs RTX 3060 Ti over PCIe gen3, with ReBAR on and off: the refuted
   "always cheaper" hypothesis (§5), and the payload crossover by GPU state (§6). The idle-GPU crossover is
   predicted at 2.0–2.6 MiB per direction, and [measured: ..]. *(§5 done; §6 waits for the extension series)*
3. A cost model that predicts a game's frame time from the microbenchmark, validated on the game with K round
   trips per frame (H2, H3, H5). *(waits for M4–M8)*
4. A measurement of decision staleness when GPU results are read back a frame late, against a same-frame oracle
   (H4). *(waits for M3, M8)*
5. A practical account of measurement hygiene for latency-bound GPU studies on a laptop. Two effects cost us
   runs: an under-powered adapter that still reports "AC Power", and background GPU clients. Both shift idle round
   trips by hundreds of microseconds (§12). *(evidence in hand; one claim waits for a run on a 35 W+ adapter)*

**What we do not claim.** The comparison is between platforms (hardware, OS, driver and API together), not
between memory architectures in isolation (§12). Each class is one machine. H1′ was formulated after M1's idle
results and before the extension that tests it, and we say so where it is used.

<!-- Keep for the final pass: one sentence on why games matter here (frame budget 16.7 / 8.3 ms makes µs-scale
     hand-offs visible); a figure 1 teaser (round trip vs payload, M5 vs 3060 Ti, idle) once fig1 is final. -->
