#!/bin/sh
# Pwn a 3GS in DFU and boot the patched iBSS (Recovery, RAM only). See tools/ipwndfu/README.md.
set -eu
IPWNDFU="${IPWNDFU:-$HOME/tools/ipwndfu}"
PIN=0e28932ec6a2a570b10fd77e50bda4216418cd98
HERE="$(cd "$(dirname "$0")" && pwd)"

[ -d "$IPWNDFU" ] || { echo "boot.sh: no ipwndfu at $IPWNDFU (see tools/ipwndfu/README.md)"; exit 1; }
HEAD_NOW="$(git -C "$IPWNDFU" rev-parse HEAD)"
[ "$HEAD_NOW" = "$PIN" ] || { echo "boot.sh: ipwndfu is at $HEAD_NOW, expected the pinned $PIN"; exit 1; }
git -C "$IPWNDFU" apply --reverse --check "$HERE/ipwndfu/python3-arm64.patch" 2>/dev/null \
  || { echo "boot.sh: the Trognon patch is not applied in $IPWNDFU (tools/ipwndfu/README.md)"; exit 1; }
python3 -I "$HERE/ipwndfu/require_3gs.py" || exit 1
cp "$HERE/ipwndfu/boot_ibss_only.py" "$HERE/ipwndfu/gate_snapshot.py" "$IPWNDFU/"

if [ -z "${TROGNON_LIBUSB_PATH:-}" ] && [ -f /opt/homebrew/lib/libusb-1.0.dylib ]; then
  export TROGNON_LIBUSB_PATH=/opt/homebrew/lib/libusb-1.0.dylib
fi
export PYTHONPATH=.
cd "$IPWNDFU"

# One attempt = pwn, then boot_ibss_only.py, whose exit code is the verdict (see its header):
#   0 booted, 10 not pwned (try again), anything else is fatal. No log text is parsed.
tries=0
while :; do
  python3 ipwndfu -p || true          # ipwndfu -p has no reliable exit code: the device state is checked below
  status=0
  python3 boot_ibss_only.py || status=$?
  [ "$status" -eq 10 ] || exit "$status"
  tries=$((tries + 1))
  if [ "$tries" -ge 3 ] || [ ! -t 0 ]; then
    echo "boot.sh: the device is not pwned (try $tries). Put it back in DFU and rerun."
    exit 10
  fi
  echo "boot.sh: pwn failed (try $tries). If the device left DFU, put it back, then press Enter."
  read -r _
done
