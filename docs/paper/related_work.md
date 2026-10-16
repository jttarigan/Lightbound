# §2 Background & related work (draft, 2026-10-16)

<!-- Search 2026-10-16 (web search, standard mode, 8 queries). Every entry below was opened, and its title, authors and
     venue were read from the publisher, arXiv, hgpu.org or the PDF itself. "Checked" says what was read; the full
     text was read only where it says so. Nothing is cited from memory. Before submission, pull BibTeX from DBLP or the
     publisher and re-check each claim against the full text. -->

## Sources found

| key | reference | checked | relevance |
|---|---|---|---|
| Beischl26 | A. Beischl, M. Morozov, M. Jungmair, L. Haussmann, T. Neumann. *Evaluating Apple Silicon for Data Processing.* DaMoN '26 (22nd Int. Workshop on Data Management on New Hardware), ACM. doi:10.1145/3789237.3809127 | full PDF text (authors' copy, db.in.tum.de) | **closest**: Apple Silicon unified memory, Metal synchronization costs |
| Feng25 | D. Feng, Z. Xu, R. Wang, F. X. Lin. *Profiling Apple Silicon Performance for ML Training.* arXiv:2501.14925, 2025 | arXiv abstract | Apple Silicon vs NVIDIA; names kernel launch time as one cause of the gap |
| Huebner25 | P. Hübner, A. Hu, I. Peng, S. Markidis. *Apple vs. Oranges: Evaluating the Apple Silicon M-Series SoCs for HPC Performance and Efficiency.* arXiv:2502.05317, 2025 | arXiv abstract | M1–M4 bandwidth and FP32 throughput with Metal; energy |
| Wahlgren25 | J. Wahlgren, G. Schieffer, R. Shi, E. A. León, R. Pearce, M. Gokhale, I. Peng. *Dissecting CPU-GPU Unified Physical Memory on AMD MI300A APUs.* IISWC 2025 (arXiv:2508.12743) | arXiv abstract | unified physical memory characterization (latency, bandwidth, coherence), HPC side |
| Spafford12 | K. Spafford, J. S. Meredith, S. Lee, D. Li, P. C. Roth, J. S. Vetter. *The Tradeoffs of Fused Memory Hierarchies in Heterogeneous Computing Architectures.* ACM Computing Frontiers (CF '12), pp. 103–112 | search snippets + hgpu/OSTI/DBLP listings (OSTI page refused the connection) | the classic fused (APU) vs discrete comparison |
| Gera18 | P. Gera, H. Kim, H. Kim, S. Hong, V. George, C.-K. Luk. *Performance Characterisation and Simulation of Intel's Integrated GPU Architecture.* ISPASS 2018 | search snippets, IEEE Xplore record 8366948 | iGPU microbenchmarks incl. CPU–GPU cache sharing |
| GomezLuna17 | J. Gómez-Luna, I. El Hajj, L.-W. Chang, V. García-Flores, S. Garcia de Gonzalo, T. B. Jablin, A. J. Peña, W.-m. Hwu. *Chai: Collaborative Heterogeneous Applications for Integrated-architectures.* ISPASS 2017 | hgpu.org abstract | fine-grained CPU–GPU collaboration patterns on integrated architectures |
| Sun16 | Y. Sun, X. Gong, A. K. Ziabari, L. Yu, X. Li, S. Mukherjee, C. McCardwell, A. Villegas, D. Kaeli. *Hetero-Mark, a Benchmark Suite for CPU-GPU Collaborative Computing.* IISWC 2016 | hgpu.org abstract | collaboration patterns; reports communication and transfer latency |
| Lumpp21 | F. Lumpp, N. Bombieri. *A Framework for Optimizing CPU-iGPU Communication on Embedded Platforms.* 2021, IEEE Xplore 9586304 (conference 5–9 Dec 2021, San Francisco: **venue name to confirm**, likely DAC) | search snippets (repository page returned 403) | copy vs unified memory vs zero-copy on embedded iGPUs, with a performance model |

## What they establish, and the gap we fill

- **Unified memory doesn't remove synchronization cost; it moves it.** Beischl26 measures this on Apple Silicon:
  - shared buffers get a coherence pass at kernel launch (once per newly allocated buffer, 70–120 GB/s on M4 Pro);
  - Metal kernel start-up costs 28–79 µs to encode plus 14–17 µs (M4 Pro) or 65–75 µs (M3 Ultra) to dispatch;
  - managing memory explicitly cuts per-run coherence overhead below 0.06 ms.

  Wahlgren25 characterizes coherence overhead on MI300A's unified physical memory, and Spafford12 makes the same
  trade-off argument for early AMD APUs. Our M5 data agree: the cost sits in the hand-off, not in the bytes (§5).
- **Copy vs zero-copy is workload-dependent** (Lumpp21 on embedded iGPUs; Spafford12). Our §6 crossover measures
  that dependence directly in payload size and GPU state, on one benchmark that runs unchanged on both platforms.
- **Collaboration benchmarks** (GomezLuna17, Sun16) define the fine-grained CPU–GPU patterns, but they target
  integrated architectures through OpenCL, CUDA or C++ AMP. Neither compares a unified-memory SoC with a discrete GPU
  on one shader source with identical buffer layouts.
- **Apple Silicon studies** (Feng25, Huebner25, Beischl26) look at ML training, HPC kernels and databases.
  - Feng25 already points to kernel launch time as a factor in the gap with NVIDIA.
  - None measures repeated round trips within a real-time frame budget, or the GPU power-state effect we see in
    the CPU→GPU hand-off (§5, §6).

**Gap (to state carefully, "we are not aware of"):** this search found no peer-reviewed measurement of
fine-grained, multiple-per-frame CPU–GPU round trips that compares a unified-memory SoC with PCIe GPUs on
identical algorithms, ties the result to a game workload, or studies ReBAR's effect on the CPU→GPU write path.
ReBAR coverage found so far is consumer game benchmarks only (TechPowerUp, Guru3D, PCGH), which is not citable as
evidence.

## Still to search (before writing the final §2)

- GPU idle-state / power-state exit latency (Apple and NVIDIA). This is needed for the §5/§6 explanation; until
  then the text says only "consistent with".
- Timeline-semaphore / `MTLSharedEvent` latency, and kernel-launch overhead studies (beyond forum numbers).
- Frame-delayed readback and staleness in games (H4). So far only engine docs (Unity AsyncGPUReadback, Stride
  ImageReadback); look for GPU-driven rendering / async readback papers.
- Crowd and swarm simulation on GPUs with CPU decision logic (the workload side).
- PCIe write-combining / BAR-mapped CPU writes (academic, for decision c).
- MoltenVK or Metal-vs-Vulkan API-overhead comparisons (threat: different APIs).
