#!/usr/bin/env python3
"""QoS/priority retest (10-22, DECISIONS #36): rt p50 per idle cell, D (--micro-qos=default) vs H (high: QoS user-interactive
+ base priority 47) vs ref m1_mac_ac (09-24), per run and as medians, plus each run's exit rusage (P-core share, GHz) and the
correlation of the spin cells with the P-core share and the effective clock across runs. Usage: lc4_compare.py <lc4 dir>"""
import glob, json, re, sys
import numpy as np
import pandas as pd

LC = sys.argv[1]
REF = sorted(glob.glob("/Users/jostarigan/Project/Lightbound/results/m1_mac_ac/micro_P-M5_run*.csv.gz"))
PATHS, PAY = ["empty", "s1_copy", "s2_direct"], [0, 4096, 1048576]
KEY = ["path", "submit", "cpuwait", "payload_bytes"]
LABELS = "DH"


def p50(f):
    d = pd.read_csv(f, comment="#", low_memory=False)
    d = d[d.path.isin(PATHS) & d.payload_bytes.isin(PAY)]
    return d.groupby(KEY).rt_us.median()


runs, ru = {}, {}
for f in sorted(glob.glob(f"{LC}/lc_*_[{LABELS}].csv*")):
    m = re.search(rf"lc_(\d+)_([{LABELS}])\.csv", f)
    k = (int(m.group(1)), m.group(2))
    runs[k] = p50(f)
    try:
        ru[k] = json.load(open(f"{LC}/ru_{k[0]}_{k[1]}.json"))
    except OSError:
        ru[k] = {}
ref = pd.DataFrame({f"ref{i}": p50(f) for i, f in enumerate(REF, 1)}).median(axis=1)
per = pd.DataFrame({"ref_0924": ref, **{f"{i}{c}": v for (i, c), v in sorted(runs.items())}})
pd.set_option("display.width", 250)
print("rt_us p50 per run (columns = run index + D/H), ref = median of the 3 m1_mac_ac runs (09-24)")
print(per.round(0).to_string())
t = pd.DataFrame({"ref_0924": ref})
for c in LABELS:
    cols = [v for (i, cc), v in sorted(runs.items()) if cc == c]
    if cols:
        t[c] = pd.concat(cols, axis=1).median(axis=1)
        t[f"{c}_n"] = len(cols)
        t[f"d{c}"] = t[c] - t.ref_0924
if all(c in t for c in LABELS):
    t["H-D"] = t["H"] - t["D"]
print("\nmedians over runs")
print(t.round(0).to_string())
print("\nper run: exit rusage")
for k in sorted(runs):
    d = ru[k]
    print(f"  run {k[0]} {k[1]}: pcore {d.get('pcore_share')} GHz all {d.get('ghz_all')} P {d.get('ghz_pcore')} "
          f"E {d.get('ghz_ecore')} cpu {d.get('cpu_s')} s wall {d.get('wall_s')} s qos {d.get('qos_share')}")
spin = t.xs("spin", level="cpuwait")
for c in LABELS:
    if f"d{c}" in spin:
        print(f"spin cells d{c} vs 09-24: {spin[f'd{c}'].min():.0f} to {spin[f'd{c}'].max():.0f} µs")
# correlation across runs: spin-cell rt vs P-core share and effective clock
ks = sorted(runs)
ps = np.array([ru[k].get("pcore_share", np.nan) for k in ks], float)
gz = np.array([ru[k].get("ghz_all", np.nan) for k in ks], float)
print("\nacross runs: r(rt, P-core share) and r(rt, GHz all) per spin cell")
for idx in per.index:
    if idx[2] != "spin":
        continue
    y = np.array([runs[k].get(idx, np.nan) for k in ks], float)
    ok = ~np.isnan(y) & ~np.isnan(ps)
    r1 = np.corrcoef(y[ok], ps[ok])[0, 1] if ok.sum() > 2 else np.nan
    ok2 = ~np.isnan(y) & ~np.isnan(gz)
    r2 = np.corrcoef(y[ok2], gz[ok2])[0, 1] if ok2.sum() > 2 else np.nan
    print(f"  {idx[0]:10s} {idx[1]:8s} {idx[3]:8d}: r_pcore {r1:+.2f}  r_ghz {r2:+.2f}   rt per run {np.round(y).astype(int).tolist()}")
