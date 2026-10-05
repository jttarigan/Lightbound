#!/usr/bin/env python3
"""Runs the M1 microbenchmark N times as separate processes (protocol: run-to-run variation
across process restarts), with an idle cooldown between runs, then builds the report.

    python tools/run_microbench.py --exe build/lightbound --out results/m1 [--runs 3] [--cooldown 20]
    python tools/run_microbench.py --exe build\\lightbound.exe --out results\\m1 --tag rebar-on
    python tools/run_microbench.py --exe build\\lightbound.exe --out results\\m1 --tag rebar-on --resume

Each run writes <out>/micro_<platform>_<tag>_run<i>.csv (+ .log). Extra arguments after `--`
are passed to lightbound (e.g. `-- --validation=on`). Do not run anything else on the machine
while this runs: other GPU/CPU work perturbs the measurements.

A series that was interrupted (shutdown, crash, killed process) leaves <out>/_running_run<i>.csv
behind. Start the same command again with --resume to continue it: finished runs are kept, and
the interrupted run goes on from its last complete cell (lightbound --resume, DECISIONS #28).
"""
from __future__ import annotations

import argparse
import re
import subprocess
import sys
import time
from pathlib import Path


def finished(csv: Path) -> bool:
    """True once lightbound wrote the end-of-run footer (it ran every cell)."""
    if not csv.exists():
        return False
    with csv.open("rb") as fh:
        fh.seek(max(0, csv.stat().st_size - 4096))
        return b"\n# failures: " in fh.read()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--exe", required=True, type=Path)
    ap.add_argument("--out", required=True, type=Path)
    ap.add_argument("--runs", type=int, default=3)
    ap.add_argument("--cooldown", type=float, default=20.0)
    ap.add_argument("--tag", default="")
    ap.add_argument("--resume", action="store_true", help="continue an interrupted series in --out")
    ap.add_argument("extra", nargs=argparse.REMAINDER)
    args = ap.parse_args()
    extra = [a for a in args.extra if a != "--"]
    args.out.mkdir(parents=True, exist_ok=True)

    if not args.resume and list(args.out.glob("_running_run*.csv")):
        sys.exit(f"{args.out} holds an interrupted series (_running_run*.csv). Add --resume to continue it, "
                 "or delete those files to start over.")

    failures = 0
    for i in range(1, args.runs + 1):
        tmp = args.out / f"_running_run{i}.csv"
        tmp_log = args.out / f"_running_run{i}.log"
        suffix = f"{args.tag + '_' if args.tag else ''}run{i}"
        done = sorted(args.out.glob(f"micro_*_{suffix}.csv"))
        if args.resume and done and not tmp.exists():
            if tmp_log.exists():  # interrupted between the two renames below
                tmp_log.replace(done[0].with_suffix(".log"))
            print(f"[{i}/{args.runs}] already complete: {done[0].name}", flush=True)
            continue
        cmd = [str(args.exe), "--mode=microbench", f"--out={tmp}", f"--tag={args.tag}run{i}"] + extra
        resuming = args.resume and tmp.exists()
        if resuming:
            cmd.append("--resume")
        print(f"[{i}/{args.runs}] {' '.join(cmd)}", flush=True)
        t0 = time.time()
        # The log goes straight to a file so that it survives an interruption.
        with tmp_log.open("ab" if resuming else "wb") as log:
            if resuming:
                log.write(f"==== resumed {time.strftime('%Y-%m-%d %H:%M:%S')} ====\n".encode())
                log.flush()
            returncode = subprocess.run(cmd, stdout=log, stderr=subprocess.STDOUT).returncode
        if not finished(tmp):
            print(f"    exit {returncode} after {time.time() - t0:.0f} s: run {i} did not finish. "
                  "Start the same command again with --resume to continue it.", flush=True)
            sys.exit(3)
        platform = "unknown"
        m = re.search(r"^# sysinfo\.platform: (.+)$", tmp.read_text(encoding="utf-8", errors="replace"), re.MULTILINE)
        if m:
            platform = re.sub(r"[^A-Za-z0-9._-]", "_", m.group(1).strip())
        stem = f"micro_{platform}_{suffix}"
        tmp.replace(args.out / f"{stem}.csv")
        tmp_log.replace(args.out / f"{stem}.log")
        print(f"    exit {returncode} after {time.time() - t0:.0f} s -> {stem}.csv", flush=True)
        if returncode != 0:
            failures += 1
            tail = (args.out / f"{stem}.log").read_text(encoding="utf-8", errors="replace")[-2000:]
            print(tail, file=sys.stderr)
        if i < args.runs:
            time.sleep(args.cooldown)

    report = Path(__file__).parent / "analysis" / "micro_report.py"
    subprocess.run([sys.executable, str(report), str(args.out), "--out", str(args.out / "report")])
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
