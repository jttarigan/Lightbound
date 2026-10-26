# §1 Introduction (draft 2, 2026-10-17: framed for The Journal of Supercomputing)

<!-- Sources: docs/06 §1 (RQ, H1–H6, H1′), DECISIONS #33, platforms_and_m1_results.md (§5 numbers, from
     results/m1_report*/report.md), related_work.md (keys in [brackets]). The contribution list is provisional:
     items 2–4 depend on runs not yet made (OUTLINE.md). Numbers in brackets [..] are placeholders until those runs exist.
     Draft 1 (game-first framing) is in git history (934c91b). Audience: HPC / heterogeneous-computing readers, so the
     game is presented as an agent-based simulation with a real-time deadline, not as a games contribution. -->

Heterogeneous nodes are moving the CPU and the GPU closer together:
- NVIDIA's Grace Hopper joins them with a cache-coherent chip-to-chip link and one system page table [Schieffer24];
- AMD's MI300A puts both on one package with one physical HBM memory [Wahlgren25];
- Apple's M-series laptop and desktop SoCs have done the same with LPDDR memory since 2020 [Huebner25, Beischl26].

The promise of this tight coupling is that data no longer has to be copied between two memories. The studies so far
mostly ask what it does for **bandwidth-bound** work: large kernels, data that exceeds GPU memory, and porting effort
[Schieffer24, Wahlgren25, Huebner25]. We ask what it does for **latency-bound, fine-grained cooperation**, where the
CPU and the GPU hand data back and forth several times per time step.

That pattern arises whenever an iterative simulation splits each step between the two processors. The GPU does the
regular, data-parallel part, such as neighbour search, field evaluation and perception, for many agents. The CPU
does the irregular, branchy part, such as each agent's decision, graph propagation and event handling. The results
go back to the GPU before the step ends. Each hand-off is a round trip:
- the GPU signals,
- the CPU wakes up, reads and writes the shared data, and signals back,
- the GPU resumes.

Under a deadline, as in interactive simulation, digital twins or games at 60 Hz (a 16.7 ms step), round trips of
tens to hundreds of microseconds are a visible share of the budget. They decide whether the coupling can stay
synchronous at all. Current practice avoids the question by reading GPU results one or more steps late. That moves
the cost from time to accuracy.

**Terminology.** In much of the GPU literature, *unified memory* means CUDA's software-managed address space on a
discrete GPU: pages migrate over PCIe on demand. Its costs have been measured in this journal and elsewhere [Knap19,
Jarzabek17, Yu20]. We study something different: memory that the CPU and GPU **physically share**, with no
migration. Apple calls it unified memory too. We write *shared physical memory* where the distinction matters, and
*PCIe* for the discrete baseline.

**We set out to test the expected advantage directly, and found that it does not hold as stated.** Our instrument
is an agent-based simulation of thousands of light-sensitive creatures, built as a playable game, Lightbound, and
designed from the start for measurement:
- its round trips run on Metal and on Vulkan from one shader source, with identical buffer layouts;
- the transfer strategy and the number of round trips per step are runtime switches.

Our first hypothesis, stated in advance with a go/no-go criterion, was that the shared-memory SoC's round trip is
cheaper than a PCIe GPU's at every payload size. It was refuted. With the GPU otherwise idle, an Apple M5's empty
round trip costs 214 µs, against 60 µs on an RTX 3060 Ti over PCIe gen3. At 1 MiB per direction, the M5's best path
costs 305 µs against 142–160 µs. The cost is not in the bytes. It lies in the CPU→GPU hand-off, which on the M5 is
multimodal and consistent with the GPU leaving an idle power state.
<!-- results/m1_report/report.md, results/m1_report_rebar-off/report.md; chain/spin p50 -->

The same measurements show where shared memory does pay. The M5's round trip is nearly flat in payload size, while
the PCIe GPU's grows linearly with it, and at 16 MiB the M5 is 1.7–2.8× faster on the direct path. The useful
question is therefore not *whether* shared memory wins, but **where the curves cross, and how the crossover moves
when the GPU is busy, as it is inside a real time step**. We answer it in three ways:
- an extended payload grid from 4 KiB to 64 MiB, measured with the GPU idle and hot;
- a cost model that turns the microbenchmark into per-step time predictions;
- the simulation itself, run at increasing agent counts and round trips per step.

**Contributions.**
1. A portable round-trip microbenchmark for CPU–GPU hand-offs (Metal and Vulkan, one Slang source, verified
   payloads and a copy-count invariant). It isolates the GPU→CPU and CPU→GPU directions. *(done: §4)*
2. Its results on a shared-memory SoC (Apple M5) and a PCIe GPU (RTX 3060 Ti, with ReBAR on and off):
   - the refuted "always cheaper" hypothesis (§5);
   - the payload crossover by GPU state (§6). The idle-GPU crossover is predicted at 2.0–2.6 MiB per direction,
     [measured: ..].

   *(§5 done; §6 waits for the extension series)*
3. A cost model that predicts a coupled simulation's step time from microbenchmark terms. It is validated on the
   simulation with K round trips per step (H2, H3, H5). *(waits for M4–M8)*
4. The accuracy cost of the asynchronous alternative: how often agents decide differently when GPU results arrive
   one step late, measured against a same-step oracle (H4). *(waits for M3, M8)*
5. A practical account of measurement hygiene for microsecond-scale GPU latency on a laptop-class SoC. Background
   GPU clients, CPU contention and an undetected dark wake cost us runs, and a shift of 100–280 µs in spin-wait
   round trips survived every power, display, wake, launch and scheduling condition we tried, until the processor
   clock explained it: on an otherwise idle laptop a lone spin-waiting thread runs at 2.7 GHz, beside a two-thread
   background load at 4.3–4.5 GHz, and the load speeds spin waits but slows block waits (§12). *(evidence in hand,
   clock trace 2026-10-25; the protocol's response is open)*

**Scope and what we do not claim.**
- We compare two **platforms** (hardware, OS, driver and API together), not memory architectures in isolation (§12).
- Each class is represented by one machine: a laptop-class SoC and a desktop PCIe GPU. We do not measure Grace
  Hopper or MI300A. Our results say where a hand-off's cost comes from (fixed signalling and power-state latency versus
  bytes), and that question carries over to those nodes. The numbers do not.
- H1′ was formulated after the idle results and before the extension that tests it, and we say so where it is used.

**Organization.** §2 places the work among studies of tightly coupled CPU–GPU systems, CUDA unified memory and
collaborative CPU–GPU benchmarks. §3 describes the platforms and §4 the microbenchmark. §5 reports the idle-GPU
results and the refuted hypothesis, and §6 the crossover and its dependence on GPU state. §7 describes the
simulation and its coupling strategies, and §8 its measured step times. §9 fits and tests the cost model, §10
measures decision staleness, and §11 energy. §12 discusses threats to validity, and §13 concludes.

<!-- Journal limits and reference style confirmed 2026-10-18 (OUTLINE.md "Submission requirements"; abstract in
     abstract.md). Final pass: keep the 16.7 ms framing as the only game-specific motivation in §1. -->
