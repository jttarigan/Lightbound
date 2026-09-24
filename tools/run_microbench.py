#!/usr/bin/env python3
"""Runs the M1 microbenchmark N times as separate processes (protocol: run-to-run variation
across process restarts), with an idle cooldown between runs, then builds the report.

    python tools/run_microbench.py --exe build/lightbound --out results/m1 [--runs 3] [--cooldown 20]
    python tools/run_microbench.py --exe build\\lightbound.exe --out results\\m1 --tag rebar-on

Each run writes <out>/micro_<platform>_<tag>_run<i>.csv (+ .log). Extra arguments after `--`
are passed to lightbound (e.g. `-- --validation=on`). Do not run anything else on the machine
while this runs: other GPU/CPU work perturbs the measurements.
"""
from __future__ import annotations

import argparse
import re
import subprocess
import sys
import time
from pathlib import Path


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--exe", required=True, type=Path)
    ap.add_argument("--out", required=True, type=Path)
    ap.add_argument("--runs", type=int, default=3)
    ap.add_argument("--cooldown", type=float, default=20.0)
    ap.add_argument("--tag", default="")
    ap.add_argument("extra", nargs=argparse.REMAINDER)
    args = ap.parse_args()
    extra = [a for a in args.extra if a != "--"]
    args.out.mkdir(parents=True, exist_ok=True)

    failures = 0
    for i in range(1, args.runs + 1):
        tmp = args.out / f"_running_run{i}.csv"
        cmd = [str(args.exe), "--mode=microbench", f"--out={tmp}", f"--tag={args.tag}run{i}"] + extra
        print(f"[{i}/{args.runs}] {' '.join(cmd)}", flush=True)
        t0 = time.time()
        log = subprocess.run(cmd, capture_output=True, text=True)
        platform = "unknown"
        if tmp.exists():
            m = re.search(r"^# sysinfo\.platform: (.+)$", tmp.read_text(errors="replace"), re.MULTILINE)
            if m:
                platform = re.sub(r"[^A-Za-z0-9._-]", "_", m.group(1))
        stem = f"micro_{platform}_{args.tag + '_' if args.tag else ''}run{i}"
        (args.out / f"{stem}.log").write_text(log.stdout + log.stderr)
        if tmp.exists():
            tmp.replace(args.out / f"{stem}.csv")
        print(f"    exit {log.returncode} after {time.time() - t0:.0f} s -> {stem}.csv", flush=True)
        if log.returncode != 0:
            failures += 1
            print(log.stderr[-2000:], file=sys.stderr)
        if i < args.runs:
            time.sleep(args.cooldown)

    report = Path(__file__).parent / "analysis" / "micro_report.py"
    subprocess.run([sys.executable, str(report), str(args.out), "--out", str(args.out / "report")])
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
