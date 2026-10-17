# Paper outline (working draft, started 2026-10-16)

_Not in the original plan: `docs/07_MILESTONES.md` M9 asks for `docs/RESULTS.md` with numbers only. The user asked
(via Main, 2026-10-16) to start the paper where the data already allows it. Nothing here pre-empts results:
sections that need unmeasured data stay as headings with the data they wait for._

**Venue: not chosen (the user decides).** Candidates by fit: a workload-characterization / systems venue
(ISPASS- or IISWC-style), or High-Performance Graphics; an arXiv preprint first in any case.

Deadlines (checked 2026-10-17; only ICPE 2027 is announced, the others are the last edition's dates, so the 2027
dates are a guess from the pattern):

| venue | fit | deadline | source |
|---|---|---|---|
| ICPE 2027 (research track; Gothenburg, May 24–28 2027) | measurement + cost model | abstract **Nov 9 2026**, paper **Nov 16 2026** (AoE) | icpe2027.spec.org/important-dates (official) |
| ISPASS 2027 | performance analysis, microbenchmarks | likely early–mid Dec 2026 (2026 edition: Dec 5/12 2025) | aggregators; ispass.org not yet posted |
| HPG 2027 | GPU / real-time graphics | likely early Apr 2027 (2026: abstract Apr 1, paper Apr 10) | highperformancegraphics.org/2026 |
| IISWC 2027 | workload characterization (best fit for the game half) | likely late May–Jun 2027 (2026: May 21; 2025: Jun 21) | sigarch.org CFPs |
| I3D 2027 | interactive graphics and games | 2026 dates conflict between listings (Jan 9 vs May) | easychair; i3dsymposium.org to check |

The game half (§7–§11) needs M3–M8, so only the spring venues can carry the full paper. A November/December
deadline would mean a microbenchmark-only paper (§3–§6 + §12), which needs both extension series within ≈ 3–7
weeks and would take the crossover result out of the game paper. Recommendation (for the user to decide): one full
paper aimed at IISWC 2027, with HPG 2027 as the earlier option if M8 is done by March; an arXiv preprint when §6
is written.

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
