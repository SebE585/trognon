"""Measure the S5L8920 free-running tick rate against the host clock, one sample per invocation.

Each run sends the ticks payload, reads the counter it published through the iBSS environment variable
`ticks`, and appends (host epoch, ticks) to a state file. From the second sample on it prints the rate
against the previous sample. A single process cannot hold the device across a long pause (the second
acquire fails with "Access denied"), hence one sample per invocation.

Prerequisite: device in Recovery with the patched iBSS (ipwndfu -p, then boot_ibss(); see
docs/en/01-boot-chain.md). Run from ~/tools/ipwndfu with PYTHONPATH=. like run_payload.py.
Usage: python3 tick_rate.py PAYLOAD.bin [STATEFILE]      (state default: /tmp/trognon_ticks.txt)
Start from a clean state file after every new DFU (rm it): the counter restarts when the device resets.
The 32-bit counter wraps: at 24 MHz that is every ~179 s, so two samples must be less than that apart.
"""
import os, sys, time
import recovery

base = open(sys.argv[1], 'rb').read()
nonce = os.urandom(4)
payload = base + b'\0' * ((-len(base)) % 4) + nonce
nonce_hex = '%08X' % int.from_bytes(nonce, 'little')
state = sys.argv[2] if len(sys.argv) > 2 else '/tmp/trognon_ticks.txt'

def getenv(device, name):
    recovery.send_command(device, 'getenv ' + name)
    return bytes(device.ctrl_transfer(0xC0, 0, 0, 0, 256, 1000)).split(b'\0')[0].decode('ascii')

device = recovery.acquire_device()
t0 = time.time()
recovery.send_data(device, payload)
recovery.send_command(device, 'run')
t1 = time.time()
time.sleep(0.3)
device = recovery.acquire_device()
ticks = int(getenv(device, 'ticks'), 16)
got = getenv(device, 'nonce')
recovery.release_device(device)
if got != nonce_hex:
    sys.exit('nonce mismatch (sent %s, read %s): this sample is not from this run, discarded' % (nonce_hex, got))
t = (t0 + t1) / 2
print('ticks = %#010x  (host %.3f, send+run %.3f s)' % (ticks, t, t1 - t0))

try:
    prev = [tuple(map(float, l.split())) for l in open(state) if l.strip()][-1]
except (OSError, IndexError):
    prev = None
with open(state, 'a') as f:
    f.write('%.6f %d\n' % (t, ticks))
if prev:
    dt, dk = t - prev[0], (ticks - int(prev[1])) & 0xFFFFFFFF
    print('since previous sample: %d ticks over %.3f s -> %.0f Hz  (host timing uncertainty ~ %.1f %%)'
          % (dk, dt, dk / dt, 100 * (t1 - t0) / dt))
