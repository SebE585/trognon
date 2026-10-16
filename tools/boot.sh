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
cp "$HERE/ipwndfu/boot_ibss_only.py" "$IPWNDFU/boot_ibss_only.py"

if [ -z "${TROGNON_LIBUSB_PATH:-}" ] && [ -f /opt/homebrew/lib/libusb-1.0.dylib ]; then
  export TROGNON_LIBUSB_PATH=/opt/homebrew/lib/libusb-1.0.dylib
fi
export PYTHONPATH=.
cd "$IPWNDFU"

tries=0
until python3 ipwndfu -p 2>&1 | tee /dev/stderr | grep -qE "now in pwned DFU Mode|already in pwned DFU Mode"; do
  tries=$((tries + 1))
  if [ "$tries" -ge 3 ]; then
    echo "boot.sh: pwn failed $tries times. Put the device back in DFU and rerun."
    exit 1
  fi
  echo "boot.sh: pwn failed (try $tries). If the device left DFU, put it back, then press Enter."
  read -r _
done
python3 boot_ibss_only.py
