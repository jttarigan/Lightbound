#!/usr/bin/env python3
"""M1 microbenchmark report (docs/06_RESEARCH_PROTOCOL.md §6, §8 figure 1).

Usage:
    python tools/analysis/micro_report.py results/ out/
    python tools/analysis/micro_report.py a.csv b.csv.gz c.csv --out out/   (.csv.gz accepted)

Inputs are micro.csv files written by `lightbound --mode=microbench` (one file per process
run; several runs per platform give the run-to-run variation). Outputs in the out dir:

    fig1_microbench.png/.pdf   round-trip latency vs payload (log-log), per platform & path
    fig1_modes.png             same, all submit x cpuwait modes (supplementary)
    summary.csv / summary.md   p50/p99 of rt, g2c, c2g + effective bandwidth per path x payload
    variation.csv              per-cell p50 spread across process restarts
    bandwidth.csv              raw GPU copy bandwidth (bw_d2h / bw_h2d rows)
    report.md                  tables + run-to-run check (< 10 %) + go criterion (§6)
"""
from __future__ import annotations

import argparse
import gzip
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
from matplotlib.lines import Line2D  # noqa: E402
from matplotlib.ticker import FuncFormatter, LogLocator  # noqa: E402
import numpy as np  # noqa: E402
import pandas as pd  # noqa: E402

# Primary condition of the study (docs/05 §3.2: chain is used for the main experiments).
PRIMARY_SUBMIT = "chain"
PRIMARY_WAIT = "spin"
VARIATION_LIMIT = 0.10  # M1 acceptance: p50 spread across 3 restarts < 10 %
RTX_PLATFORMS = ("P-3060", "P-4060")

# Fixed path -> color assignment (color follows the entity; reference categorical order).
PATH_ORDER = ["s2_direct", "s1_copy", "s2_hostcached", "s2_rebar", "s2_coherent"]
PATH_COLORS = {
    "s2_direct": "#2a78d6",
    "s1_copy": "#eb6834",
    "s2_hostcached": "#1baf7a",
    "s2_rebar": "#eda100",
    "s2_coherent": "#e87ba4",
}
PATH_MARKERS = {"s2_direct": "o", "s1_copy": "s", "s2_hostcached": "^", "s2_rebar": "D", "s2_coherent": "v"}
INK = "#0b0b0b"
INK_2 = "#52514e"
GRID = "#e4e3df"
SURFACE = "#fcfcfb"


def load(paths: list[Path]) -> tuple[pd.DataFrame, dict[str, dict[str, str]]]:
    files: list[Path] = []
    for p in paths:
        files.extend(sorted(list(p.rglob("micro*.csv")) + list(p.rglob("micro*.csv.gz"))) if p.is_dir() else [p])
    if not files:
        sys.exit("no micro*.csv files found")
    frames, headers = [], {}
    for f in files:
        header = {}
        opener = gzip.open if f.suffix == ".gz" else open
        with opener(f, "rt") as fh:
            for line in fh:
                if line.startswith("# ") and ": " in line:
                    k, v = line[2:].rstrip("\n").split(": ", 1)
                    header[k] = v
        df = pd.read_csv(f, comment="#")
        if df.empty:
            continue
        run = f.name.removesuffix(".gz").removesuffix(".csv")
        df["run"] = run
        headers[run] = header
        frames.append(df)
    data = pd.concat(frames, ignore_index=True)
    return data, headers


def fmt_payload(b: int) -> str:
    if b == 0:
        return "0"
    if b % (1 << 20) == 0:
        return f"{b >> 20}M"
    if b % (1 << 10) == 0:
        return f"{b >> 10}K"
    return str(b)


def summarize(rt: pd.DataFrame) -> pd.DataFrame:
    keys = ["platform", "path", "submit", "cpuwait", "payload_bytes"]
    g = rt.groupby(keys)
    s = g.agg(
        n=("rt_us", "count"),
        runs=("run", "nunique"),
        rt_p50=("rt_us", "median"),
        rt_p99=("rt_us", lambda x: x.quantile(0.99)),
        g2c_p50=("g2c_us", "median"),
        g2c_p99=("g2c_us", lambda x: x.quantile(0.99)),
        c2g_p50=("c2g_us", "median"),
        c2g_p99=("c2g_us", lambda x: x.quantile(0.99)),
        cpu_read_p50=("cpu_read_us", "median"),
        cpu_write_p50=("cpu_write_us", "median"),
        copy_g2c_p50=("copy_g2c_us", "median"),
        copy_c2g_p50=("copy_c2g_us", "median"),
    ).reset_index()
    # Effective bandwidth of the round trip: P bytes each way over the round-trip time.
    s["eff_bw_GBps"] = np.where(s["payload_bytes"] > 0, 2 * s["payload_bytes"] / (s["rt_p50"] * 1e-6) / 1e9, np.nan)
    return s


