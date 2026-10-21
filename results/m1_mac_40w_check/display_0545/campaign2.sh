#!/bin/zsh
# Part 1 v2: display-sleep A/B with the power state (pmset -g systemstate) logged before/after every run.
# Run as: caffeinate -is campaign2.sh. Slot 05:45-06:05 WIB 2026-10-20 (hard end).
SP=/private/tmp/claude-501/-Users-jostarigan-Project-Lightbound/1e18616d-0259-440b-be49-17f58975c322/scratchpad; R=/Users/jostarigan/Project/Lightbound; cd $R
LOG=$SP/campaign.log; D=$SP/disp; mkdir -p $D
ep() { date -j -f "%Y-%m-%d %H:%M:%S" "2026-10-20 $1" +%s; }
T_START=$(ep 05:45:00); T_GATE=$(ep 05:52:00); T_END=$(ep 06:04:30); T_LOGDUMP=$(ep 06:02:30)
log() { echo "$(date '+%H:%M:%S') $*" | tee -a $LOG; }
ss() { pmset -g systemstate | awk '/Capabilities/{sub(/.*are: /,""); c=$0} /Power State/{p=$NF} END{gsub(/ /,"+",c); printf "sys=%s/ps%s", c, p}'; }
SAMPLER=""; DPID=""
cleanup() { [ -n "$DPID" ] && kill $DPID 2>/dev/null; [ -n "$SAMPLER" ] && kill $SAMPLER 2>/dev/null; log "campaign end (display assertion released; displays back to normal idle sleep): $($SP/dispstate) $(ss)"; }
trap cleanup EXIT
log "v2 armed; waiting for 05:45"
now=$(date +%s); [ $now -lt $T_START ] && sleep $((T_START - now))
ok=0
while [ $(date +%s) -lt $T_GATE ]; do
  l=$(sysctl -n vm.loadavg | awk '{print $2}'); g=$(ioreg -r -d 1 -c IOAccelerator | grep -oE '"Device Utilization %"=[0-9]+' | head -1 | sed 's/.*=//')
  if awk -v l=$l -v g=$g 'BEGIN{exit !(l < 3.0 && g < 10)}'; then ok=$((ok+1)); else ok=0; log "gate: busy (load $l, gpu $g %)"; fi
  [ $ok -ge 3 ] && break; sleep 5
done
[ $ok -ge 3 ] || { log "gate not met by 05:52: SKIPPED"; exit 2; }
$SP/sampler.sh $SP/power_log2.csv & SAMPLER=$!
log "gate passed; $(pmset -g ac | grep Wattage | xargs); $($SP/dispstate) $(ss)"
awake()  { caffeinate -u -t 2; caffeinate -d & DPID=$!; sleep 15; }
asleep() { [ -n "$DPID" ] && kill $DPID 2>/dev/null; DPID=""; pmset displaysleepnow; sleep 15; }
i=0
for st in awake asleep asleep awake awake asleep asleep awake awake asleep asleep awake; do
  [ $(( $(date +%s) + 45 )) -gt $T_END ] && { log "deadline: stopping before run $((i+1))"; break; }
  i=$((i+1)); $st
  log "disp_${i}_$st start: $($SP/dispstate) $(ss); load $(sysctl -n vm.loadavg | awk '{print $2}')"
  build/frozen_ext_2d5c1ba/lightbound --mode=microbench --out=$D/disp_${i}_$st.csv --micro-paths=empty,s1_copy,s2_direct --micro-payloads=4K,1M > $D/disp_${i}_$st.log 2>&1
  log "  exit $? ; after: $($SP/dispstate) $(ss)"
done
# Power-log extract for 09-24, 10-17 and 10-19/20 (sleep/wake history), only if the slot still has time
if [ $(date +%s) -lt $T_LOGDUMP ]; then
  pmset -g log | grep -E "^2026-(09-24|10-17|10-19|10-20)" | grep -E "Sleep|Wake|DarkWake|FullWake" > $SP/pmset_wake_history.txt
  log "pmset wake history: $(wc -l < $SP/pmset_wake_history.txt) lines"
fi
