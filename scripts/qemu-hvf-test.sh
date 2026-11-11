#!/bin/bash
#
# Boots a small Linux (Alpine's netboot kernel + initramfs) in QEMU's HVF accelerator, through the AMDVirt shim.
#
#   ./scripts/qemu-hvf-test.sh            # boots; leave QEMU with Ctrl-A then X
#   AMDVIRT_TRACE=1 ./scripts/qemu-hvf-test.sh   # also log every Hypervisor.framework call to /tmp/amdvirt-qemu-trace.txt
#
# Success looks like Linux kernel messages on the serial console and, at the end, "initramfs emergency recovery
# shell launched" and a "~ #" prompt (there is no root disk and no network, so Alpine's init gives up there).
#
#   AMDVIRT_STATS=1 ./scripts/qemu-hvf-test.sh   # once a second: exits/s, kernel calls per exit, us per exit -> /tmp/amdvirt-stats.txt
#
# Extra QEMU options go in AMDVIRT_QEMU_EXTRA, e.g. to log what QEMU's interrupt controllers do:
#   AMDVIRT_QEMU_EXTRA="-D /tmp/amdvirt-qemu.log -trace ioapic_* -trace apic_deliver_irq -trace apic_local_deliver"
#
# QEMU and the guest files live in ~/amdvirt-qemu (see docs/legacy/PHASE3.md for how they were built).
set -eu
HERE="$(cd "$(dirname "$0")/.." && pwd)"
W="${AMDVIRT_QEMU_DIR:-$HOME/amdvirt-qemu}"
QEMU="$W/src/build-qemu/qemu-system-x86_64"
LIB="$HERE/build/libamdvirt.dylib"
[ -x "$QEMU" ] || { echo "QEMU is not built at $QEMU" >&2; exit 1; }
[ -f "$LIB" ] || { echo "build the shim first: make shim" >&2; exit 1; }
TRACE="${AMDVIRT_TRACE:-0}"
sudo rm -f /tmp/amdvirt-stats.txt /tmp/amdvirt-qemu-trace.txt /tmp/amdvirt-mon.sock       # the shim appends; start from empty files
exec sudo env DYLD_INSERT_LIBRARIES="$LIB" AMDVIRT_TRACE="$TRACE" AMDVIRT_TRACE_FILE=/tmp/amdvirt-qemu-trace.txt \
    AMDVIRT_STATS="${AMDVIRT_STATS:-0}" AMDVIRT_STATS_FILE=/tmp/amdvirt-stats.txt \
    "$QEMU" -accel hvf -m 512 -smp 1 -nographic -no-reboot ${AMDVIRT_QEMU_EXTRA:-} \
    -kernel "$W/guest/vmlinuz-virt" -initrd "$W/guest/initramfs-virt" -append "console=ttyS0 ${AMDVIRT_KERNEL_ARGS:-}"
