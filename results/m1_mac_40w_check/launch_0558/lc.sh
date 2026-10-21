#!/bin/zsh
# Launch-context A/B: S = launched from this session (BattleStation daemon tree), T = launched from one Terminal.app window.
# Hard end 06:09 WIB 2026-10-20: a run starts only if it can finish by 06:08:20.
SP=/private/tmp/claude-501/-Users-jostarigan-Project-Lightbound/1e18616d-0259-440b-be49-17f58975c322/scratchpad; R=/Users/jostarigan/Project/Lightbound; L=$SP/lc; LOG=$L/lc.log; cd $R
T_END=$(date -j -f "%Y-%m-%d %H:%M:%S" "2026-10-20 06:08:20" +%s)
log() { echo "$(date '+%H:%M:%S') $*" | tee -a $LOG; }
ss() { pmset -g systemstate | awk '/Capabilities/{sub(/.*are: /,""); c=$0} /Power State/{p=$NF} END{gsub(/ /,"+",c); printf "sys=%s/ps%s", c, p}'; }
BENCH="build/frozen_ext_2d5c1ba/lightbound --mode=microbench --micro-paths=empty,s1_copy,s2_direct --micro-payloads=4K,1M"
WID=$(osascript -e 'tell application "Terminal"' -e 'set t to do script ""' -e 'delay 1' -e 'repeat with w in windows' -e 'if (tabs of w) contains t then return id of w' -e 'end repeat' -e 'return -1' -e 'end tell')
log "Terminal window id $WID; $(ss); load $(sysctl -n vm.loadavg | awk '{print $2}')"
[ "$WID" = "-1" ] && { log "no window id: abort"; exit 3; }
psinfo() { ps -o pid=,pri=,ni=,stat= -p $1 2>/dev/null | xargs; p=$(ps -o ppid= -p $1 | tr -d ' '); echo " parents: $(ps -o comm= -p $p | xargs -I{} basename {}) <- $(ps -o comm= -p $(ps -o ppid= -p $p | tr -d ' ') | xargs -I{} basename {})"; }
i=0
for c in S T T S S T T S; do
  [ $(( $(date +%s) + 40 )) -gt $T_END ] && { log "deadline: stop before run $((i+1))"; break; }
  i=$((i+1)); sleep 3; log "run ${i}_$c start: $(ss); load $(sysctl -n vm.loadavg | awk '{print $2}')"
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
osascript -e "tell application \"Terminal\" to do script \"exit\" in tab 1 of window id $WID" > /dev/null 2>&1; sleep 2
osascript -e "tell application \"Terminal\" to if (exists window id $WID) then close window id $WID" > /dev/null 2>&1
log "end: window $WID exists? $(osascript -e "tell application \"Terminal\" to exists window id $WID" 2>&1); $(ss)"
