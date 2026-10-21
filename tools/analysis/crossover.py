#!/usr/bin/env python3
"""Payload crossover report for H1′ (docs/06_RESEARCH_PROTOCOL.md §1, §6 "Extension for H1′").

Usage:
    python tools/analysis/crossover.py M5=results/m1_mac_ext 3060=results/m1_3060_ext --shared M5 --out results/crossover_ext
    python tools/analysis/crossover.py M5=results/m1_mac_ac 3060=results/m1_3060 3060-rebaroff=results/m1_3060_rebar-off \\
        --shared M5 --out results/crossover_m1          (the M1 grid: 1 and 4 MiB are neighbours)

Each NAME=PATH is one series (micro*.csv[.gz] files, read with micro_report.load). Series are never
pooled with each other: the ReBAR-off series and every *_ext series get their own name. The --shared
series is compared with each other one, per GPU state (idle / hot cells, DECISIONS #32) and per
submit x cpuwait mode (default: the primary chain/spin only).

Per comparison and payload, d = ln(rt_pcie / rt_shared) on the p50 round trip. The crossover is where
d changes sign between neighbouring payloads, interpolated linearly in log-log (the protocol's rule).
Path pairs: "best" (each series' fastest path at that payload, as in the go criterion; sensitivity
paths s2_rebar / s2_coherent are excluded), "copy" (s1_copy vs s1_copy) and "direct" (s2_direct vs
s2_direct). The error bar is the run-to-run spread: the crossover for every pair of runs (run i of
the shared series vs run j of the other), min-max. Outputs in --out:

    crossover.csv   one row per comparison x state x mode x pair x crossing (pooled + spread)
    ratios.csv      rt_pcie / rt_shared per payload (pooled p50)
    crossover.md    tables + the H1′ predictions next to the measurement
"""
from __future__ import annotations

import argparse
import itertools
import math
import sys
from pathlib import Path

import numpy as np
import pandas as pd

sys.path.insert(0, str(Path(__file__).resolve().parent))
from micro_report import PRIMARY_SUBMIT, PRIMARY_WAIT, fmt_payload, load  # noqa: E402

SENSITIVITY_PATHS = ("s2_rebar", "s2_coherent")  # strawmen (DECISIONS #17), never "best"
PAIRS = {"best": None, "copy": ("s1_copy", "s1_copy"), "direct": ("s2_direct", "s2_direct")}
BYTES_PER_AGENT = 28  # per direction, average (docs/M1_RESCOPE_PROPOSAL.md: 1 MiB ≈ 37k agents)
MIB = 1 << 20

# H1′ predictions, written 2026-10-13 before the extension (protocol §1). They are against the RTX 3060 Ti,
# best path, chain/spin. "idle"/"hot" = GPU state. Do not edit after the extension has been measured.
PRED_CROSSOVER_MIB = {"idle": (2.0, 2.6), "hot": (0.0, 0.25)}
PRED_RATIO_HOT = {1 * MIB: 1.3, 4 * MIB: 1.8, 8 * MIB: 1.8, 16 * MIB: 1.8}


def crossings(payloads: list[int], d: list[float]) -> list[tuple[float, str]]:
    """Sign changes of d = ln(rt_pcie / rt_shared) between neighbouring payloads, interpolated in log-log.

    Returns (payload in bytes, "shared faster above" | "pcie faster above") per crossing. A grid point with
    d == 0 counts as the crossing itself.

    >>> [(round(p / MIB, 2), s) for p, s in crossings([MIB, 4 * MIB], [math.log(160 / 305), math.log(466 / 316)])]
    [(2.38, 'shared faster above')]
    >>> crossings([MIB, 4 * MIB], [-0.5, -0.1])
    []
    >>> [(p, s) for p, s in crossings([1, 2, 4], [-1.0, 0.0, 1.0])]
    [(2.0, 'shared faster above')]
    """
    out = []
    for (p0, d0), (p1, d1) in zip(zip(payloads, d), zip(payloads[1:], d[1:])):
        if d0 == 0 or d0 * d1 >= 0 and d1 != 0:
            continue
        if d1 == 0:
            x = math.log(p1)
        else:
            x = math.log(p0) + d0 / (d0 - d1) * (math.log(p1) - math.log(p0))
        out.append((math.exp(x), "shared faster above" if d1 > d0 else "pcie faster above"))
    return out


