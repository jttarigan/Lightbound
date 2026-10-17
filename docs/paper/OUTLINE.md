# Paper outline (working draft, started 2026-10-16)

_Not in the original plan: `docs/07_MILESTONES.md` M9 asks for `docs/RESULTS.md` with numbers only. The user asked
(via Main, 2026-10-16) to start the paper where the data already allows it. Nothing here pre-empts results:
sections that need unmeasured data stay as headings with the data they wait for._

**Venue: a journal (the user, 2026-10-17: "not too ambitious").** No deadline, so the paper goes out once, complete
(through M8), with an arXiv preprint allowed earlier. Shortlist (checked 10-17; metrics are from aggregators unless marked,
so re-check on the journal's page before submitting). Hybrid journals are free on the subscription route.

| rank | journal | why it fits | level | cost / speed |
|---|---|---|---|---|
| **1** | **The Journal of Supercomputing** (Springer) | scope names "performance measures and methods"; GPU measurement papers are common there | JCR Q2, IF ≈ 2.5–3.5 | free (hybrid); first decision median 18 days (Springer page) |
| 2 | Concurrency and Computation: Practice and Experience (Wiley) | broad parallel/HPC "practice"; GPU benchmark papers in 2026 issues | JCR Q3, IF ≈ 1.5–2.2 (sources disagree) | free (hybrid; OA ≈ $4.7k) |
| 3 | Entertainment Computing (Elsevier) | games and entertainment-technology architectures; leads with the game | JCR Q2, IF ≈ 2.4 | free (hybrid) |
| 4 | Journal of Computer Graphics Techniques (JCGT) | graphics practice incl. games; values reproducibility over novelty; publishes code | indexing not confirmed | diamond OA: free for authors and readers |
| stretch | Simulation Modelling Practice and Theory (Elsevier) | Skrzypczak23 (GPU sync for simulations) appeared there | JCR Q1, IF ≈ 4.6 | free (hybrid); ≈ 4–6 months to publication |

Precedents in The Journal of Supercomputing (checked 10-17; DOIs from Crossref unless marked):
- Knap, Czarnul, *Performance evaluation of Unified Memory with prefetching and oversubscription … Pascal and Volta*,
  75 (2019) 7625–7645, doi 10.1007/s11227-019-02966-8.
- Jarząbek, Czarnul, *Performance evaluation of unified memory and dynamic parallelism … CUDA applications*,
  73 (2017) 5378–5401, doi 10.1007/s11227-017-2091-x.
- Yu, Childers, Huang, Qian, Wang, *A quantitative evaluation of unified memory in GPUs*, 76 (2020) 2958–2985,
  doi 10.1007/s11227-019-03079-y.
- Moreno et al., *Designing a benchmark for the performance evaluation of agent-based simulation applications on
  HPC*, 75 (2019) 1524–1550, doi 10.1007/s11227-018-2688-8.
- *Massively parallel modelling & simulation of large crowd with GPGPU*, 63 (2013) 675–690, doi 10.1007/s11227-011-0675-4
  (Springer URL, not checked on Crossref).
- TOUBKAL system characterization (2025), kernel-launch latency microbenchmarks, doi 10.1007/s11227-025-07858-8 (URL only).
- Not found there: Apple silicon, Metal/Vulkan, or game workloads. Czarnul's group published both the CUDA-UM evaluations
  (above) and Skrzypczak23 (hybrid CPU+GPU crowd simulation, SIMPAT), the two halves of our study.
- Note for §2: "unified memory" in these papers is CUDA's software-managed page migration on a discrete GPU, not
  physically shared memory. The paper must make the distinction explicit, and these papers are related work.

Not recommended: Future Generation Computer Systems and J. Systems Architecture (Q1, more ambitious than asked); IEEE
Access ($2,160 APC, official 2026 list) and PeerJ CS (≈ $1,395) only if open access is required. Conference dates
checked the same day (ICPE 2027 paper Nov 16 2026; ISPASS ≈ Dec; HPG ≈ Apr; IISWC ≈ May–Jun) are kept for reference
only.

**Working title:** _When does unified memory pay off? Fine-grained CPU–GPU round trips on Apple M5 vs discrete
GPUs over PCIe, measured in a game workload_

**Contribution (as re-scoped, DECISIONS #33):** (1) a cross-backend round-trip microbenchmark with one shader
source and identical buffer layouts; (2) the measured payload crossover between unified memory and PCIe, and its
dependence on the GPU's power state (H1′); (3) a cost model that predicts a game's frame time from the
microbenchmark (§7), validated on a real game workload with K round trips per frame (H2, H3, H5); (4) decision
staleness of frame-delayed readback (H4).

| § | Section | State | Waits on |
|---|---|---|---|
| 1 | Introduction | **draft:** `introduction.md` (provisional contributions, placeholders) | contribution list is final after the crossover runs |
| 2 | Background & related work | **draft:** `related_work.md` (14 sources; search status per topic, 10-17) | full-text checks and BibTeX before submission (DOIs of Velicka25/26 verified 10-17) |
| 3 | Platforms | **draft:** `platforms_and_m1_results.md` | P-4060 (November) |
| 4 | Microbenchmark method | **draft:** `methods_microbench.md` | — |
| 5 | M1 results, idle GPU: the refuted H1 | **draft:** `platforms_and_m1_results.md` (data final, figure 1 embedded) | — |
| 6 | Payload crossover and GPU state (H1′) | predictions only (written 2026-10-13) | `m1_mac_ext` (needs a 35 W+ adapter, #35), `m1_3060_ext` (PC, needs the bundle) |
| 7 | Game workload & pipeline (S0–S3, K) | **draft (design):** `game_pipeline.md` | re-check against the code at M8 |
| 8 | Frame-time results (H2, H3, H5) | — | M4, M8 matrices |
| 9 | Cost model (§7) | **formulation drafted:** `cost_model.md` (payload per agent, estimation, falsifiers) | microbench ext + K = 1 game runs |
| 10 | Decision staleness (H4) | — | M3 oracle, M8 audit runs |
| 11 | Energy (H6, secondary) | — | M8 |
| 12 | Threats to validity | **draft:** `threats_to_validity.md` (platform/API, measurement hygiene with the 10-17 evidence, run-to-run, interpretation) | 35 W+ adapter check; game-side threats with M3–M8 |
| 13 | Conclusion | — | everything |

**Draft order:** §4 (method), then §3 and §5 (facts already final), then §2 after a literature search, then §6
once both extension series are in.

**Rules for the prose.** Numbers come only from committed reports (`results/m1_report*`, `docs/PC_REPORT.md`,
later `docs/RESULTS.md`), with the report path in a comment next to each number. H1 is reported as refuted, and
H1′ is marked as written after M1 and before the extension. No backend-specific claims beyond what R1 permits.
