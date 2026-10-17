"""Paper figure 1: round trip vs payload, M5 vs RTX 3060 Ti (ReBAR on / off), idle GPU, chain/spin.

    python3 tools/analysis/paper_fig1.py [--out docs/paper/fig]

Reads only committed reports: results/m1_report/summary.csv (P-M5 and P-3060 with ReBAR on) and
results/m1_report_rebar-off/summary.csv (P-3060 with ReBAR off). Panel (a): the direct path (s2_direct);
panel (b): the copy path (s1_copy). Same log-log scale in both panels. Solid line = p50; the shaded band
reaches up to p99. The empty round trip is marked at the left of each panel.
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
    ("Apple M5 (unified)", "m1_report", "P-M5", "#2a78d6", "o"),
    ("RTX 3060 Ti, ReBAR on", "m1_report", "P-3060", "#eb6834", "s"),
    ("RTX 3060 Ti, ReBAR off", "m1_report_rebar-off", "P-3060", "#1baf7a", "^"),
]
INK, MUTED, GRID, SURFACE = "#0b0b0b", "#52514e", "#e4e3df", "#fcfcfb"
EMPTY_X = 1024  # where the empty round trip is drawn (left of the 4 KiB grid point)


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

    plt.rcParams.update({"font.size": 9, "axes.edgecolor": MUTED, "axes.labelcolor": INK,
                         "xtick.color": MUTED, "ytick.color": MUTED, "figure.facecolor": SURFACE,
                         "axes.facecolor": SURFACE, "savefig.facecolor": SURFACE})
    fig, axes = plt.subplots(1, 2, figsize=(7.2, 3.1), sharey=True, constrained_layout=True)
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
        if path == "s2_direct":  # the crossover of §5 is between the direct / best paths
            ax.axvspan(2.0 * (1 << 20), 2.6 * (1 << 20), color=MUTED, alpha=0.10, linewidth=0)
        ax.set_title(title, loc="left", fontsize=9, color=INK)
        ax.set_xlabel("payload per direction (bytes)")
        ax.grid(True, which="major", color=GRID, linewidth=0.6)
        ax.minorticks_off()
        for s in ("top", "right"):
            ax.spines[s].set_visible(False)
    axes[0].set_ylabel("round trip, µs (p50; band to p99)")
    axes[0].annotate("idle crossover,\ninterpolated", (2.3 * (1 << 20), 60), ha="center", fontsize=7,
                     color=MUTED)
    handles, labels = axes[0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="outside upper center", ncol=3, frameon=False, fontsize=8)
    for ext in ("pdf", "png"):
        fig.savefig(args.out / f"fig1_roundtrip_vs_payload.{ext}", dpi=200)
    print(f"wrote {args.out}/fig1_roundtrip_vs_payload.(pdf|png)")


if __name__ == "__main__":
    main()
