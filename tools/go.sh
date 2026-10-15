#!/bin/sh
# One command for the whole cycle: device in DFU -> pwn -> patched iBSS -> send the payload -> read variables.
# The device leaves Recovery after a few idle minutes, so doing it in one go avoids a second DFU.
# Usage: tools/go.sh PAYLOAD.bin [VARIABLE ...]     (variables are read after the nonce check)
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"
IPWNDFU="${IPWNDFU:-$HOME/tools/ipwndfu}"
[ $# -ge 1 ] || { echo "usage: $0 PAYLOAD.bin [VARIABLE ...]"; exit 2; }
PAYLOAD="$1"; shift
tries=0
until "$HERE/boot.sh" </dev/null 2>&1 | tee /tmp/trognon-boot.log | grep -q "boot_ibss() returned"; do
  tries=$((tries + 1))
  [ "$tries" -lt 6 ] || { echo "go.sh: pwn failed $tries times; put the device back in DFU and rerun"; exit 1; }
  echo "go.sh: pwn failed (try $tries), retrying"
done
status=0
"$HERE/send.sh" "$PAYLOAD" || status=$?
if [ $# -gt 0 ] && [ "$status" -ne 1 ]; then
  if [ -z "${TROGNON_LIBUSB_PATH:-}" ] && [ -f /opt/homebrew/lib/libusb-1.0.dylib ]; then
    export TROGNON_LIBUSB_PATH=/opt/homebrew/lib/libusb-1.0.dylib
  fi
  (cd "$IPWNDFU" && PYTHONPATH=. python3 "$HERE/getenv.py" "$@")
fi
exit "$status"
