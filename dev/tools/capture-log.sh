#!/bin/bash
# Streams RyzenHV's kernel log into a file and fsyncs every line, so the last lines before a host panic survive the reboot.
# Usage: tools/capture-log.sh [out-file]   (stop with Ctrl-C)
P=$(cd "$(dirname "$0")/.." && pwd)
OUT=${1:-$P/test-images/capture-$(date +%Y%m%d-%H%M%S).log}
echo "writing $OUT"
"$P/upstream/build/amdvirt-info" log -f 2>&1 | perl -e '
    use IO::Handle;
    open(my $fh, ">>", $ARGV[0]) or die "open: $!";
    while (my $l = <STDIN>) { print $fh $l; $fh->flush; $fh->sync; }
' "$OUT"
