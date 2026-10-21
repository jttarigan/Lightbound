# Paper outline (working draft, started 2026-10-16)

_Not in the original plan: `docs/07_MILESTONES.md` M9 asks for `docs/RESULTS.md` with numbers only. The user asked
(via Main, 2026-10-16) to start the paper where the data already allows it. Nothing here pre-empts results:
sections that need unmeasured data stay as headings with the data they wait for._

**Venue: The Journal of Supercomputing (Springer), chosen by the user 2026-10-17.** Framing: HPC / heterogeneous-computing readers; the game is presented as an agent-based simulation with a real-time deadline (§1 draft 2). Earlier reasoning: No deadline, so the paper goes out once, complete
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

**Submission requirements** (journal's submission-guidelines page, link.springer.com/journal/11227/submission-guidelines,
checked 2026-10-18; Springer Nature policy pages for AI and preprints, same day):
- **Abstract 100–150 words**, no undefined abbreviations or unspecified references; **4–6 keywords**. Draft 1:
  `abstract.md` (143 words with placeholders).
- **No length limit stated.** Headings: decimal numbering, at most 3 levels. Footnotes, not endnotes; a footnote may
  not be only a citation.
- **Format:** LaTeX preferred (Word accepted), **Springer Nature LaTeX template** (`sn-jnl` class; the page links
  the `sn-basic.bst` reference style) from springernature.com/gp/authors/campaigns/latex-author-support. The full
  editable source is required at every submission and revision, **flat: no subfolders** (figures and .bib included).
  **Lab rule (user, 2026-10-21, LAB.md "References: Zotero + Word"): the manuscript is Word with live Zotero
  fields** (Word is accepted here), so the sn-jnl LaTeX plan of 10-18 is replaced. References live in the Zotero
  collection `Lightbound` (GMGWF6JT; added by `tools/zotero_add.py`, keys in `zotero_map.json`); the Word build
  is `docs/paper/build_docx.py` → `lightbound_jsc.docx` (plain Word document, Springer basic numeric style; 10-21),
  counted as done only once the user's Zotero Refresh works on it.
- **References:** numbered in square brackets in citation order, cited and published/accepted works only, full DOI
  links, LTWA journal abbreviations. arXiv preprints may be cited (Springer Nature preprint policy).
- **Title page:** title, author names, affiliations and addresses, corresponding author's e-mail, telephone (fax).
- **Declarations before the references:** funding; competing interests; **data availability (required for original
  research)**; code availability; author contributions (entered in the submission system). Ethics/consent: not
  applicable (no human or animal participants).
- **Figures:** Helvetica or Arial lettering at 8–12 pt print size, lines ≥ 0.3 pt, vector (EPS/PDF) for line art,
  RGB, legible in black and white, captions "**Fig. 1** …" in the text, not in the file. Width 84 mm (one column) or
  174 mm (full), max height 234 mm (the page also lists 119 mm for small-format journals; which one applies is to be
  checked against a recent issue). **Fig. 1 fixed 10-18:** 174 mm wide, Arial embedded (TrueType), all text ≥ 8 pt,
  legend "shared memory" instead of "unified", caption "**Fig. 1**"; markers and direct labels keep it legible in
  black and white. Still open: EPS (if the PDF isn't accepted) and the flat-folder layout.
- **Preprints:** allowed on any server (arXiv included) and not prior publication; disclose the preprint's DOI and
  licence at submission.
- **AI use (Springer Nature policy):** an LLM cannot be an author. Use beyond copy-editing (readability, style,
  grammar) must be **declared in the Methods section** (or a suitable section). This draft is written with an AI
  assistant, so a declaration is needed. Its wording is the user's decision (see STATUS, "needs the user").
- **Peer-review model** (single or double anonymous): not stated on the page; check in the submission system.

**Working title:** _When does shared CPU–GPU memory pay off? Fine-grained round trips on an Apple M5 and PCIe GPUs,
measured in a real-time agent simulation_ (was "…unified memory… in a game workload": "unified memory" collides with
CUDA UM for this journal's readers)

**Contribution (as re-scoped, DECISIONS #33):** (1) a cross-backend round-trip microbenchmark with one shader
source and identical buffer layouts; (2) the measured payload crossover between unified memory and PCIe, and its
dependence on the GPU's power state (H1′); (3) a cost model that predicts a game's frame time from the
microbenchmark (§7), validated on a real game workload with K round trips per frame (H2, H3, H5); (4) decision
staleness of frame-delayed readback (H4).

| § | Section | State | Waits on |
|---|---|---|---|
| — | Abstract & keywords | **draft 1:** `abstract.md` (143 / 150 words, 6 keywords, 10-18) | §6 crossover and §9 model numbers |
| 1 | Introduction | **draft 2:** `introduction.md` (J. Supercomputing framing, 10-17; provisional contributions, placeholders) | contribution list is final after the crossover runs |
| 2 | Background & related work | **draft:** `related_work.md` (18 sources incl. J. Supercomputing CUDA-UM papers and Grace Hopper; search status per topic, 10-17) | full-text checks before submission; BibTeX for all 18 in `refs.bib` (10-18); all 18 in Zotero collection `Lightbound` (10-21) |
| 3 | Platforms | **draft:** `platforms_and_m1_results.md` | P-4060 (November) |
| 4 | Microbenchmark method | **draft:** `methods_microbench.md` | — |
| 5 | M1 results, idle GPU: the refuted H1 | **draft:** `platforms_and_m1_results.md` (data final, figure 1 embedded) | — |
| 6 | Payload crossover and GPU state (H1′) | **draft 6.1–6.2:** `crossover.md` (pre-registered H1′ + predictions, measurement; 10-21); 6.3 = placeholder | `m1_mac_ext` (waits until a reference check reproduces the 09-24 levels, §12.2 item 5, #35), `m1_3060_ext` (PC; whether the 10-19 run happened is unconfirmed) |
| 7 | Game workload & pipeline (S0–S3, K) | **draft (design):** `game_pipeline.md` | re-check against the code at M8 |
| 8 | Frame-time results (H2, H3, H5) | — | M4, M8 matrices |
| 9 | Cost model (§7) | **formulation drafted:** `cost_model.md` (payload per agent, estimation, falsifiers) | microbench ext + K = 1 game runs |
| 10 | Decision staleness (H4) | — | M3 oracle, M8 audit runs |
| 11 | Energy (H6, secondary) | — | M8 |
| 12 | Threats to validity | **draft:** `threats_to_validity.md` (platform/API, measurement hygiene with the 10-17/10-20 evidence, run-to-run, interpretation; §12.2 revised 10-21: adapter ruled out, spin-wait shift open) | launch-context retest; game-side threats with M3–M8 |
| 13 | Conclusion | — | everything |

**Draft order:** §4 (method), then §3 and §5 (facts already final), then §2 after a literature search, then §6
once both extension series are in.

**Rules for the prose.** Numbers come only from committed reports (`results/m1_report*`, `docs/PC_REPORT.md`,
later `docs/RESULTS.md`), with the report path in a comment next to each number. H1 is reported as refuted, and
H1′ is marked as written after M1 and before the extension. No backend-specific claims beyond what R1 permits.
