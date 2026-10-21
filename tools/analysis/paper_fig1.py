"""Paper figure 1: round trip vs payload, M5 vs RTX 3060 Ti (ReBAR on / off), idle GPU, chain/spin.

    python3 tools/analysis/paper_fig1.py [--out docs/paper/fig]

Reads only committed reports: results/m1_report/summary.csv (P-M5 and P-3060 with ReBAR on),
results/m1_report_rebar-off/summary.csv (P-3060 with ReBAR off) and results/m1_crossover/crossover.csv
(tools/analysis/crossover.py). Panel (a): the direct path (s2_direct); panel (b): the copy path (s1_copy).
Same log-log scale in both panels. Solid line = p50; the shaded band reaches up to p99. The empty round trip
is marked at the left of each panel. The grey column in each panel spans the crossovers of the curves it
shows: min-max over all run pairs and both ReBAR settings (until 10-21 it was a fixed 2.0-2.6 MiB, which
matched neither visible crossing).
"""
import argparse
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import pandas as pd

ROOT = Path(__file__).resolve().parents[2]
# Categorical slots 1-3 of the dataviz reference palette (validated all-pairs, light mode). Slot 3 sits below 3:1
# on the surface, so every series is also direct-labeled and has its own marker.
SERIES = [
    # (label, report dir, platform, color, marker)
    ("Apple M5 (shared memory)", "m1_report", "P-M5", "#2a78d6", "o"),
    ("RTX 3060 Ti, ReBAR on", "m1_report", "P-3060", "#eb6834", "s"),
    ("RTX 3060 Ti, ReBAR off", "m1_report_rebar-off", "P-3060", "#1baf7a", "^"),
]
INK, MUTED, GRID, SURFACE = "#0b0b0b", "#52514e", "#e4e3df", "#fcfcfb"
EMPTY_X = 1024  # where the empty round trip is drawn (left of the 4 KiB grid point)
PANEL_PAIR = {"s2_direct": "direct", "s1_copy": "copy"}  # crossover.py's path pairs


def crossover_span(pair: str) -> tuple[float, float]:
    """Bytes spanned by the M5-vs-3060 Ti crossovers of one path pair (idle, chain/spin), both ReBAR settings."""
    x = pd.read_csv(ROOT / "results" / "m1_crossover" / "crossover.csv")
    x = x[(x.gpuload == "idle") & (x.submit == "chain") & (x.cpuwait == "spin") & (x.pair == pair)
          & (x.kind == "shared faster above")]
    return x.spread_min_MiB.min() * (1 << 20), x.spread_max_MiB.max() * (1 << 20)


def load(report: str, platform: str, path: str) -> tuple[pd.DataFrame, pd.DataFrame]:
    df = pd.read_csv(ROOT / "results" / report / "summary.csv")
    df = df[(df.platform == platform) & (df.submit == "chain") & (df.cpuwait == "spin")]
    return df[df.path == path].sort_values("payload_bytes"), df[df.path == "empty"]


def label_size(b: float) -> str:
    return f"{int(b) >> 20}M" if b >= 1 << 20 else f"{int(b) >> 10}K"


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", type=Path, default=ROOT / "docs" / "paper" / "fig")
    args = ap.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)

    # Journal figure rules (docs/paper/OUTLINE.md "Submission requirements"): Arial/Helvetica, 8-12 pt at print
    # size, fonts embedded in the vector file, full width 174 mm.
    plt.rcParams.update({"font.family": "sans-serif", "font.sans-serif": ["Arial", "Helvetica"],
                         "pdf.fonttype": 42, "ps.fonttype": 42, "font.size": 9, "axes.edgecolor": MUTED, "axes.labelcolor": INK,
                         "xtick.color": MUTED, "ytick.color": MUTED, "figure.facecolor": SURFACE,
                         "axes.facecolor": SURFACE, "savefig.facecolor": SURFACE})
    fig, axes = plt.subplots(1, 2, figsize=(174 / 25.4, 3.1), sharey=True, constrained_layout=True)
    for ax, (path, title) in zip(axes, [("s2_direct", "(a) direct path, s2_direct"),
                                        ("s1_copy", "(b) copy path, s1_copy")]):
        ends = []
        for label, report, platform, color, marker in SERIES:
            d, empty = load(report, platform, path)
            x, p50, p99 = d.payload_bytes.to_numpy(), d.rt_p50.to_numpy(), d.rt_p99.to_numpy()
            ax.fill_between(x, p50, p99, color=color, alpha=0.12, linewidth=0)
            ax.plot(x, p50, color=color, linewidth=2, marker=marker, markersize=5,
                    markeredgecolor=SURFACE, markeredgewidth=1, label=label, zorder=3)
            if not empty.empty:
                ax.plot([EMPTY_X], [empty.rt_p50.iloc[0]], marker=marker, markersize=5, color=color,
                        markeredgecolor=SURFACE, markeredgewidth=1, linestyle="none", zorder=3)
            short = {"P-M5": "M5"}.get(platform, "3060 Ti " + ("off" if "off" in report else "on"))
            ends.append((short, x[-1], p50[-1]))
        # Direct labels at the right end, in ink; the two ReBAR series merge when their ends overlap (< 15 %).
        ends.sort(key=lambda e: e[2])
        merged = []
        for short, xe, ye in ends:
            if merged and ye / merged[-1][2] < 1.15 and short.startswith("3060") and merged[-1][0].startswith("3060"):
                merged[-1] = ("3060 Ti on/off", xe, max(ye, merged[-1][2]))
            else:
                merged.append((short, xe, ye))
        for short, xe, ye in merged:
            ax.annotate(short, (xe, ye), xytext=(5, 0), textcoords="offset points", va="center", fontsize=8,
                        color=INK)
        ax.set_xscale("log", base=2)
        ax.set_yscale("log")
        ticks = [EMPTY_X, 4096, 65536, 262144, 1 << 20, 4 << 20, 16 << 20]
        ax.set_xticks(ticks, ["empty"] + [label_size(t) for t in ticks[1:]])
        ax.set_xlim(EMPTY_X / 1.6, (16 << 20) * 2.6)
        lo, hi = crossover_span(PANEL_PAIR[path])
        ax.axvspan(lo, hi, color=MUTED, alpha=0.10, linewidth=0)
        if path == "s2_direct":
            ax.annotate("idle crossover,\ninterpolated", ((lo * hi) ** 0.5, 60), ha="center", fontsize=8,
                        color=MUTED)
        ax.set_title(title, loc="left", fontsize=9, color=INK)
        ax.set_xlabel("payload per direction (bytes)")
        ax.grid(True, which="major", color=GRID, linewidth=0.6)
        ax.minorticks_off()
        for s in ("top", "right"):
            ax.spines[s].set_visible(False)
    axes[0].set_ylabel("round trip, µs (p50; band to p99)")
    handles, labels = axes[0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="outside upper center", ncol=3, frameon=False, fontsize=8)
    for ext in ("pdf", "png"):
        fig.savefig(args.out / f"fig1_roundtrip_vs_payload.{ext}", dpi=200)
    print(f"wrote {args.out}/fig1_roundtrip_vs_payload.(pdf|png)")


if __name__ == "__main__":
    main()
