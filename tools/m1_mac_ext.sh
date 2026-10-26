#!/bin/zsh
# M1 extension series on the Mac (docs/M1_RESCOPE_PROPOSAL.md §4) with the CPU-load conditions of the spin-wait
# decision (STATUS to-do 2, DECISIONS #36):  a = idle only, b = load2 only, b2 = both, alternating run by run.
#   caffeinate -is tools/m1_mac_ext.sh <a|b|b2> <frozen dir with lightbound + shaders/> [runs=3]
# Main's quiet-Mac slot only. Gates (LAB.md): full wake, AC >= 35 W, battery current >= -100 mA. Logs the adapter
# (charger serial redacted), displays, systemstate, load and top processes before and after, and one line per minute
# (systemstate, load, battery) while the series runs. Output: results/m1_mac_ext/<idle|load2>/ (+ run.log, env.log).
setopt NO_BG_NICE
R=/Users/jostarigan/Project/Lightbound; cd $R
MODE=${1:?a|b|b2}; EXE=${2:?frozen dir}/lightbound; RUNS=${3:-3}
case $MODE in
  a)  CONDS=(--cond idle=) ;;
  b)  CONDS=(--cond load2=--micro-cpuload=2) ;;
  b2) CONDS=(--cond idle= --cond load2=--micro-cpuload=2) ;;
  *)  echo "mode must be a, b or b2" >&2; exit 2 ;;
esac
OUT=$R/results/m1_mac_ext; mkdir -p $OUT; LOG=$OUT/run.log; ENV=$OUT/env.log
export PATH="$HOME/Library/Python/3.9/bin:$PATH"
log() { echo "$(date '+%F %T') $*" | tee -a $LOG; }
ss() { pmset -g systemstate | awk '/Capabilities/{sub(/.*are: /,""); c=$0} /Power State/{p=$NF} END{gsub(/ /,"+",c); printf "sys=%s/ps%s", c, p}'; }
ma() { ioreg -rn AppleSmartBattery | awk -F'= ' '/"InstantAmperage"/{v=$2; if (v > 2^63) v -= 2^64; print v}'; }
batt() { echo "ac=$(pmset -g ac | awk -F'= ' '/Wattage/{print $2}' | tr -d ' ') mA=$(ma) $(pmset -g batt | awk '/InternalBattery/{print $3, $4}' | tr -d ';')"; }
env_dump() { { echo "=== $1 $(date '+%F %T')"; pmset -g ac | sed -E 's/^( *Serial Number).*/\1 = <redacted>/'; pmset -g systemstate
  system_profiler SPDisplaysDataType 2>/dev/null | grep -E 'Display Type|Resolution|Online|Connection Type'
  sysctl -n vm.loadavg; top -l 1 -n 8 -o cpu -stats pid,cpu,command | tail -9; } >> $ENV 2>&1; }

[ -x $EXE ] && [ -d ${EXE:h}/shaders ] || { log "no lightbound + shaders/ in ${EXE:h}: abort"; exit 3; }
if [ $MODE != a ]; then $EXE --help 2>&1 | grep -q -- '--micro-cpuload' || { log "$EXE has no --micro-cpuload: abort"; exit 3; }; fi
ss | grep -q Graphics || { log "not in full wake ($(ss)): abort"; exit 4; }
A=$(pmset -g ac | awk -F'= ' '/Wattage/{print $2}' | tr -dc '0-9'); [ "${A:-0}" -ge 35 ] || { log "adapter ${A:-?} W < 35 W: abort"; exit 5; }
[ "$(ma)" -ge -100 ] || { log "battery current $(ma) mA < -100 mA (charge above the limit?): abort"; exit 5; }
env_dump pre
log "start mode=$MODE runs=$RUNS exe=$EXE sha256=$(shasum -a 256 $EXE | cut -c1-12); $(ss); $(batt); load $(sysctl -n vm.loadavg | awk '{print $2}')"
( while sleep 60; do echo "$(date '+%F %T') minute $(ss) $(batt) load $(sysctl -n vm.loadavg | awk '{print $2}')" >> $LOG; done ) &
TICK=$!; trap 'kill $TICK 2>/dev/null' EXIT INT TERM
python3 tools/run_microbench.py --exe $EXE --out $OUT --runs $RUNS $CONDS -- \
    --micro-gpuload=both --micro-paths=empty,s1_copy,s2_direct \
    --micro-payloads=4K,64K,256K,1M,1536K,2M,3M,4M,8M,16M,32M,64M 2>&1 | tee -a $LOG
rc=${pipestatus[1]}
env_dump post
log "end rc=$rc; $(ss); $(batt)"
exit $rc
