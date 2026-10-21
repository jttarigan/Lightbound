#!/usr/bin/env python3
"""Launch-context retest (10-21): rt p50 per idle cell, S (session) vs T (Terminal.app) vs ref m1_mac_ac (09-24),
plus each run's exit rusage (P-core share, GHz). Usage: lc_compare.py <lc dir>"""
import glob, json, re, sys
import pandas as pd

LC = sys.argv[1]
REF = sorted(glob.glob("/Users/jostarigan/Project/Lightbound/results/m1_mac_ac/micro_P-M5_run*.csv.gz"))
PATHS, PAY = ["empty", "s1_copy", "s2_direct"], [0, 4096, 1048576]
KEY = ["path", "submit", "cpuwait", "payload_bytes"]


def p50(f):
    d = pd.read_csv(f, comment="#", low_memory=False)
    d = d[d.path.isin(PATHS) & d.payload_bytes.isin(PAY)]
    return d.groupby(KEY).rt_us.median()


runs = {}
for f in sorted(glob.glob(f"{LC}/lc_*_[ST].csv")):
    m = re.search(r"lc_(\d+)_([ST])\.csv$", f)
    runs[(int(m.group(1)), m.group(2))] = p50(f)
ref = pd.DataFrame({f"ref{i}": p50(f) for i, f in enumerate(REF, 1)}).median(axis=1)
t = pd.DataFrame({"ref_0924": ref})
for c in "ST":
    cols = [v for (i, cc), v in sorted(runs.items()) if cc == c]
    if cols:
        t[c] = pd.concat(cols, axis=1).median(axis=1)
        t[f"{c}_n"] = len(cols)
for c in "ST":
    if c in t:
        t[f"d{c}"] = t[c] - t.ref_0924
if "S" in t and "T" in t:
    t["T-S"] = t["T"] - t["S"]
pd.set_option("display.width", 250)
print("rt_us p50 (median over runs of per-run p50); ref = median of the 3 m1_mac_ac runs")
print(t.round(0).to_string())
print("\nper run: exit rusage")
for (i, c) in sorted(runs):
    try:
        d = json.load(open(f"{LC}/ru_{i}_{c}.json"))
        print(f"  run {i} {c}: pcore {d.get('pcore_share')} GHz all {d.get('ghz_all')} P {d.get('ghz_pcore')} "
              f"E {d.get('ghz_ecore')} cpu {d.get('cpu_s')} s wall {d.get('wall_s')} s qos {d.get('qos_share')}")
    except OSError as e:
        print(f"  run {i} {c}: no rusage ({e})")
spin = t.xs("spin", level="cpuwait")
for c in "ST":
    if f"d{c}" in spin:
        print(f"spin cells d{c} vs 09-24: {spin[f'd{c}'].min():.0f} to {spin[f'd{c}'].max():.0f} µs")
