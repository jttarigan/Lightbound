#!/bin/zsh
# Spin-wait shift, clock check (STATUS to-do 2c, DECISIONS #36): root powermetrics (cpu_power + gpu_power, 250 ms) over
# 4 microbench runs I L I L (I = bench alone, L = bench + 2x `yes >/dev/null`). powermetrics comes from the user's root
# waiter (Main's shared sudo line in a Terminal window: waits for /tmp/labpm_go/lightbound, records 720 x 250 ms = 3 min
# into /var/tmp/labpm/lightbound.plist, root-owned 0644: copied, never chowned or removed by this script).
# Start (Main's quiet slot only):  caffeinate -is results/m1_mac_40w_check/pm_check.sh
# Gates: waiter running as root, full wake, AC >= 35 W. Run start/end are logged as epoch ms to align with the samples.
setopt NO_BG_NICE
R=/Users/jostarigan/Project/Lightbound; TRIG=/tmp/labpm_go/lightbound; PL=/var/tmp/labpm/lightbound.plist; cd $R
L=$R/results/m1_mac_40w_check/pm_$(date +%m%d_%H%M); LOG=$L/pm.log; mkdir -p $L
ms() { /usr/bin/python3 -c 'import time; print(int(time.time()*1000))'; }
log() { echo "$(date '+%H:%M:%S') $*" | tee -a $LOG; }
ss() { pmset -g systemstate | awk '/Capabilities/{sub(/.*are: /,""); c=$0} /Power State/{p=$NF} END{gsub(/ /,"+",c); printf "sys=%s/ps%s", c, p}'; }
fullwake() { ss | grep -q Graphics; }
batt() { echo "ac=$(pmset -g ac | awk -F'= ' '/Wattage/{print $2}' | tr -d ' ') mA=$(ioreg -rn AppleSmartBattery | awk -F'= ' '/"InstantAmperage"/{v=$2; if (v > 2^63) v -= 2^64; print v}') $(pmset -g batt | awk '/InternalBattery/{print $3, $4}' | tr -d ';')"; }
env_dump() { { echo "=== $1 $(date '+%F %T')"; pmset -g ac | sed -E 's/^( *Serial Number).*/\1 = <redacted>/'; pmset -g systemstate
  system_profiler SPDisplaysDataType 2>/dev/null | grep -E 'Display Type|Resolution|Online'
  sysctl -n vm.loadavg; top -l 1 -n 8 -o cpu -stats pid,cpu,command | tail -9; } >> $L/env.log 2>&1; }
YES=()
stop_load() { [ ${#YES} -gt 0 ] && kill $YES 2>/dev/null; YES=(); }
trap 'stop_load' EXIT INT TERM

pgrep -U 0 -f labpm_go > /dev/null || { log "root waiter not running (pgrep -U 0 -f labpm_go): abort"; exit 6; }
pgrep -x powermetrics > /dev/null && { log "powermetrics already running (someone else's capture): abort"; exit 6; }
M0=$(stat -f %m $PL 2>/dev/null || echo 0)
fullwake || { log "not in full wake ($(ss)): abort"; exit 4; }
A=$(pmset -g ac | awk -F'= ' '/Wattage/{print $2}' | tr -dc '0-9'); [ "${A:-0}" -ge 35 ] || { log "adapter ${A:-?} W < 35 W: abort"; exit 5; }
[ -x build/lightbound ] || { log "build/lightbound missing: abort"; exit 3; }
env_dump pre
WRAP="/usr/bin/python3 -I tools/rusage_wrap.py"
BENCH="build/lightbound --mode=microbench --micro-paths=empty,s1_copy,s2_direct --micro-payloads=4K,1M"
log "start: $(ss); $(batt); load $(sysctl -n vm.loadavg | awk '{print $2}'); binary $(git rev-parse --short HEAD)$(git diff --quiet -- src shaders || echo -dirty)"
touch $TRIG || { log "cannot touch $TRIG: abort"; exit 6; }; log "trigger at $(ms)"; sleep 3
pgrep -x powermetrics > /dev/null && log "powermetrics running" || log "WARNING: powermetrics not seen"
i=0
for c in I L I L; do
  i=$((i+1)); sleep 3
  fullwake || { log "not in full wake before run $i ($(ss)): stop"; break; }
  if [ $c = L ]; then yes > /dev/null & YES+=($!); yes > /dev/null & YES+=($!); sleep 2; fi
  log "run ${i}_$c start $(ms): $(ss); $(batt); load $(sysctl -n vm.loadavg | awk '{print $2}')"
  $=WRAP --out $L/ru_${i}_$c.json -- $=BENCH --out=$L/pm_${i}_$c.csv > $L/pm_${i}_$c.log 2>&1; rc=$?
  log "run ${i}_$c end $(ms): exit $rc"
  stop_load
done
log "runs finished; waiting for powermetrics to end"
for t in {1..60}; do pgrep -x powermetrics > /dev/null || break; sleep 2; done
M1=$(stat -f %m $PL 2>/dev/null || echo 0)
if [ $M1 -gt $M0 ] && cp $PL $L/pm.plist; then log "plist copied ($(wc -c < $L/pm.plist) bytes, mtime $M1)"; else log "WARNING: no new plist (mtime $M0 -> $M1)"; fi
env_dump post
log "done; $(batt)"
