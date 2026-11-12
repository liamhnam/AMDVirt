#!/bin/bash
# Runs a UTM VM for a while and watches how responsive macOS stays; kills the VM's QEMU process before the host stalls.
# Responsiveness = how long `sleep 0.2` really takes (scheduler latency), sampled every second.
# Usage: tools/safe-vm-test.sh <utm-vm-name> [seconds=120] [kill-threshold-s=2.0] [press-keys-for-s=0]
#   press-keys-for-s: send a space to the VM every second for that long (gets past "Press any key to boot from CD or DVD")
set -u
VM=${1:?vm name}; DUR=${2:-120}; LIMIT=${3:-2.0}; PRESS=${4:-0}
P=$(cd "$(dirname "$0")/.." && pwd)
OUT=$P/test-images/safe-$(echo "$VM" | tr ' ' _)-$(date +%H%M%S).log
U=/Applications/UTM.app/Contents/MacOS/utmctl
pgrep -xq UTM || { open -a /Applications/UTM.app; sleep 8; }
echo "VM=$VM duration=${DUR}s kill-threshold=${LIMIT}s  log=$OUT" | tee "$OUT"
"$U" start "$VM" >>"$OUT" 2>&1
start=$(date +%s); bad=0; max=0; sum=0; n=0
while [ $(( $(date +%s) - start )) -lt "$DUR" ]; do
    if [ $(( $(date +%s) - start )) -lt "$PRESS" ]; then
        (perl -e 'alarm 3; exec @ARGV' osascript -e "tell application \"UTM\" to input keystroke virtual machine \"$VM\" text \" \"" >/dev/null 2>&1 &)
    fi
    t0=$(perl -MTime::HiRes=time -e 'printf "%.3f", time')
    sleep 0.2
    t1=$(perl -MTime::HiRes=time -e 'printf "%.3f", time')
    lat=$(echo "$t1 - $t0 - 0.2" | bc -l)
    load=$(sysctl -n vm.loadavg | awk '{print $2}')
    qcpu=$(ps -A -o %cpu=,command= | awk '/QEMULauncher/ && !/awk/ {s+=$1} END {printf "%.0f", s}')
    printf "%3ds latency=%.3fs load1=%s qemu_cpu=%s%%\n" $(( $(date +%s) - start )) "$lat" "$load" "${qcpu:-0}" | tee -a "$OUT"
    n=$((n+1)); sum=$(echo "$sum + $lat" | bc -l); [ "$(echo "$lat > $max" | bc -l)" = 1 ] && max=$lat
    if [ "$(echo "$lat > $LIMIT" | bc -l)" = 1 ]; then bad=$((bad+1)); else bad=0; fi
    if [ $bad -ge 3 ]; then
        echo "!! host latency above ${LIMIT}s three times in a row: killing QEMU" | tee -a "$OUT"
        pkill -9 -f QEMULauncher; break
    fi
    sleep 0.8
done
"$U" stop "$VM" >/dev/null 2>&1; sleep 2; pkill -9 -f QEMULauncher 2>/dev/null
awk -v n=$n -v s=$sum -v m=$max -v k=$( [ $bad -ge 3 ] && echo yes || echo no ) 'BEGIN{printf "summary: samples=%d avg_latency=%.3fs max_latency=%.3fs killed=%s\n", n, (n?s/n:0), m, k}' | tee -a "$OUT"