def variation(rt: pd.DataFrame) -> pd.DataFrame:
    keys = ["platform", "path", "submit", "cpuwait", "payload_bytes"]
    per_run = rt.groupby(keys + ["run"])["rt_us"].median().reset_index()
    v = per_run.groupby(keys)["rt_us"].agg(runs="count", p50_min="min", p50_max="max", p50_median="median").reset_index()
    v["spread"] = (v["p50_max"] - v["p50_min"]) / v["p50_median"]
    return v


def style_axes(ax):
    ax.set_facecolor(SURFACE)
    ax.grid(True, which="major", color=GRID, linewidth=0.8)
    ax.grid(False, which="minor")
    for side in ("top", "right"):
        ax.spines[side].set_visible(False)
    for side in ("left", "bottom"):
        ax.spines[side].set_color(INK_2)
    ax.tick_params(colors=INK_2, labelsize=8)


def plot_panel(ax, s: pd.DataFrame, platform: str, submit: str, wait: str, title: str, legend: bool):
    sub = s[(s.platform == platform) & (s.submit == submit) & (s.cpuwait == wait)]
    style_axes(ax)
    payloads = sorted(p for p in sub.payload_bytes.unique() if p > 0)
    for path in PATH_ORDER:
        d = sub[sub.path == path].sort_values("payload_bytes")
        d = d[d.payload_bytes > 0]
        if d.empty:
            continue
        c = PATH_COLORS[path]
        ax.plot(d.payload_bytes, d.rt_p50, color=c, linewidth=2, marker=PATH_MARKERS[path], markersize=5,
                markeredgecolor=SURFACE, markeredgewidth=1.5, label=f"{path} p50", zorder=3)
        ax.plot(d.payload_bytes, d.rt_p99, color=c, linewidth=1, linestyle="--", alpha=0.8, zorder=2)
    empty = sub[sub.path == "empty"]
    if not empty.empty:
        e = float(empty.rt_p50.iloc[0])
        ax.axhline(e, color=INK_2, linewidth=1, linestyle=":", zorder=1)
        ax.annotate(f"empty RT p50 {e:.0f} µs", xy=(0.02, e), xycoords=("axes fraction", "data"),
                    xytext=(0, 3), textcoords="offset points", fontsize=7, color=INK_2)
    ax.set_xscale("log", base=2)
    ax.set_yscale("log")
    ax.yaxis.set_major_locator(LogLocator(base=10, subs=(1.0, 2.0, 5.0)))
    ax.yaxis.set_minor_locator(LogLocator(base=10, subs=(3.0, 4.0, 6.0, 7.0, 8.0, 9.0)))
    ax.yaxis.set_major_formatter(FuncFormatter(lambda y, _: f"{y:g}"))
    ax.yaxis.set_minor_formatter(FuncFormatter(lambda y, _: ""))
    ax.margins(x=0.06)
    if payloads:
        ax.set_xticks(payloads)
        ax.set_xticklabels([fmt_payload(int(p)) for p in payloads])
    ax.set_title(title, fontsize=9, color=INK, loc="left")
    ax.set_xlabel("payload per direction", fontsize=8, color=INK_2)
    ax.set_ylabel("round trip (µs)", fontsize=8, color=INK_2)
    del legend  # one figure-level legend (see add_legend)


def add_legend(fig, s: pd.DataFrame):
    present = [p for p in PATH_ORDER if p in set(s.path)]
    handles = [Line2D([], [], color=PATH_COLORS[p], linewidth=2, marker=PATH_MARKERS[p], markersize=5,
                      markeredgecolor=SURFACE, label=p) for p in present]
    handles.append(Line2D([], [], color=INK_2, linewidth=1, linestyle="--", label="p99 (same color)"))
    handles.append(Line2D([], [], color=INK_2, linewidth=1, linestyle=":", label="empty round trip p50"))
    fig.legend(handles=handles, loc="upper left", bbox_to_anchor=(0.01, 0.93), ncol=len(handles),
               fontsize=7, frameon=False, labelcolor=INK)


