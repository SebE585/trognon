#!/bin/sh
# Send a payload to a device in Recovery (after tools/boot.sh) and verify its nonce.
# Usage: tools/send.sh PAYLOAD.bin [MARKER] [run_payload.py options, e.g. --evidence evidence --vars a,b --wait 40]
set -eu
IPWNDFU="${IPWNDFU:-$HOME/tools/ipwndfu}"
HERE="$(cd "$(dirname "$0")" && pwd)"
[ $# -ge 1 ] || { echo "usage: $0 PAYLOAD.bin [VARIABLE]"; exit 2; }
PAYLOAD="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
if [ -z "${TROGNON_LIBUSB_PATH:-}" ] && [ -f /opt/homebrew/lib/libusb-1.0.dylib ]; then
  export TROGNON_LIBUSB_PATH=/opt/homebrew/lib/libusb-1.0.dylib
fi
export PYTHONPATH=.
shift
# run_payload.py runs from the ipwndfu directory: make a relative --evidence DIR absolute first, or the
# bundle lands inside ipwndfu (seen 2026-10-16).
ARGS=""
want_dir=0
for a in "$@"; do
  if [ "$want_dir" = 1 ]; then
    case "$a" in /*) ;; *) a="$(pwd)/$a" ;; esac
    want_dir=0
  elif [ "$a" = "--evidence" ]; then
    want_dir=1
  fi
  ARGS="$ARGS $(printf '%s' "$a" | sed "s/'/'\\\\''/g; s/^/'/; s/\$/'/")"
done
cd "$IPWNDFU"
eval "exec python3 \"$HERE/run_payload.py\" \"$PAYLOAD\" $ARGS"
