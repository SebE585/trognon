#!/bin/sh
# Send a payload to a device in Recovery (after tools/boot.sh) and verify its nonce.
# Usage: tools/send.sh PAYLOAD.bin [VARIABLE]
set -eu
IPWNDFU="${IPWNDFU:-$HOME/tools/ipwndfu}"
HERE="$(cd "$(dirname "$0")" && pwd)"
[ $# -ge 1 ] || { echo "usage: $0 PAYLOAD.bin [VARIABLE]"; exit 2; }
PAYLOAD="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
if [ -z "${TROGNON_LIBUSB_PATH:-}" ] && [ -f /opt/homebrew/lib/libusb-1.0.dylib ]; then
  export TROGNON_LIBUSB_PATH=/opt/homebrew/lib/libusb-1.0.dylib
fi
export PYTHONPATH=.
cd "$IPWNDFU"
exec python3 "$HERE/run_payload.py" "$PAYLOAD" "${2:-trognon}"