def figure1(s: pd.DataFrame, out: Path):
    platforms = sorted(s.platform.unique())
    fig, axes = plt.subplots(1, len(platforms), figsize=(4.2 * len(platforms), 3.6), squeeze=False, sharey=True)
    fig.patch.set_facecolor(SURFACE)
    for i, p in enumerate(platforms):
        plot_panel(axes[0][i], s, p, PRIMARY_SUBMIT, PRIMARY_WAIT, f"{p}  ({PRIMARY_SUBMIT}, {PRIMARY_WAIT})", i == 0)
    fig.suptitle("Figure 1 — CPU↔GPU round-trip latency vs payload (solid p50, dashed p99)",
                 fontsize=10, color=INK, x=0.01, ha="left")
    fig.tight_layout(rect=(0, 0, 1, 0.88))
    add_legend(fig, s)
    fig.savefig(out / "fig1_microbench.png", dpi=160, facecolor=SURFACE)
    fig.savefig(out / "fig1_microbench.pdf", facecolor=SURFACE)
    plt.close(fig)

    modes = [("chain", "spin"), ("chain", "block"), ("perpass", "spin"), ("perpass", "block")]
    fig, axes = plt.subplots(len(platforms), 4, figsize=(15, 3.3 * len(platforms)), squeeze=False, sharey=True)
    fig.patch.set_facecolor(SURFACE)
    for i, p in enumerate(platforms):
        for j, (sm, wm) in enumerate(modes):
            plot_panel(axes[i][j], s, p, sm, wm, f"{p}  {sm} / {wm}", i == 0 and j == 0)
    fig.tight_layout(rect=(0, 0, 1, 1 - 0.35 / len(platforms)))
    add_legend(fig, s)
    fig.savefig(out / "fig1_modes.png", dpi=130, facecolor=SURFACE)
    plt.close(fig)


def markdown_table(s: pd.DataFrame, platform: str) -> str:
    sub = s[(s.platform == platform) & (s.submit == PRIMARY_SUBMIT) & (s.cpuwait == PRIMARY_WAIT)]
    payloads = sorted(sub.payload_bytes.unique())
    lines = [f"| path | " + " | ".join(fmt_payload(int(p)) for p in payloads) + " |",
             "|---|" + "---:|" * len(payloads)]
    for path in ["empty"] + PATH_ORDER:
        d = sub[sub.path == path]
        if d.empty:
            continue
        cells = []
        for p in payloads:
            r = d[d.payload_bytes == p]
            cells.append("—" if r.empty else f"{r.rt_p50.iloc[0]:.0f} / {r.rt_p99.iloc[0]:.0f}")
        lines.append(f"| {path} | " + " | ".join(cells) + " |")
    return "\n".join(lines)


