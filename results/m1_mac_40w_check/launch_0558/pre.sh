#!/bin/zsh
# Pre-start gate for the launch-context retest (10-21). Read-only. Exit 0 = all gates pass.
L=${1:?log file}
{
echo "== precheck $(date '+%Y-%m-%d %H:%M:%S %Z') (UTC $(date -u '+%H:%M:%S'))"
ac=$(pmset -g ac); echo "$ac" | grep -v "Serial String" | sed "s/^/ac: /"
w=$(echo "$ac" | awk -F'= ' '/Wattage/{gsub(/W/,"",$2); print $2+0}')
amp=$(ioreg -rn AppleSmartBattery | awk -F'= ' '/"InstantAmperage"/{print $2; exit}')
ma=$(/usr/bin/python3 -c "v=int('${amp:-0}'); print(v-2**64 if v>=2**63 else v)")
cap=$(ioreg -rn AppleSmartBattery | awk -F'= ' '/"CurrentCapacity"/{print $2; exit}')
echo "battery: ${cap} %  InstantAmperage ${ma} mA"
ss=$(pmset -g systemstate); echo "$ss" | sed 's/^/systemstate: /'
pmset -g therm | sed 's/^/therm: /'
echo "lowpowermode: $(pmset -g | awk '/lowpowermode/{print $2}')"
la=$(sysctl -n vm.loadavg); echo "loadavg: $la"
gu=$(ioreg -r -d 1 -w 0 -c IOAccelerator | grep -o '"Device Utilization %"=[0-9]*' | head -1 | cut -d= -f2); echo "gpu util: ${gu} %"
echo "top cpu:"; ps -Ao pcpu,pid,comm -r | head -9 | sed 's/^/  /'
echo "displays:"; system_profiler SPDisplaysDataType 2>/dev/null | grep -E "^ {8}[A-Za-z0-9].*:$|Resolution|Refresh|UI Looks like|Online|Main Display|Mirror" | sed 's/^/  /'
l1=$(echo $la | awk '{print $2}')
ok=1
[ "${w:-0}" -ge 35 ] || { echo "GATE FAIL: adapter ${w} W < 35"; ok=0; }
[ "$ma" -ge -100 ] || { echo "GATE FAIL: battery current $ma mA < -100"; ok=0; }
echo "$ss" | grep -q Graphics || { echo "GATE FAIL: not in full wake"; ok=0; }
/usr/bin/python3 -c "import sys; sys.exit(0 if float('$l1') < 3 else 1)" || { echo "GATE FAIL: 1-min load $l1 >= 3"; ok=0; }
[ "${gu:-100}" -le 5 ] || { echo "GATE FAIL: GPU util ${gu} % > 5"; ok=0; }
[ $ok = 1 ] && echo "GATES: PASS" || echo "GATES: FAIL"
} 2>&1 | tee -a $L
grep -q "GATES: PASS" <(tail -1 $L)
