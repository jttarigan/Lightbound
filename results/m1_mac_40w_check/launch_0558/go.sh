#!/bin/zsh
# Launcher for the 10-21 launch-context retest. Gate (load / GPU may settle: retry ≤ 3 min if only those fail),
# then lc3.sh with an end time 7.5 min after start, then a post-run state dump (no gating).
SP=/private/tmp/claude-501/-Users-jostarigan-Project-Lightbound/3a97b98e-4901-485d-9218-1b6cefb1fdee/scratchpad; L=$SP/lc; mkdir -p $L; P=$L/precheck_$(date +%H%M).log
t0=$(date +%s)
until $SP/pre.sh $P; do
  onlyload=$(tail -n 40 $P | grep "GATE FAIL" | grep -vcE "load|GPU util")
  if [ "$onlyload" != 0 ] || [ $(( $(date +%s) - t0 )) -gt 180 ]; then echo "$(date +%H:%M:%S) gate failed: not starting" | tee -a $P; exit 5; fi
  sleep 20
done
END=$(date -r $(( $(date +%s) + 450 )) '+%Y-%m-%d %H:%M:%S')
echo "$(date +%H:%M:%S) start lc3.sh, LC_END $END" | tee -a $P
cd /Users/jostarigan/Project/Lightbound
LC_SP=$SP LC_END="$END" caffeinate -dis results/m1_mac_40w_check/launch_0558/lc3.sh; rc=$?
echo "$(date +%H:%M:%S) lc3.sh exit $rc" | tee -a $P
$SP/pre.sh $L/postcheck.log > /dev/null
exit $rc