def go_criterion(s: pd.DataFrame, v: pd.DataFrame) -> str:
    platforms = set(s.platform)
    rtx = [p for p in RTX_PLATFORMS if p in platforms]
    if "P-M5" not in platforms or not rtx:
        missing = [p for p in ("P-M5",) + RTX_PLATFORMS if p not in platforms]
        return f"**PENDING** — needs data from {', '.join(missing)}."

    def cell(platform, path, payload):
        r = s[(s.platform == platform) & (s.path == path) & (s.submit == PRIMARY_SUBMIT) &
              (s.cpuwait == PRIMARY_WAIT) & (s.payload_bytes == payload)]
        return None if r.empty else float(r.rt_p50.iloc[0])

    def noise(platform, path, payload):
        r = v[(v.platform == platform) & (v.path == path) & (v.submit == PRIMARY_SUBMIT) &
              (v.cpuwait == PRIMARY_WAIT) & (v.payload_bytes == payload)]
        return float(r.spread.iloc[0]) if not r.empty and r.runs.iloc[0] >= 2 else float("nan")

    lines, verdict = [], True
    for label, payload, m5_path, vk_paths in (("empty round trip", 0, "empty", ["empty"]),
                                              ("1 MB round trip", 1 << 20, "s2_direct", PATH_ORDER)):
        m5 = cell("P-M5", m5_path, payload)
        m5_noise = noise("P-M5", m5_path, payload)
        for p in rtx:
            best = min(((cell(p, vp, payload), vp) for vp in vk_paths if cell(p, vp, payload) is not None),
                       default=(None, None))
            if m5 is None or best[0] is None:
                lines.append(f"- {label}: missing data for P-M5 or {p}")
                verdict = False
                continue
            rtx_noise = noise(p, best[1], payload)
            margin = np.nanmax([m5_noise, rtx_noise, 0.0])
            ok = m5 * (1 + margin) < best[0] * (1 - margin)
            verdict &= bool(ok)
            lines.append(f"- {label}: P-M5 {m5:.1f} µs vs {p} best ({best[1]}) {best[0]:.1f} µs, "
                         f"noise margin ±{margin * 100:.1f} % → {'M5 clearly lower' if ok else 'NOT clearly lower'}")
    head = "**GO**" if verdict else "**NO-GO** (protocol §6: stop and re-scope)"
    return head + " — criterion: empty and 1 MB round-trip p50 on M5 lower than both RTX cards under their best " \
                  "Vulkan path by more than run-to-run noise.\n\n" + "\n".join(lines)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("inputs", nargs="+", type=Path, help="micro.csv files or directories (last one = out dir if --out absent)")
    ap.add_argument("--out", type=Path, default=None)
    args = ap.parse_args()
    inputs, out = args.inputs, args.out
    if out is None:
        if len(inputs) < 2:
            sys.exit("give an output directory (positional or --out)")
        out = inputs[-1]
        inputs = inputs[:-1]
    out.mkdir(parents=True, exist_ok=True)

    data, headers = load(inputs)
    bw = data[data.path.str.startswith("bw_")].copy()
    rt = data[~data.path.str.startswith("bw_")].copy()

    s = summarize(rt)
    v = variation(rt)
    s.to_csv(out / "summary.csv", index=False)
    v.to_csv(out / "variation.csv", index=False)
    if not bw.empty:
        b = bw.groupby(["platform", "path", "payload_bytes"])["rt_us"].median().reset_index(name="copy_p50_us")
        b["GBps"] = b.payload_bytes / (b.copy_p50_us * 1e-6) / 1e9
        b.to_csv(out / "bandwidth.csv", index=False)
    figure1(s, out)

    md = ["# M1 microbenchmark report", ""]
    md.append("Runs: " + ", ".join(f"`{r}` ({h.get('sysinfo.platform', '?')}, {h.get('sysinfo.power_source', '?')}, "
                                   f"thermal {h.get('sysinfo.thermal_state_start', '?')}→{h.get('thermal_state_end', '?')}, "
                                   f"failures {h.get('failures', '?')})" for r, h in sorted(headers.items())))
    md.append("")
    for platform in sorted(s.platform.unique()):
        md.append(f"## {platform} — round trip p50 / p99 (µs), {PRIMARY_SUBMIT} / {PRIMARY_WAIT}")
        md.append("")
        md.append(markdown_table(s, platform))
        md.append("")
        if not bw.empty:
            bp = bw[bw.platform == platform].groupby(["path", "payload_bytes"])["rt_us"].median()
            for (path, payload), us in bp.items():
                md.append(f"- raw copy {path} {fmt_payload(int(payload))}: {us:.1f} µs = "
                          f"{payload / (us * 1e-6) / 1e9:.2f} GB/s")
            md.append("")
    md.append("## Run-to-run variation (p50 across process restarts)")
    md.append("")
    multi = v[v.runs >= 2]
    if multi.empty:
        md.append("Only one run per platform: variation not measured.")
    else:
        for platform in sorted(multi.platform.unique()):
            m = multi[multi.platform == platform]
            worst = m.sort_values("spread", ascending=False).iloc[0]
            bad = m[m.spread >= VARIATION_LIMIT]
            prim = m[(m.submit == PRIMARY_SUBMIT) & (m.cpuwait == PRIMARY_WAIT)]
            md.append(f"- {platform}: {int(m.runs.max())} runs, {len(m)} cells; cells ≥ 10 %: {len(bad)}; "
                      f"worst {worst.spread * 100:.1f} % ({worst.path} {worst.submit}/{worst.cpuwait} "
                      f"{fmt_payload(int(worst.payload_bytes))}); worst in primary condition "
                      f"{prim.spread.max() * 100:.1f} % → {'PASS' if len(bad) == 0 else 'FAIL'}")
    md.append("")
    md.append("## Latency modes (empty round trip, all runs pooled)")
    md.append("")
    md.append("A single p50 hides multimodal sync latency; histogram peaks (5 µs bins, ≥ 1 % of samples):")
    md.append("")
    for platform in sorted(rt.platform.unique()):
        e = rt[(rt.platform == platform) & (rt.path == "empty")]
        parts = []
        for col in ("g2c_us", "c2g_us", "rt_us"):
            x = e[col].dropna().to_numpy()
            if x.size == 0:
                continue
            h, edges = np.histogram(x, bins=np.arange(0, max(600.0, float(np.percentile(x, 99))) + 5, 5))
            peaks = [f"{edges[i]:.0f}–{edges[i + 1]:.0f} ({h[i] / x.size * 100:.0f} %)" for i in range(1, len(h) - 1)
                     if h[i] >= h[i - 1] and h[i] >= h[i + 1] and h[i] > 0.01 * x.size]
            parts.append(f"{col.replace('_us', '')}: " + ", ".join(peaks))
        md.append(f"- {platform}: " + "; ".join(parts))
    md.append("")
    md.append("## Go criterion (protocol §6)")
    md.append("")
    md.append(go_criterion(s, v))
    md.append("")
    md.append("Figures: `fig1_microbench.png` (primary condition), `fig1_modes.png` (all submit × wait modes).")
    (out / "report.md").write_text("\n".join(md) + "\n")
    s_md = [f"## {p}\n\n" + markdown_table(s, p) for p in sorted(s.platform.unique())]
    (out / "summary.md").write_text("\n\n".join(s_md) + "\n")
    print((out / "report.md").read_text())


if __name__ == "__main__":
    main()
