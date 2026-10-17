# 20 W adapter check (2026-10-17, Mac, not a measurement series)

Diagnostic only; never pool with `m1_mac_ac` or `m1_mac_ext` (DECISIONS #35).

- Conditions: 20 W USB-C adapter (12 V / 1.66 A, `pmset -g ac`), macOS "AC Power", battery 80 % "not charging",
  Low Power Mode off, Chrome quit, GPU Device Utilization 0 %, 1-min load < 3, thermal state nominal.
- `attempt3_0658_partial.log`: first minute of the stopped `m1_mac_ext` attempt 3 (frozen binary 2d5c1ba).
- `ab_{old,new}_{1,2}.csv.gz`, 07:01–07:03 WIB, run alternately old, new, old, new: idle cells, paths empty/s1_copy/s2_direct,
  payloads 4K and 1M.
  - old = commit 055d696 (the `m1_mac_ac` commit) rebuilt 10-17 into `build/ref_055d696/`;
  - new = the frozen ext binary `build/frozen_ext_2d5c1ba/`.
- Result: old and new agree, and both are ≈ +200 µs per round trip against `m1_mac_ac` (10-13).
  Example, s1_copy chain/spin rt p50: 4K 493/502 (old) and 497/504 (new) µs, against 289–294 µs on 10-13. So the
  environment changed, not the code. Suspect: the adapter (not proven).
