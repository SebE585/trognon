#!/bin/sh
# Comparative test: pwn an iPhone 4 (CPID:8930) with limera1n instead of ipwndfu's own SHAtter choice for that
# chip, then one extra DFU request, nothing more. See tools/ipwndfu/try_limera1n.py.
set -eu
IPWNDFU="${IPWNDFU:-$HOME/tools/ipwndfu}"
PIN=0e28932ec6a2a570b10fd77e50bda4216418cd98
HERE="$(cd "$(dirname "$0")" && pwd)"

[ -d "$IPWNDFU" ] || { echo "try_limera1n.sh: no ipwndfu at $IPWNDFU (see tools/ipwndfu/README.md)"; exit 1; }
HEAD_NOW="$(git -C "$IPWNDFU" rev-parse HEAD)"
[ "$HEAD_NOW" = "$PIN" ] || { echo "try_limera1n.sh: ipwndfu is at $HEAD_NOW, expected the pinned $PIN"; exit 1; }
git -C "$IPWNDFU" apply --reverse --check "$HERE/ipwndfu/python3-arm64.patch" 2>/dev/null \
  || { echo "try_limera1n.sh: the Trognon patch is not applied in $IPWNDFU (tools/ipwndfu/README.md)"; exit 1; }
python3 -I "$HERE/ipwndfu/require_3gs.py" 8930 || exit 1
cp "$HERE/ipwndfu/try_limera1n.py" "$IPWNDFU/"

if [ -z "${TROGNON_LIBUSB_PATH:-}" ] && [ -f /opt/homebrew/lib/libusb-1.0.dylib ]; then
  export TROGNON_LIBUSB_PATH=/opt/homebrew/lib/libusb-1.0.dylib
fi
export PYTHONPATH=.
cd "$IPWNDFU"
python3 try_limera1n.py
