#!/bin/sh
# One command for the whole cycle: device in DFU -> pwn -> patched iBSS -> send the payload -> read variables.
# The device leaves Recovery after a few idle minutes, so doing it in one go avoids a second DFU.
# Usage: tools/go.sh PAYLOAD.bin [run_payload.py options]   e.g. --vars t_hits,t_stray --wait 40
# An evidence bundle is written under evidence/ (see tools/run_payload.py).
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"
IPWNDFU="${IPWNDFU:-$HOME/tools/ipwndfu}"
[ $# -ge 1 ] || { echo "usage: $0 PAYLOAD.bin [VARIABLE ...]"; exit 2; }
PAYLOAD="$1"; shift
python3 -I "$HERE/ipwndfu/require_3gs.py" || exit 1   # one loud refusal, not six retries
LOG="${TMPDIR:-/tmp}/trognon-boot.log"
tries=0
while :; do
  boot=0
  "$HERE/boot.sh" </dev/null >"$LOG" 2>&1 || boot=$?
  cat "$LOG"
  [ "$boot" -eq 0 ] && break
  [ "$boot" -eq 10 ] || { echo "go.sh: boot failed (exit $boot); put the device back in DFU and rerun"; exit "$boot"; }
  tries=$((tries + 1))
  [ "$tries" -lt 6 ] || { echo "go.sh: pwn failed $tries times; put the device back in DFU and rerun"; exit 1; }
  echo "go.sh: pwn failed (try $tries), retrying"
done
status=0
"$HERE/send.sh" "$PAYLOAD" --evidence "$HERE/../evidence" "$@" || status=$?
exit "$status"