def p50_table(df: pd.DataFrame, by_run: bool) -> pd.DataFrame:
    """p50 rt per (gpuload, submit, cpuwait, path, payload[, run]); pooled = median over all samples of all runs."""
    keys = ["gpuload", "submit", "cpuwait", "path", "payload_bytes"] + (["run"] if by_run else [])
    return df.groupby(keys).rt_us.median().rename("rt_p50").reset_index()


def curve(t: pd.DataFrame, pair_side: str | None) -> pd.Series:
    """rt p50 vs payload for one path, or the per-payload minimum over the candidate paths ("best")."""
    if pair_side is None:
        t = t[~t.path.isin(SENSITIVITY_PATHS)]
        return t.groupby("payload_bytes").rt_p50.min()
    return t[t.path == pair_side].set_index("payload_bytes").rt_p50


def compare(shared: pd.Series, pcie: pd.Series) -> tuple[list[int], list[float]]:
    common = sorted(set(shared.index[shared.index > 0]) & set(pcie.index[pcie.index > 0]))
    return common, [math.log(pcie[p] / shared[p]) for p in common]


def analyse(series: dict[str, pd.DataFrame], shared_name: str, modes: str):
    rows, ratio_rows = [], []
    sh_pool, sh_run = p50_table(series[shared_name], False), p50_table(series[shared_name], True)
    for other_name, other in series.items():
        if other_name == shared_name:
            continue
        ot_pool, ot_run = p50_table(other, False), p50_table(other, True)
        conds = sh_pool[["gpuload", "submit", "cpuwait"]].drop_duplicates().merge(
            ot_pool[["gpuload", "submit", "cpuwait"]].drop_duplicates())
        if modes == "primary":
            conds = conds[(conds.submit == PRIMARY_SUBMIT) & (conds.cpuwait == PRIMARY_WAIT)]
        for c in conds.itertuples(index=False):
            sel = lambda t: t[(t.gpuload == c.gpuload) & (t.submit == c.submit) & (t.cpuwait == c.cpuwait)]  # noqa: E731
            sp, op, sr, orr = sel(sh_pool), sel(ot_pool), sel(sh_run), sel(ot_run)
            for pair, sides in PAIRS.items():
                s_side, o_side = (None, None) if sides is None else sides
                s_curve, o_curve = curve(sp, s_side), curve(op, o_side)
                if s_curve.empty or o_curve.empty:
                    continue
                grid, d = compare(s_curve, o_curve)
                for p in grid:
                    ratio_rows.append({"shared": shared_name, "other": other_name, "gpuload": c.gpuload,
                                       "submit": c.submit, "cpuwait": c.cpuwait, "pair": pair, "payload_bytes": p,
                                       "rt_shared": s_curve[p], "rt_other": o_curve[p],
                                       "ratio_other_over_shared": o_curve[p] / s_curve[p]})
                pooled = crossings(grid, d)
                # Run-to-run spread: every run of the shared series against every run of the other.
                per_run = []
                for rs, ro in itertools.product(sorted(sr.run.unique()), sorted(orr.run.unique())):
                    g, dd = compare(curve(sr[sr.run == rs], s_side), curve(orr[orr.run == ro], o_side))
                    per_run.append([p for p, kind in crossings(g, dd) if kind == "shared faster above"])
                first = [x[0] for x in per_run if x]
                base = {"shared": shared_name, "other": other_name, "gpuload": c.gpuload, "submit": c.submit,
                        "cpuwait": c.cpuwait, "pair": pair, "grid": " ".join(fmt_payload(p) for p in grid),
                        "run_pairs": len(per_run), "run_pairs_with_crossing": len(first),
                        "spread_min_MiB": min(first) / MIB if first else np.nan,
                        "spread_max_MiB": max(first) / MIB if first else np.nan}
                if not pooled:
                    rows.append({**base, "crossing": 0, "kind": "none in grid", "crossover_MiB": np.nan,
                                 "agents_k": np.nan,
                                 "faster_everywhere": "shared" if all(x > 0 for x in d) else "pcie"})
                for k, (p, kind) in enumerate(pooled, start=1):
                    rows.append({**base, "crossing": k, "kind": kind, "crossover_MiB": p / MIB,
                                 "agents_k": p / BYTES_PER_AGENT / 1000, "faster_everywhere": ""})
    x, ratios = pd.DataFrame(rows), pd.DataFrame(ratio_rows)
    if x.empty:
        sys.exit("no comparable cells (same GPU state, submit and cpuwait mode in both series)")
    order = {"idle": 0, "hot": 1, **{k: i for i, k in enumerate(PAIRS)}}  # idle first (the protocol's order)
    key = lambda c: c.map(lambda v: order.get(v, v)) if c.name in ("gpuload", "pair") else c  # noqa: E731
    x = x.sort_values(["other", "gpuload", "submit", "cpuwait", "pair", "crossing"], key=key, ignore_index=True)
    ratios = ratios.sort_values(["other", "gpuload", "submit", "cpuwait", "pair", "payload_bytes"], key=key,
                                ignore_index=True)
    return x, ratios


