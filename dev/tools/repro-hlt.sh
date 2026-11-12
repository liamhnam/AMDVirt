#!/bin/bash
# Reboots the Lima guest in a loop until vCPU0 hangs in the UEFI firmware's HLT loop
# (Virtualization.framework process at ~100% CPU and the guest never comes back), then dumps
# RyzenHV's flight recorder. Needs the amdvirt-info built from upstream/ (make tools) and kern.hv_support = 1.
#
# No sudo needed while the kext runs with rhv_allow_users=1 (limactl must NOT run as root).
# Usage: tools/repro-hlt.sh [instance=rhv-alpine] [max-reboots=15]
set -u
INST=${1:-rhv-alpine}
MAX=${2:-15}
P=$(cd "$(dirname "$0")/.." && pwd)
RHV=$P/upstream/build/amdvirt-info
OUT=$P/test-images/repro-hlt-$(date +%H%M%S)
export PATH=$HOME/.local/opt/lima/bin:$PATH
mkdir -p "$OUT"
[ -x "$RHV" ] || { echo "missing $RHV (run: cd upstream && make tools)"; exit 1; }
[ "$(sysctl -n kern.hv_support)" = 1 ] || { echo "kern.hv_support is 0 (amdvirt-info hv sysctl 1)"; exit 1; }

for i in $(seq 1 "$MAX"); do
    echo "== reboot $i"
    # 13 = record every exit (1), freeze the ring 64 exits (4) after the first injection that follows a vCPU reset (8):
    # it then holds which event the firmware got after this reboot and whether it answered with an EOI
    "$RHV" hv debug 13 >/dev/null 2>&1
    limactl shell "$INST" sudo reboot >/dev/null 2>&1
    sleep 8
    ok=0
    for t in $(seq 1 20); do                # up to ~60 s for the guest to answer again
        if perl -e 'alarm 5; exec @ARGV' limactl shell "$INST" true >/dev/null 2>&1; then ok=1; break; fi
        sleep 3
    done
    if [ $ok = 1 ]; then echo "   back ($(limactl shell "$INST" cat /proc/uptime 2>/dev/null | cut -d' ' -f1) s)"; continue; fi
    VZ=$(cat "$HOME/.lima/$INST/vz.pid" 2>/dev/null)
    echo "   NO ANSWER after reboot $i; VZ cpu = $(ps -o %cpu= -p "$VZ" 2>/dev/null)"
    "$RHV" log > "$OUT/log-before-stop.txt" 2>&1
    # stop the spinning VM first: its trap logging would otherwise overwrite the kernel log before the dump is read
    limactl stop -f "$INST" >/dev/null 2>&1
    sleep 2
    "$RHV" hv flightdump 400 > "$OUT/flightdump.txt" 2>&1
    "$RHV" stats > "$OUT/stats.txt" 2>&1
    "$RHV" log > "$OUT/log.txt" 2>&1
    "$RHV" hv debug 0 >/dev/null 2>&1
    echo "   dumps in $OUT"
    exit 2
done
"$RHV" hv debug 0 >/dev/null 2>&1
echo "no hang in $MAX reboots"
