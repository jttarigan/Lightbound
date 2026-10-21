#!/usr/bin/env python3
"""A/B gate: rt_us p50/p99 per idle cell (paths empty/s1_copy/s2_direct, 4K and 1M) for each CSV, vs 10-13 m1_mac_ac."""
import sys, glob, pandas as pd
REF = sorted(glob.glob("/Users/jostarigan/Project/Lightbound/results/m1_mac_ac/micro_P-M5_run*.csv"))
PATHS = ["empty", "s1_copy", "s2_direct"]; PAY = [4096, 1048576]
def load(f):
    d = pd.read_csv(f, comment="#", low_memory=False)
    if "gpuload" in d.columns: d = d[d.gpuload.astype(str).isin(["idle", "na", "nan"])]
    d = d[d.path.isin(PATHS) & d.payload_bytes.isin(PAY)]
    return d.groupby(["path", "submit", "cpuwait", "payload_bytes"]).rt_us.agg(p50="median", p99=lambda s: s.quantile(.99))
cols = {}
for f in REF: cols["ref1013_" + f.split("_")[-1][:4]] = load(f).p50
for f in sys.argv[1:]: cols[f.rsplit("/", 1)[-1].split(".")[0]] = load(f).p50
t = pd.DataFrame(cols).round(0)
ref = t[[c for c in t if c.startswith("ref1013")]].median(axis=1)
for c in t:
    if not c.startswith("ref1013"): t["d_" + c] = (t[c] - ref).round(0)
pd.set_option("display.width", 250); pd.set_option("display.max_columns", 30)
print(t.to_string())
