#!/usr/bin/env python3
"""Runs the M1 microbenchmark N times as separate processes (protocol: run-to-run variation
across process restarts), with an idle cooldown between runs, then builds the report.

    python tools/run_microbench.py --exe build/lightbound --out results/m1 [--runs 3] [--cooldown 20]
    python tools/run_microbench.py --exe build\\lightbound.exe --out results\\m1 --tag rebar-on
    python tools/run_microbench.py --exe build\\lightbound.exe --out results\\m1 --tag rebar-on --resume

Each run writes <out>/micro_<platform>_<tag>_run<i>.csv (+ .log). Extra arguments after `--`
are passed to lightbound (e.g. `-- --validation=on`, or `-- --micro-gpuload=both` for the hot-GPU
cells of DECISIONS #32). Do not run anything else on the machine while this runs: other GPU/CPU
work perturbs the measurements.

--cond NAME=ARGS (repeatable) measures named conditions in alternation, run 1 of each, then run 2 of
each, ..., so a drift over the series hits every condition alike; each goes to <out>/<NAME>/ with its own
report. Example (DECISIONS #36): --cond idle= --cond load2=--micro-cpuload=2.

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


def run_one(exe: Path, out: Path, i: int, runs: int, tag: str, extra: list[str], resume: bool) -> int | None:
    """Runs (or resumes, or skips) run i into `out`. Returns lightbound's exit code, or None if the run was
    already complete. Exits the script if the process did not finish."""
    tmp = out / f"_running_run{i}.csv"
    tmp_log = out / f"_running_run{i}.log"
    suffix = f"{tag + '_' if tag else ''}run{i}"
    done = sorted(out.glob(f"micro_*_{suffix}.csv"))
    if resume and done and not tmp.exists():
        if tmp_log.exists():  # interrupted between the two renames below
            tmp_log.replace(done[0].with_suffix(".log"))
        print(f"[{i}/{runs}] already complete: {out / done[0].name}", flush=True)
        return None
    cmd = [str(exe), "--mode=microbench", f"--out={tmp}", f"--tag={tag}run{i}"] + extra
    resuming = resume and tmp.exists()
    if resuming:
        cmd.append("--resume")
    print(f"[{i}/{runs}] {' '.join(cmd)}", flush=True)
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
    tmp.replace(out / f"{stem}.csv")
    tmp_log.replace(out / f"{stem}.log")
    print(f"    exit {returncode} after {time.time() - t0:.0f} s -> {out / stem}.csv", flush=True)
    if returncode != 0:
        tail = (out / f"{stem}.log").read_text(encoding="utf-8", errors="replace")[-2000:]
        print(tail, file=sys.stderr)
    return returncode


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--exe", required=True, type=Path)
    ap.add_argument("--out", required=True, type=Path)
    ap.add_argument("--runs", type=int, default=3)
    ap.add_argument("--cooldown", type=float, default=20.0)
    ap.add_argument("--tag", default="")
    ap.add_argument("--resume", action="store_true", help="continue an interrupted series in --out")
    ap.add_argument("--cond", action="append", default=[], metavar="NAME=ARGS",
                    help="a named condition, repeatable: its runs go to <out>/<NAME>/ with ARGS (space-separated "
                         "lightbound flags, may be empty) added; conditions alternate run by run")
    ap.add_argument("extra", nargs=argparse.REMAINDER)
    args = ap.parse_args()
    extra = [a for a in args.extra if a != "--"]

    conds = []  # (output dir, extra args)
    for c in args.cond:
        name, sep, cargs = c.partition("=")
        if not sep or not re.fullmatch(r"[A-Za-z0-9._-]+", name):
            sys.exit(f"--cond {c!r}: expected NAME=ARGS with NAME of letters, digits, . _ -")
        conds.append((args.out / name, extra + cargs.split()))
    if not conds:
        conds = [(args.out, extra)]
    for out, _ in conds:
        out.mkdir(parents=True, exist_ok=True)
        if not args.resume and list(out.glob("_running_run*.csv")):
            sys.exit(f"{out} holds an interrupted series (_running_run*.csv). Add --resume to continue it, "
                     "or delete those files to start over.")

    failures = 0
    for i in range(1, args.runs + 1):
        for k, (out, cextra) in enumerate(conds):
            rc = run_one(args.exe, out, i, args.runs, args.tag, cextra, args.resume)
            if rc is None:
                continue
            failures += rc != 0
            if i < args.runs or k + 1 < len(conds):
                time.sleep(args.cooldown)

    report = Path(__file__).parent / "analysis" / "micro_report.py"
    for out, _ in conds:
        subprocess.run([sys.executable, str(report), str(out), "--out", str(out / "report")])
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
