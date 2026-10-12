# Disturbed run 1 of the 3060 Ti ReBAR-on series — not part of `results\m1_3060`

Run 1 of the RTX 3060 Ti / ReBAR-on series (2026-10-05 14:34 → 10-06 14:18, binary `6962eff`,
exit 0, 0 failures) was moved here and replaced by a new run 1 in `results\m1_3060`, because two
of its cells were disturbed by activity on the PC during the run's first minutes: a git commit
and push ran at ≈ 14:37:40, while `s2_hostcached` perpass/spin 1M and 4M were being measured
(179–183 s into the run). Those two cells came out 17.6 % and 11.8 % above runs 2 and 3, which
made the run-to-run check of the pooled report fail; runs 2 and 3 agree on them within 0.1 %.

Everything else in this run is consistent with runs 2 and 3 (slow paths within 0.3 %, fast paths
within 1–4 %), so the file is kept for reference. Do not pool it with `results\m1_3060`
(`micro_report.py` pools every `micro_*.csv` in a directory). See `docs/PC_PROGRESS.md`,
"Step C results".
