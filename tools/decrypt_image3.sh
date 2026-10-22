#!/bin/sh
# Decrypt an img3/dfu file with the pwned device's own GID key, read-only on the chip side.
# See tools/ipwndfu/decrypt_image3.py.
# Usage: tools/decrypt_image3.sh CPID INPUT OUTPUT
set -eu
IPWNDFU="${IPWNDFU:-$HOME/tools/ipwndfu}"
PIN=0e28932ec6a2a570b10fd77e50bda4216418cd98
HERE="$(cd "$(dirname "$0")" && pwd)"

[ $# -eq 3 ] || { echo "usage: $0 CPID INPUT OUTPUT"; exit 2; }
CPID="$1"; IN="$2"; OUT="$3"
[ -f "$IN" ] || { echo "decrypt_image3.sh: no such file: $IN"; exit 1; }
# decrypt_image3.py runs from inside $IPWNDFU: make relative paths absolute first, or they'd resolve there instead.
case "$IN" in /*) ;; *) IN="$(pwd)/$IN" ;; esac
case "$OUT" in /*) ;; *) OUT="$(pwd)/$OUT" ;; esac

[ -d "$IPWNDFU" ] || { echo "decrypt_image3.sh: no ipwndfu at $IPWNDFU (see tools/ipwndfu/README.md)"; exit 1; }
HEAD_NOW="$(git -C "$IPWNDFU" rev-parse HEAD)"
[ "$HEAD_NOW" = "$PIN" ] || { echo "decrypt_image3.sh: ipwndfu is at $HEAD_NOW, expected the pinned $PIN"; exit 1; }
git -C "$IPWNDFU" apply --reverse --check "$HERE/ipwndfu/python3-arm64.patch" 2>/dev/null \
  || { echo "decrypt_image3.sh: the Trognon patch is not applied in $IPWNDFU (tools/ipwndfu/README.md)"; exit 1; }
python3 -I "$HERE/ipwndfu/require_3gs.py" "$CPID" || exit 1
cp "$HERE/ipwndfu/decrypt_image3.py" "$IPWNDFU/"

if [ -z "${TROGNON_LIBUSB_PATH:-}" ] && [ -f /opt/homebrew/lib/libusb-1.0.dylib ]; then
  export TROGNON_LIBUSB_PATH=/opt/homebrew/lib/libusb-1.0.dylib
fi
export PYTHONPATH=.
cd "$IPWNDFU"
python3 decrypt_image3.py "$CPID" "$IN" "$OUT"
