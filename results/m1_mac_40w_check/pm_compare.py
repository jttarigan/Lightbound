#!/usr/bin/env python3
"""Clock check (10-25, STATUS to-do 2c, DECISIONS #36): per run (I = bench alone, L = bench + 2x yes) the powermetrics clocks
inside the run window (P/E cluster freq, the busiest P-core's freq and active share, cores > 90 % busy, GPU freq and active
share) next to rt p50 per idle cell vs the 09-24 reference (m1_mac_ac) and the exit rusage. Usage: pm_compare.py <pm dir>"""
import datetime as dt, glob, gzip, json, plistlib, re, statistics as st, sys
import pandas as pd

PM = sys.argv[1]
REF = sorted(glob.glob("/Users/jostarigan/Project/Lightbound/results/m1_mac_ac/micro_P-M5_run*.csv.gz"))
PATHS, PAY = ["empty", "s1_copy", "s2_direct"], [0, 4096, 1048576]
KEY = ["path", "submit", "cpuwait", "payload_bytes"]
TRIM = 2.0  # s cut from each end of a run window (start-up, shutdown)

# --- powermetrics samples: one plist per sample, NUL-separated; is_delta samples ending at timestamp (1-s resolution)
docs = [plistlib.loads(d) for d in gzip.open(f"{PM}/pm.plist.gz").read().split(b"\0") if d.strip()]
cum, ends = 0.0, []
for d in docs:
    cum += d["elapsed_ns"] / 1e9
    ends.append(cum)
ts = [d["timestamp"].replace(tzinfo=dt.timezone.utc).timestamp() for d in docs]
t0 = st.median(t - e for t, e in zip(ts, ends)) + 0.5  # second-resolution stamps: centre the offset
samples = []
for d, e in zip(docs, ends):
    end = t0 + e
    start = end - d["elapsed_ns"] / 1e9
    cl = {("E" if c["name"][0] == "E" else "P"): c for c in d["processor"]["clusters"]}  # M5: E + S(uper) cluster
    cpus = [("E" if c["name"][0] == "E" else "P", cpu) for c in d["processor"]["clusters"] for cpu in c["cpus"]]
    pc = [cpu for k, cpu in cpus if k == "P"]
    top = min(pc, key=lambda c: c["idle_ratio"])
    samples.append(dict(start=start, end=end, e_ghz=cl["E"]["freq_hz"] / 1e9, p_ghz=cl["P"]["freq_hz"] / 1e9,
                        p_act=1 - cl["P"]["idle_ratio"], top_ghz=top["freq_hz"] / 1e9, top_act=1 - top["idle_ratio"],
                        busy=sum(1 for _, c in cpus if c["idle_ratio"] < 0.1),
                        gpu_mhz=d["gpu"]["freq_hz"], gpu_act=1 - d["gpu"]["idle_ratio"]))
S = pd.DataFrame(samples)

# --- run windows from pm.log (epoch ms)
win = {}
for line in open(f"{PM}/pm.log"):
    m = re.search(r"run (\d+)_([IL]) (start|end) (\d+)", line)
    if m:
        win.setdefault((int(m.group(1)), m.group(2)), {})[m.group(3)] = int(m.group(4)) / 1000


def p50(f):
    d = pd.read_csv(f, comment="#", low_memory=False)
    d = d[d.path.isin(PATHS) & d.payload_bytes.isin(PAY)]
    return d.groupby(KEY).rt_us.median()


ref = pd.concat([p50(f) for f in REF], axis=1).median(axis=1)
rows, rt = [], {}
for k in sorted(win):
    w = win[k]
    s = S[(S.start >= w["start"] + TRIM) & (S.end <= w["end"] - TRIM)]
    ru = json.load(open(f"{PM}/ru_{k[0]}_{k[1]}.json"))
    rt[f"{k[0]}{k[1]}"] = p50(f"{PM}/pm_{k[0]}_{k[1]}.csv.gz")
    rows.append({"run": f"{k[0]}{k[1]}", "n": len(s), "P GHz": s.p_ghz.median(), "top-P GHz": s.top_ghz.median(),
                 "top-P GHz p10": s.top_ghz.quantile(.1), "top-P act": s.top_act.median(), "busy cores": s.busy.median(),
                 "E GHz": s.e_ghz.median(), "GPU MHz": s.gpu_mhz.median(), "GPU act": s.gpu_act.median(),
                 "ru P-share": ru.get("pcore_share"), "ru GHz": ru.get("ghz_all"), "ru cpu s": ru.get("cpu_s")})
pd.set_option("display.width", 200)
print("Clocks inside each run window (medians over 250 ms samples, %.0f s trimmed each end):" % TRIM)
print(pd.DataFrame(rows).set_index("run").round(3).to_string())
idle = S[S.end < min(w["start"] for w in win.values()) - 0.5]
if len(idle):
    print(f"\nBefore run 1 ({len(idle)} samples): P {idle.p_ghz.median():.2f} GHz, top-P {idle.top_ghz.median():.2f} GHz "
          f"act {idle.top_act.median():.2f}, GPU {idle.gpu_mhz.median():.0f} MHz")
T = pd.DataFrame(rt)
T["ref 09-24"] = ref
for c in rt:
    T[f"d{c}"] = T[c] - T["ref 09-24"]
print("\nrt p50 (µs) per idle cell; d = run − ref:")
print(T.round(0).to_string())