def verdict(r) -> str:
    """Pooled crossover and run spread against the H1′ interval (only the primary best-path comparison)."""
    lo, hi = PRED_CROSSOVER_MIB[r.gpuload]
    if math.isnan(r.crossover_MiB):
        return "no crossover in the grid"
    s_lo = r.spread_min_MiB if not math.isnan(r.spread_min_MiB) else r.crossover_MiB
    s_hi = r.spread_max_MiB if not math.isnan(r.spread_max_MiB) else r.crossover_MiB
    if lo <= r.crossover_MiB <= hi:
        return "inside the predicted interval"
    if s_hi >= lo and s_lo <= hi:
        return "pooled value outside, run spread overlaps the predicted interval"
    return "below the predicted interval" if r.crossover_MiB < lo else "above the predicted interval"


def md_table(t: pd.DataFrame) -> str:
    """Pipe table without the optional tabulate dependency (pandas' to_markdown needs it)."""
    lines = ["| " + " | ".join(map(str, t.columns)) + " |", "|" + "---|" * len(t.columns)]
    lines += ["| " + " | ".join("" if pd.isna(v) else str(v) for v in row) + " |" for row in t.itertuples(index=False)]
    return "\n".join(lines)


def to_markdown(x: pd.DataFrame, ratios: pd.DataFrame, names: dict[str, str]) -> str:
    out = ["# Payload crossover (H1′)", "",
           "Generated by `tools/analysis/crossover.py`. Series: " +
           ", ".join(f"`{k}` = `{v}`" for k, v in names.items()) + ".",
           f"Crossover = sign change of ln(rt_other / rt_shared) on the p50 round trip, log-log interpolated between "
           f"neighbouring payloads. Spread = min–max over every pair of runs. Agents at {BYTES_PER_AGENT} B per "
           f"direction. Nothing is measured between grid points.", ""]
    cols = ["other", "gpuload", "submit", "cpuwait", "pair", "kind", "crossover_MiB", "spread_min_MiB",
            "spread_max_MiB", "run_pairs_with_crossing", "run_pairs", "agents_k", "faster_everywhere"]
    t = x[cols].copy()
    for c in ("crossover_MiB", "spread_min_MiB", "spread_max_MiB"):
        t[c] = t[c].map(lambda v: "" if pd.isna(v) else f"{v:.2f}")
    t["agents_k"] = t["agents_k"].map(lambda v: "" if pd.isna(v) else f"{v:.0f}k")
    out += ["## Crossovers", "", md_table(t), ""]

    prim = x[(x.pair == "best") & (x.submit == PRIMARY_SUBMIT) & (x.cpuwait == PRIMARY_WAIT)
             & (x.kind != "pcie faster above")]
    out += ["## Against the H1′ predictions (best path, chain/spin; written 2026-10-13)", "",
            "H1′ is tested on the extension series (12-point grid, idle and hot). On any other grid this "
            "section is a preview only.", ""]
    for r in prim.itertuples(index=False):
        lo, hi = PRED_CROSSOVER_MIB[r.gpuload]
        pred = f"{lo}–{hi} MiB" if lo > 0 else f"≤ {hi * 1024:.0f} KiB"
        meas = "none" if pd.isna(r.crossover_MiB) else (
            f"{r.crossover_MiB:.2f} MiB (runs {r.spread_min_MiB:.2f}–{r.spread_max_MiB:.2f})")
        out.append(f"- `{r.other}`, {r.gpuload} GPU: predicted {pred}, measured {meas}: **{verdict(r)}**.")
    hot = ratios[(ratios.pair == "best") & (ratios.gpuload == "hot") & (ratios.submit == PRIMARY_SUBMIT)
                 & (ratios.cpuwait == PRIMARY_WAIT) & ratios.payload_bytes.isin(PRED_RATIO_HOT)]
    for r in hot.itertuples(index=False):
        out.append(f"- `{r.other}`, hot GPU, {fmt_payload(r.payload_bytes)}: predicted shared "
                   f"{PRED_RATIO_HOT[r.payload_bytes]}× faster, measured {r.ratio_other_over_shared:.2f}× "
                   "(no tolerance was fixed in advance; compare by eye).")
    if hot.empty:
        out.append("- Hot-GPU speed ratios: no hot cells in these series.")
    out += ["", "## Speed ratio rt_other / rt_shared (pooled p50, best path; > 1 = shared faster)", ""]
    rb = ratios[ratios.pair == "best"]
    if not rb.empty:
        piv = rb.pivot_table(index=["other", "gpuload", "submit", "cpuwait"], columns="payload_bytes",
                             values="ratio_other_over_shared")
        piv.columns = [fmt_payload(c) for c in piv.columns]
        piv = piv.round(2).reset_index()
        piv = piv.sort_values(["other", "gpuload"], key=lambda c: c.map({"idle": 0, "hot": 1}).fillna(0)
                              if c.name == "gpuload" else c, kind="stable")
        out += [md_table(piv), ""]
    return "\n".join(out) + "\n"


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("series", nargs="+", help="NAME=PATH, one per series (directory or micro*.csv[.gz])")
    ap.add_argument("--shared", required=True, help="NAME of the shared-memory series (the M5)")
    ap.add_argument("--modes", choices=["primary", "all"], default="primary",
                    help="primary = chain/spin only; all = every submit x cpuwait mode in both series")
    ap.add_argument("--out", type=Path, required=True)
    a = ap.parse_args()

    names = dict(s.split("=", 1) for s in a.series)
    if a.shared not in names or len(names) < 2:
        sys.exit("need --shared NAME among the series and at least one other series")
    series = {}
    for name, path in names.items():
        df, _, _ = load([Path(path)])
        platforms = df.platform.unique()
        if len(platforms) != 1:
            sys.exit(f"{name}: one platform per series expected, found {list(platforms)}")
        df = df[~df.path.str.startswith("bw_")]  # raw copy-bandwidth rows are not round trips (micro_report)
        series[name] = df[["gpuload", "submit", "cpuwait", "path", "payload_bytes", "run", "rt_us"]]
    x, ratios = analyse(series, a.shared, a.modes)
    a.out.mkdir(parents=True, exist_ok=True)
    x.to_csv(a.out / "crossover.csv", index=False)
    ratios.to_csv(a.out / "ratios.csv", index=False)
    (a.out / "crossover.md").write_text(to_markdown(x, ratios, names), encoding="utf-8")
    print((a.out / "crossover.md").read_text(encoding="utf-8"))


if __name__ == "__main__":
    main()
