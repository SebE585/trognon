"""Send a Thumb payload to a 3GS already in Recovery via a patched iBSS and run it.

Prerequisite: device pwned and booted with ipwndfu's boot_ibss() (the patched
iBSS maps 'run' to 0x41000001). Reads back the env variable the payload sets.

Usage: python3 run_payload.py PAYLOAD.bin [VARIABLE]   (run from ~/tools/ipwndfu)
"""
import sys, time
import usb.core
import recovery

payload = open(sys.argv[1], 'rb').read()
var = sys.argv[2] if len(sys.argv) > 2 else 'trognon'

def getenv(device, name):
    # iBoot STALLs on getenv of an unset variable: report it instead of dying.
    try:
        recovery.send_command(device, 'getenv ' + name)
        return bytes(device.ctrl_transfer(0xC0, 0, 0, 0, 256, 1000)).split(b'\0')[0].decode('ascii', 'replace')
    except usb.core.USBError as e:
        return '<unset: %s>' % e

device = recovery.acquire_device()
print('before:', var, '=', repr(getenv(device, var)))
print('sending %d bytes' % len(payload), flush=True)
recovery.send_data(device, payload)
print('data sent, issuing run', flush=True)
recovery.send_command(device, 'run')
print('run issued', flush=True)
time.sleep(0.5)
device = recovery.acquire_device()
print('after: ', var, '=', repr(getenv(device, var)))
recovery.release_device(device)
