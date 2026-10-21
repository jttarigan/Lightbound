#!/bin/zsh
# Launch-context A/B: S = launched from this session (BattleStation daemon tree), T = launched from one Terminal.app window.
# v2 (fixed after the failed 05:58 run). Start: LC_END='2026-10-20 HH:MM:SS' caffeinate -is lc2.sh
# A run starts only if it can finish (≈ 40 s) before LC_END. Full-wake gate before the window opens and before every run:
# stops if Graphics is missing from systemstate (dark wake shows only "CPU Network", e.g. 10-20 07:32 after RTBench).
setopt NO_BG_NICE  # zsh would run & jobs at nice 5 (bug in the 05:58 run)
SP=${LC_SP:-/private/tmp/claude-501/-Users-jostarigan-Project-Lightbound/1e18616d-0259-440b-be49-17f58975c322/scratchpad}; R=/Users/jostarigan/Project/Lightbound; L=$SP/lc; LOG=$L/lc.log; mkdir -p $L; cd $R
T_END=$(date -j -f "%Y-%m-%d %H:%M:%S" "${LC_END:?set LC_END=YYYY-mm-dd HH:MM:SS}" +%s)
log() { echo "$(date '+%H:%M:%S') $*" | tee -a $LOG; }
ssparse() { awk '/Capabilities/{sub(/.*are: /,""); c=$0} /Power State/{p=$NF} END{gsub(/ /,"+",c); printf "sys=%s/ps%s", c, p}'; }
ss() { pmset -g systemstate | ssparse; }
fullwake() { ss | grep -q Graphics; }   # pmset failing or a format change also counts as not full wake
fullwake || { log "not in full wake ($(ss)): abort, no window opened"; exit 4; }
BENCH="build/frozen_ext_2d5c1ba/lightbound --mode=microbench --micro-paths=empty,s1_copy,s2_direct --micro-payloads=4K,1M"
TTY=$(osascript -e 'tell application "Terminal" to return tty of (do script "")' 2>>$LOG); sleep 1
WID=$(osascript -e "tell application \"Terminal\"" -e "repeat with w in windows" -e "if (count of tabs of w) > 0 and tty of tab 1 of w is \"$TTY\" then return id of w" -e "end repeat" -e "return -1" -e "end tell" 2>>$LOG)
log "created tty $TTY"
closewin() {   # closes only the window whose tab 1 is on our tty (works even if WID was never captured)
  pkill -f "$L/lc_" 2>/dev/null   # a timed-out bench must not keep the window busy
  [ -z "$TTY" ] && return
  osascript -e "tell application \"Terminal\"" -e "repeat with w in windows" -e "if (count of tabs of w) > 0 and tty of tab 1 of w is \"$TTY\" then do script \"exit\" in tab 1 of w" -e "end repeat" -e "end tell" >/dev/null 2>&1
  sleep 2
  osascript -e "tell application \"Terminal\"" -e "repeat with w in windows" -e "if (count of tabs of w) > 0 and tty of tab 1 of w is \"$TTY\" then close w" -e "end repeat" -e "end tell" >/dev/null 2>&1
  log "cleanup: tty $TTY gone? $([ -e $TTY ] && echo NO || echo yes); windows now: $(osascript -e 'tell application "Terminal" to get id of windows' 2>&1); $(ss)"
}
trap closewin EXIT
log "Terminal window id $WID; $(ss); load $(sysctl -n vm.loadavg | awk '{print $2}')"
[[ "$WID" == <-> && "$WID" != "-1" ]] || { log "no valid window id ($WID): abort"; exit 3; }
psinfo() { taskinfo $1 > $L/ti_${i}_${c}.txt 2>&1; taskinfo --threads $1 >> $L/ti_${i}_${c}.txt 2>&1; ps -o pid=,pri=,ni=,stat= -p $1 2>/dev/null | xargs; p=$(ps -o ppid= -p $1 | tr -d ' '); echo " parents: $(ps -o comm= -p $p | xargs -I{} basename {}) <- $(ps -o comm= -p $(ps -o ppid= -p $p | tr -d ' ') | xargs -I{} basename {})"; }
i=0
for c in S T T S S T T S; do
  [ $(( $(date +%s) + 40 )) -gt $T_END ] && { log "deadline: stop before run $((i+1))"; break; }
  i=$((i+1)); sleep 3
  fullwake || { log "not in full wake before run $i ($(ss)): stop"; break; }
  log "run ${i}_$c start: $(ss); load $(sysctl -n vm.loadavg | awk '{print $2}')"
  if [ $c = S ]; then
    $=BENCH --out=$L/lc_${i}_S.csv > $L/lc_${i}_S.log 2>&1 & BP=$!
    sleep 3; log "  bench $(psinfo $BP)"; wait $BP; log "  exit $?"
  else
    rm -f $L/lc_${i}_T.done
    osascript -e "tell application \"Terminal\" to do script \"cd $R && $BENCH --out=$L/lc_${i}_T.csv > $L/lc_${i}_T.log 2>&1; echo \$? > $L/lc_${i}_T.done\" in tab 1 of window id $WID" > /dev/null
    sleep 3; BP=$(pgrep -n -f "lc_${i}_T.csv"); log "  bench $(psinfo $BP)"
    n=0; while [ ! -f $L/lc_${i}_T.done ] && [ $n -lt 70 ]; do sleep 1; n=$((n+1)); done
    log "  exit $(cat $L/lc_${i}_T.done 2>/dev/null || echo timeout)"
  fi
done
log "runs finished"
