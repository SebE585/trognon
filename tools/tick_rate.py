"""Measure the S5L8920 free-running tick rate against the host clock.

Runs the ticks payload twice, N seconds apart (host time), each time reading the counter the payload
published through the iBSS environment variable `ticks`. Rate = delta ticks / delta host time.

Prerequisite: device in Recovery with the patched iBSS (ipwndfu -p, then boot_ibss(); see
docs/en/01-boot-chain.md). Run from ~/tools/ipwndfu with PYTHONPATH=. like run_payload.py.
Usage: python3 tick_rate.py PAYLOAD.bin [SECONDS]      (default 30 s)
The 32-bit counter wraps: with a 24 MHz tick that is every ~179 s, so keep SECONDS below that.
"""
import sys, time
import usb.core
import recovery

payload = open(sys.argv[1], 'rb').read()
gap = float(sys.argv[2]) if len(sys.argv) > 2 else 30.0

def getenv(device, name):
    recovery.send_command(device, 'getenv ' + name)
    return bytes(device.ctrl_transfer(0xC0, 0, 0, 0, 256, 1000)).split(b'\0')[0].decode('ascii')

def sample():
    device = recovery.acquire_device()
    t0 = time.monotonic()
    recovery.send_data(device, payload)
    recovery.send_command(device, 'run')
    t1 = time.monotonic()
    time.sleep(0.3)
    device = recovery.acquire_device()
    ticks = int(getenv(device, 'ticks'), 16)
    recovery.release_device(device)
    return ticks, (t0 + t1) / 2, t1 - t0

a, ta, ja = sample()
print('sample 1: ticks=%#010x  (send+run took %.2f s)' % (a, ja), flush=True)
time.sleep(gap)
b, tb, jb = sample()
print('sample 2: ticks=%#010x  (send+run took %.2f s)' % (b, jb))
d_ticks, d_t = (b - a) & 0xFFFFFFFF, tb - ta
print('delta ticks = %d over %.3f s host time -> %.0f Hz  (uncertainty ~ %.1f %%)'
      % (d_ticks, d_t, d_ticks / d_t, 100 * (ja + jb) / 2 / d_t))
