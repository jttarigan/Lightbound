#!/bin/zsh
# Bench-thread scheduling A/B for the M5 spin-wait shift (DECISIONS #36): D = --micro-qos=default, H = --micro-qos=high
# (QoS user-interactive + base priority 47 by THREAD_PRECEDENCE_POLICY). All runs are launched from this session: the
# launch context was ruled out on 10-21 (lc3). Needs a binary with the flag: build/lightbound (NOT the frozen ext binary,
# 2d5c1ba predates it). Each run is wrapped in tools/rusage_wrap.py (P-core share, GHz -> ru_<i>_<c>.json), and the CSV
# header's qos / thread_priority lines are logged, so the log shows whether the request took effect.
# Start:  LC_SP=<scratchpad dir> LC_END='YYYY-mm-dd HH:MM:SS' caffeinate -is results/m1_mac_40w_check/launch_0558/lc4_qos.sh
# Gates: full wake before the first run and before every run (dark wake shows no "Graphics" in systemstate); AC adapter
# ≥ 35 W (pmset -g ac; the battery current is logged, not gated: check it is ≈ 0 mA); a run starts only if it can finish
# (≈ 40 s) before LC_END. Compare: tools/analysis/micro_report.py or ab_compare.py on lc_<i>_<c>.csv against 09-24 / 10-21.
setopt NO_BG_NICE  # zsh would run & jobs at nice 5 on top of the daemon tree's nice 5
SP=${LC_SP:?set LC_SP=<scratchpad dir>}; R=/Users/jostarigan/Project/Lightbound; L=$SP/lc4; LOG=$L/lc4.log; mkdir -p $L; cd $R
T_END=$(date -j -f "%Y-%m-%d %H:%M:%S" "${LC_END:?set LC_END=YYYY-mm-dd HH:MM:SS}" +%s)
log() { echo "$(date '+%H:%M:%S') $*" | tee -a $LOG; }
ssparse() { awk '/Capabilities/{sub(/.*are: /,""); c=$0} /Power State/{p=$NF} END{gsub(/ /,"+",c); printf "sys=%s/ps%s", c, p}'; }
ss() { pmset -g systemstate | ssparse; }
fullwake() { ss | grep -q Graphics; }
batt() { echo "ac=$(pmset -g ac | awk -F'= ' '/Wattage/{print $2}' | tr -d ' ') mA=$(ioreg -rn AppleSmartBattery | awk -F'= ' '/"InstantAmperage"/{v=$2; if (v > 2^63) v -= 2^64; print v}') $(pmset -g batt | awk '/InternalBattery/{print $3, $4}' | tr -d ';')"; }
fullwake || { log "not in full wake ($(ss)): abort"; exit 4; }
W=$(pmset -g ac | awk -F'= ' '/Wattage/{print $2}' | tr -dc '0-9'); [ "${W:-0}" -ge 35 ] || { log "adapter ${W:-?} W < 35 W ($(batt)): abort"; exit 5; }
[ -x build/lightbound ] || { log "build/lightbound missing: build first"; exit 3; }
build/lightbound --help 2>&1 | grep -q -- '--micro-qos' || { log "build/lightbound has no --micro-qos: rebuild"; exit 3; }
WRAP="/usr/bin/python3 -I tools/rusage_wrap.py"
BENCH="build/lightbound --mode=microbench --micro-paths=empty,s1_copy,s2_direct --micro-payloads=4K,1M"
rusum() { /usr/bin/python3 -I -c 'import json,sys; d=json.load(open(sys.argv[1])); print("pcore %s GHz %s/P %s/E %s cpu %ss qos %s" % (d.get("pcore_share"), d.get("ghz_all"), d.get("ghz_pcore"), d.get("ghz_ecore"), d.get("cpu_s"), d.get("qos_share")))' $1 2>&1 | head -1; }
hdr() { grep -E '^# (qos|qos\.note|sysinfo\.thread_priority|sysinfo\.process_nice):' $1 | sed 's/^# //' | tr '\n' ';'; }
log "start: $(ss); $(batt); load $(sysctl -n vm.loadavg | awk '{print $2}'); binary $(git rev-parse --short HEAD)$(git diff --quiet || echo -dirty)"
i=0
for c in D H H D D H H D; do
  [ $(( $(date +%s) + 40 )) -gt $T_END ] && { log "deadline: stop before run $((i+1))"; break; }
  i=$((i+1)); sleep 3
  fullwake || { log "not in full wake before run $i ($(ss)): stop"; break; }
  Q=default; [ $c = H ] && Q=high
  log "run ${i}_$c start: $(ss); $(batt); load $(sysctl -n vm.loadavg | awk '{print $2}')"
  $=WRAP --out $L/ru_${i}_$c.json -- $=BENCH --micro-qos=$Q --out=$L/lc_${i}_$c.csv > $L/lc_${i}_$c.log 2>&1; rc=$?
  log "  exit $rc; $(rusum $L/ru_${i}_$c.json)"
  log "  header: $(hdr $L/lc_${i}_$c.csv)"
done
log "runs finished; $(batt)"
