#!/bin/zsh
out=$1; echo "time_wib,adapter_w,batt_pct,batt_ma,batt_c,load1,gpu_util,therm,displays,top3" > $out
while true; do
  w=$(pmset -g ac | awk '/Wattage/{gsub("W","",$3); print $3}')
  b=$(ioreg -rn AppleSmartBattery)
  pct=$(echo "$b" | awk -F'= ' '/"CurrentCapacity"/{print $2; exit}')
  ma=$(echo "$b" | awk -F'= ' '/"InstantAmperage"/{v=$2; if (v > 9223372036854775807) v = v - 18446744073709551616; printf "%d", v; exit}')
  tc=$(echo "$b" | awk -F'= ' '/"Temperature"/{printf "%.1f", $2/100; exit}')
  ld=$(sysctl -n vm.loadavg | awk '{print $2}')
  gu=$(ioreg -r -d 1 -c IOAccelerator | grep -oE '"Device Utilization %"=[0-9]+' | head -1 | sed 's/.*=//')
  th=$(pmset -g therm 2>&1 | grep -c -v "No .* has been recorded")
  ds=$(/private/tmp/claude-501/-Users-jostarigan-Project-Lightbound/1e18616d-0259-440b-be49-17f58975c322/scratchpad/dispstate)
  tp=$(ps -Ao pcpu,comm -r | sed 1d | head -3 | awk '{n=$NF; sub(".*/","",n); printf "%s %s;", $1, n}')
  echo "$(date '+%H:%M:%S'),$w,$pct,$ma,$tc,$ld,$gu,$th,$ds,$tp" >> $out
  sleep 60
done
