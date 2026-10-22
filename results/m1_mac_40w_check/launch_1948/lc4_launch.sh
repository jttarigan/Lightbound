#!/bin/zsh
# Lightbound QoS retest launcher (session 18, 2026-10-22): waits for the GO time, logs the environment, runs lc4_qos.sh.
SP=/private/tmp/claude-501/-Users-jostarigan-Project-Lightbound/00ac5adf-6adf-4649-93aa-0b2e40319811/scratchpad
L=$SP/lc4; mkdir -p $L; E=$L/env.log
T0=$(date -j -f "%Y-%m-%d %H:%M:%S" "2026-10-22 19:48:00" +%s)
echo "launcher armed $(date '+%F %T') for $(date -r $T0 '+%F %T') (epoch $T0), deadline 19:56:00" | tee -a $E
while [ $(( $(date +%s) + 25 )) -lt $T0 ]; do sleep 1; done
{ echo "=== pre $(date '+%F %T')"; pmset -g ac | sed -E 's/^( *Serial Number).*/\1 = <redacted>/'; pmset -g systemstate
  echo "--- displays"; system_profiler SPDisplaysDataType 2>/dev/null | grep -E 'Display Type|Resolution|Connection Type|Main Display|Online' 
  echo "--- HID"; ioreg -rc IOHIDDevice 2>/dev/null | grep -E '"(Product|Transport)" =' | sort | uniq -c
  echo "--- USB"; system_profiler SPUSBDataType 2>/dev/null | grep -E '^\s{6,}[^:]+:$' | sed 's/:$//' | sort | uniq -c
  echo "--- load/top"; sysctl -n vm.loadavg; top -l 1 -n 8 -o cpu -stats pid,cpu,command | tail -9
  echo "--- caffeinate"; pmset -g assertions | grep -E 'caffeinate|PreventUserIdle' | head -6; } >> $E 2>&1
while [ $(date +%s) -lt $T0 ]; do sleep 1; done
echo "=== GO $(date '+%F %T.%N')" >> $E
LC_SP=$SP LC_END='2026-10-22 19:56:00' /Users/jostarigan/Project/Lightbound/results/m1_mac_40w_check/launch_0558/lc4_qos.sh; rc=$?
{ echo "=== post $(date '+%F %T') rc=$rc"; pmset -g systemstate | grep -E 'Capabilities|Power State'; sysctl -n vm.loadavg
  system_profiler SPDisplaysDataType 2>/dev/null | grep -E 'Display Type|Resolution' ; } >> $E 2>&1
exit $rc
