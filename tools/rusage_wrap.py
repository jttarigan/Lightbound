#!/usr/bin/env python3
"""Runs a command and records where macOS scheduled it: the child's exit rusage (proc_pid_rusage,
RUSAGE_INFO_V6), read before the child is reaped. No root needed (taskinfo needs root).

    python3 -I tools/rusage_wrap.py --out run.rusage.json -- build/lightbound --mode=microbench ...

Written for the Mac launch-context A/B (STATUS 10-20/10-21): spin-wait cells there run at an
E-core-like rate, and this shows whether the bench really ran on the P-cores, at what clock and
QoS. The wrapper sleeps in waitid() while the child runs, so it adds no load. The exit code is
the child's. Covers only the child process itself, not its own children (lightbound has none).
Apple silicon only: other platforms do not have the P-core fields.
"""
from __future__ import annotations

import argparse
import ctypes
import json
import subprocess
import sys

P_PID, WEXITED, WNOWAIT = 1, 0x04, 0x20  # <sys/wait.h> on macOS
RUSAGE_INFO_V6 = 6

V6_FIELDS = (
    "user_time system_time pkg_idle_wkups interrupt_wkups pageins wired_size resident_size "
    "phys_footprint proc_start_abstime proc_exit_abstime child_user_time child_system_time "
    "child_pkg_idle_wkups child_interrupt_wkups child_pageins child_elapsed_abstime "
    "diskio_bytesread diskio_byteswritten cpu_time_qos_default cpu_time_qos_maintenance "
    "cpu_time_qos_background cpu_time_qos_utility cpu_time_qos_legacy cpu_time_qos_user_initiated "
    "cpu_time_qos_user_interactive billed_system_time serviced_system_time logical_writes "
    "lifetime_max_phys_footprint instructions cycles billed_energy serviced_energy "
    "interval_max_phys_footprint runnable_time flags user_ptime system_ptime pinstructions pcycles "
    "energy_nj penergy_nj secure_time_in_system secure_ptime_in_system neural_footprint "
    "lifetime_max_neural_footprint interval_max_neural_footprint conclave_footprint "
    "page_wait_time_mach page_cache_hits").split()


class RusageInfoV6(ctypes.Structure):  # struct rusage_info_v6, <sys/resource.h> (macOS 26 SDK)
    _fields_ = ([("uuid", ctypes.c_uint8 * 16)] + [(f, ctypes.c_uint64) for f in V6_FIELDS]
                + [("reserved", ctypes.c_uint64 * 6)])


class MachTimebase(ctypes.Structure):
    _fields_ = [("numer", ctypes.c_uint32), ("denom", ctypes.c_uint32)]


def summarize(r: RusageInfoV6, ns_per_tick: float) -> dict:
    """Times in the struct are mach ticks (24 MHz on Apple silicon); cycles and instructions are counts."""
    cpu_ns = (r.user_time + r.system_time) * ns_per_tick
    p_ns = (r.user_ptime + r.system_ptime) * ns_per_tick
    e_ns = cpu_ns - p_ns
    qos = {f[len("cpu_time_qos_"):]: getattr(r, f) for f in V6_FIELDS if f.startswith("cpu_time_qos_")}
    qos_total = sum(qos.values())

    def ghz(cycles: float, ns: float) -> float | None:
        return round(cycles / ns, 3) if ns > 0 else None

    return {
        "wall_s": round((r.proc_exit_abstime - r.proc_start_abstime) * ns_per_tick / 1e9, 3),
        "cpu_s": round(cpu_ns / 1e9, 3),
        "pcore_share": round(p_ns / cpu_ns, 3) if cpu_ns > 0 else None,
        "ghz_all": ghz(r.cycles, cpu_ns),
        "ghz_pcore": ghz(r.pcycles, p_ns),
        "ghz_ecore": ghz(r.cycles - r.pcycles, e_ns),
        "ipc": round(r.instructions / r.cycles, 3) if r.cycles else None,
        "runnable_s": round(r.runnable_time * ns_per_tick / 1e9, 3),
        "qos_share": {k: round(v / qos_total, 3) for k, v in qos.items() if v} if qos_total else {},
        "interrupt_wkups": r.interrupt_wkups,
        "pkg_idle_wkups": r.pkg_idle_wkups,
        "energy_j": round(r.energy_nj / 1e9, 3),
        "raw": {f: getattr(r, f) for f in V6_FIELDS},
        "ns_per_tick": ns_per_tick,
    }


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--out", required=True, help="JSON file for the child's exit rusage")
    ap.add_argument("cmd", nargs=argparse.REMAINDER, help="-- command [args...]")
    a = ap.parse_args()
    cmd = a.cmd[1:] if a.cmd[:1] == ["--"] else a.cmd
    if not cmd:
        ap.error("no command given")

    libc = ctypes.CDLL(None, use_errno=True)
    libproc = ctypes.CDLL("/usr/lib/libproc.dylib", use_errno=True)
    tb = MachTimebase()
    libc.mach_timebase_info(ctypes.byref(tb))
    ns_per_tick = tb.numer / tb.denom

    child = subprocess.Popen(cmd)
    siginfo = ctypes.create_string_buffer(256)  # siginfo_t is 104 bytes on macOS
    while libc.waitid(P_PID, child.pid, siginfo, WEXITED | WNOWAIT) != 0:
        if ctypes.get_errno() != 4:  # EINTR: retry
            print(f"rusage_wrap: waitid failed, errno {ctypes.get_errno()}", file=sys.stderr)
            return child.wait()
    r = RusageInfoV6()
    ok = libproc.proc_pid_rusage(child.pid, RUSAGE_INFO_V6, ctypes.byref(r)) == 0
    rc = child.wait()  # reaps the zombie
    rec = {"cmd": cmd, "pid": child.pid, "exit": rc}
    rec.update(summarize(r, ns_per_tick) if ok else {"error": f"proc_pid_rusage errno {ctypes.get_errno()}"})
    with open(a.out, "w") as f:
        json.dump(rec, f, indent=1)
    return rc


if __name__ == "__main__":
    sys.exit(main())
