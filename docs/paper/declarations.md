# Declarations (draft 1, 2026-10-23)

<!-- Springer Nature / J. Supercomputing: the declarations stand before the references: funding; competing interests;
     ethics and consent (not applicable here); data availability (required for original research); code availability;
     author contributions (also entered in the submission system). AI use beyond copy-editing is declared in the Methods
     section (OUTLINE.md "Submission requirements"). Items in [brackets] are the user's decisions (STATUS to-do 7).
     Each H2 block below becomes one bold-headed paragraph in the .docx (build_docx.py reads this file; its DECLARATIONS
     list is the fallback). Checked 2026-10-23: every results/ folder cited in the paper is tracked in the repository
     (git ls-files results); the repository has NO LICENSE file yet (user's choice; code availability says so). -->

## Funding
[No funding was received for this work. / to state]

## Competing interests
[The author declares no competing interests. / to state]

## Ethics approval and consent to participate
Not applicable: the study involves no human or animal participants.

## Data availability
The measured series behind every number in this paper are in the public repository
https://github.com/jttarigan/Lightbound under `results/`: the per-iteration CSV files of the microbenchmark with their
environment headers, the pooled summaries and reports (`results/m1_report`, `results/m1_report_rebar-off`), the
crossover tables (`results/m1_crossover`) and the diagnostic checks of §12.2 (`results/m1_mac_20w_check`,
`results/m1_mac_40w_check`). [An archived snapshot with a DOI (Zenodo) at submission: to decide.]

## Code availability
The benchmark, the simulation, the shader sources, the analysis scripts (`tools/analysis/`) and the build files are in
the same repository; the commit measured for each series is recorded in that series' report, and the macOS and Windows
binaries are built from one source tree with CMake (§4). [Licence: the repository has no licence file yet; to choose
before submission.]

## Author contributions
Single author. [Entered in the submission system.]

## Use of AI tools
[Wording = the user's decision; Springer Nature asks for use beyond copy-editing to be declared in the Methods section.
Proposed text, to move into §4 if adopted: "The author used an AI assistant (Claude, Anthropic) to draft code, analysis
scripts and manuscript text under the author's direction. The author designed the study, fixed the hypotheses in
advance, ran every measurement, checked every number against the recorded data and takes responsibility for the
content."]
