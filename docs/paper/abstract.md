# Abstract and keywords (draft 1, 2026-10-18)

<!-- Journal limits (J. Supercomputing submission guidelines, checked 2026-10-18): abstract 100–150 words, no undefined
     abbreviations or unspecified references; 4–6 keywords. Numbers from results/m1_report*/report.md (chain/spin p50),
     as in introduction.md and platforms_and_m1_results.md. [..] = waits for the §6 extension series and the §9 cost
     model. Count: 143 words with the placeholders (`wc -w`). Filling them adds ≈ 8–12 words, so the final pass must cut
     ≈ 5: first candidate is the first sentence's "where …" clause. "PCI Express" is spelled out; CPU, GPU, µs and MiB
     are left as standard units/terms (re-check against the editor's reading at the final pass). -->

Tightly coupled CPU–GPU nodes promise to remove data copies, but their value for fine-grained cooperation, where the
processors exchange data several times per simulation step, is rarely measured. We compare an Apple M5
system-on-chip with shared physical memory against an NVIDIA RTX 3060 Ti on PCI Express, using a portable Metal and
Vulkan round-trip microbenchmark and a real-time simulation of thousands of agents. The hypothesis, fixed in advance,
that the shared-memory round trip is cheaper at every payload size was refuted: with the GPU idle, an empty round
trip costs 214 µs on the M5 against 60 µs on the RTX, dominated by the CPU-to-GPU hand-off. The M5's cost barely
grows with payload, however, making it 1.7–2.8 times faster at 16 MiB. We locate the crossover with the GPU idle and
busy [..], and a microbenchmark-based cost model predicts step time within [..].

**Keywords** (6, the maximum): Shared physical memory · CPU–GPU synchronization · Round-trip latency ·
Heterogeneous computing · Apple silicon · Agent-based simulation

<!-- Keyword choice: "shared physical memory" rather than "unified memory" (collides with CUDA UM, introduction.md
     §Terminology); "agent-based simulation" links to the journal's ABS precedents (Moreno19, OUTLINE.md). Swap
     candidate if the editor's system offers a fixed list: "GPU microbenchmarking". -->
