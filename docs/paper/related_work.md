# §2 Background & related work (draft, 2026-10-16)

<!-- Search 2026-10-16 (web search, standard mode, 8 queries) and 2026-10-17 (12 queries, 8 pages opened). Every entry below was opened, and its title, authors and
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
| Lumpp21 | F. Lumpp, N. Bombieri. *A Framework for Optimizing CPU-iGPU Communication on Embedded Platforms.* DAC 2021 (San Francisco, 5–9 Dec 2021), IEEE Xplore 9586304 | IEEE Xplore record + Univ. Verona repository abstract (venue confirmed 10-17) | copy vs unified memory vs zero-copy on embedded iGPUs, with a performance model and micro-benchmarks |
| Maczan26 | J. Maczan. *Measuring and Reducing WebGPU Dispatch Overhead for LLM Inference.* arXiv:2608.08730 (v2, Aug 2026); accepted at the 4th UK AI Conference 2026 (PMLR) | arXiv abstract + HTML full text §1, §4.1, §5.1 | per-dispatch cost 24–36 µs on Vulkan and 32–71 µs on Metal (sequential method, sync excluded); a single timed op is dominated by sync (450 of 497 µs) |
| Cassagne25 | A. Cassagne, N. Amiot, M. Bouyer. *Dalek: An Unconventional and Energy-Aware Heterogeneous Cluster.* arXiv:2508.10481, 2025 | arXiv abstract + HTML full text §V-E | OpenCL launch latency (host call → kernel start): ≈ 5 µs RTX 4090 and Radeon 890M, 35–40 µs Intel iGPUs, ≈ 90 µs Arc 770 over Oculink |
| Velicka25 | D. Velicka, O. Vysocky, L. Riha. *Methodology for GPU Frequency Switching Latency Measurement.* IPDPS Workshops 2025, pp. 830–839, doi 10.1109/IPDPSW66978.2025.00133 (Crossref, 10-17); preprint arXiv:2502.20075 | arXiv abstract | DVFS switching latency on GH200, A100, RTX Quadro 6000 (as the abstract names it); differs widely between GPUs |
| Skrzypczak23 | J. Skrzypczak, P. Czarnul. *Efficient parallel implementation of crowd simulation using a hybrid CPU+GPU high performance computing system.* Simulation Modelling Practice and Theory 123 (2023) 102691. doi:10.1016/j.simpat.2022.102691 | search snippets + university repository listing (repository host unreachable 10-17) | workload side: OpenMP+CUDA crowd simulation with CPU–GPU load balancing, 10k–1M agents |
| Velicka26 | D. Velicka, O. Vysocky, O. Yasal, L. Riha. *An in-depth study of GPU frequency-scaling latency and its optimization on modern architectures.* Future Generation Computer Systems 179 (2026), article 108331, doi 10.1016/j.future.2025.108331 (verified on Crossref 10-17; the search result's DOI was one digit short) | search result abstract (repository page rate-limited) | journal version of Velicka25: switching vs transition latency, LATEST tool, GH200 / A100 / RTX Quadro 6000; dataset on Zenodo (2025-09-29) |

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

- **Launch and dispatch overhead are known to be tens of µs, but measured one way.** Maczan26 separates the
  marginal cost of a dispatch (24–36 µs Vulkan, 32–71 µs Metal) from the synchronization that a naive single-op
  timing folds in (450 of 497 µs). Cassagne25 reports host-to-kernel-start latency from ≈ 5 µs (RTX 4090) to
  35–40 µs (Intel iGPUs). Both measure the CPU→GPU direction only. Our round trip is exactly the part they exclude:
  the full hand-off in both directions, repeated within a frame, which is what a game pays for each K.
- **GPU power-state transitions are slow and vendor-specific.** Velicka25 measures DVFS switching latency on
  data-centre NVIDIA GPUs. We found no study of idle-state exit latency, on Apple or NVIDIA. So §5/§6 can only say
  the multimodal M5 hand-off is "consistent with" a power-state effect, and the hot-GPU cells test that reading
  directly.
- **Workload side.** Hybrid CPU+GPU crowd simulation exists as an HPC problem (Skrzypczak23: load balancing over
  10k–1M agents, throughput-oriented). Real-time swarms with CPU-side decisions inside a 16.7 ms frame are not
  studied there.

**Gap (to state carefully, "we are not aware of"):** this search found no peer-reviewed measurement of
fine-grained, multiple-per-frame CPU–GPU round trips that compares a unified-memory SoC with PCIe GPUs on
identical algorithms, ties the result to a game workload, or studies ReBAR's effect on the CPU→GPU write path.
ReBAR coverage found so far is consumer game benchmarks only (TechPowerUp, Guru3D, PCGH), which is not citable as
evidence.

## Search status (2026-10-17)

| topic | result |
|---|---|
| GPU idle-state exit latency | **none found** (only DVFS switching, Velicka25, and forum reports). Keep "consistent with". |
| semaphore / `MTLSharedEvent` latency, launch overhead | Maczan26, Cassagne25 (above). No study of timeline-semaphore or `MTLSharedEvent` signal-to-wake latency found. |
| frame-delayed readback and staleness (H4) | **no academic source found**; only engine docs (Unity `AsyncGPUReadback`: "adds a few frames of latency"). §10 can state the gap. |
| crowd/swarm with CPU decisions | Skrzypczak23 (HPC). Real-time game swarms: still open. |
| ReBAR / BAR-mapped CPU writes | **no academic source found**; vendor explainers (Intel) and press only. For §5's ReBAR result, say "we are not aware of a published measurement". |
| MoltenVK vs Metal overhead | Khronos Vulkanised 2023 talk (B. Hollings, "MoltenVK: Layering Vulkan Over Metal", Feb 2023) covers status and conformance, **no overhead numbers**. No controlled benchmark found. The threat stays qualitative, and the optional MoltenVK control run (§12) is the only evidence we could add. |
