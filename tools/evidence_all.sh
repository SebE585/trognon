#!/bin/sh
# Re-run the proven stages on the device and write an evidence bundle for each (one DFU cycle, one boot).
# Usage: tools/evidence_all.sh           (device in DFU; the payloads are rebuilt first)
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"
P="$HERE/payloads"
make -C "$P" trace.bin trace_pabt.bin trace_irq.bin trace_regs.bin trace_vic.bin trace_timer.bin >/dev/null
tries=0
until "$HERE/boot.sh" </dev/null 2>&1 | tee /tmp/trognon-boot.log | grep -q "boot_ibss() returned"; do
  tries=$((tries + 1))
  [ "$tries" -lt 6 ] || { echo "evidence_all.sh: pwn failed $tries times; put the device back in DFU and rerun"; exit 1; }
done
fail=0
for payload in trace trace_pabt trace_irq trace_regs trace_vic trace_timer; do
  echo "=== $payload"
  "$HERE/send.sh" "$P/$payload.bin" --evidence "$HERE/../evidence" --wait 8 || { echo "evidence_all.sh: $payload not proven"; fail=1; }
done
exit "$fail"
