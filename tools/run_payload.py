"""Send a Thumb payload to a 3GS already in Recovery via a patched iBSS and run it.

Prerequisite: device pwned and booted with ipwndfu's boot_ibss() (the patched
iBSS maps 'run' to 0x41000001); see tools/boot.sh. Reads back the env variable
the payload sets.

Each run is made unambiguous: a random 32-bit nonce is appended to the binary, the payload
republishes it as env variable `nonce` (hal/result.c), and this script checks it. A match proves
that THIS binary, sent THIS time, executed; a stale marker from an earlier run cannot match.
The SHA-256 of the binary and the outcome are appended to a log (default ~/trognon-runs.log,
override with TROGNON_RUN_LOG).

Usage: python3 run_payload.py PAYLOAD.bin [VARIABLE]   (run from the ipwndfu checkout, see tools/boot.sh)
Exit status: 0 nonce confirmed, 3 nonce missing or different (the run is NOT proven), 1 on USB error.
"""
import hashlib, os, sys, time
import usb.core
import recovery

base = open(sys.argv[1], 'rb').read()
var = sys.argv[2] if len(sys.argv) > 2 else 'trognon'
nonce = os.urandom(4)
payload = base + b'\0' * ((-len(base)) % 4) + nonce
nonce_hex = '%08X' % int.from_bytes(nonce, 'little')   # the ARM payload reads the 4 bytes as a little-endian u32
sha = hashlib.sha256(base).hexdigest()

def getenv(device, name):
    # iBoot STALLs on getenv of an unset variable: report it instead of dying.
    try:
        recovery.send_command(device, 'getenv ' + name)
        return bytes(device.ctrl_transfer(0xC0, 0, 0, 0, 256, 1000)).split(b'\0')[0].decode('ascii', 'replace')
    except usb.core.USBError as e:
        return '<unset: %s>' % e

def log(outcome):
    path = os.environ.get('TROGNON_RUN_LOG', os.path.expanduser('~/trognon-runs.log'))
    try:
        with open(path, 'a') as f:
            f.write('%s %s sha256=%s nonce=%s %s\n' % (time.strftime('%Y-%m-%dT%H:%M:%S%z'),
                    os.path.basename(sys.argv[1]), sha, nonce_hex, outcome))
    except OSError:
        pass

print('payload sha256 %s (%d bytes, +4 nonce bytes)' % (sha, len(base)))
print('nonce sent     %s' % nonce_hex)
device = recovery.acquire_device()
print('before:', var, '=', repr(getenv(device, var)), '| nonce =', repr(getenv(device, 'nonce')))
print('sending %d bytes' % len(payload), flush=True)
try:
    recovery.send_data(device, payload)
    print('data sent, issuing run', flush=True)
    recovery.send_command(device, 'run')
    print('run issued', flush=True)
    time.sleep(0.5)
    device = recovery.acquire_device()
    after = getenv(device, var)
    got = getenv(device, 'nonce')
    recovery.release_device(device)
except usb.core.USBError as e:
    print('USB error:', e)
    log('usb-error')
    sys.exit(1)
print('after: ', var, '=', repr(after))
print('nonce read     %s' % got)
if got == nonce_hex:
    print('NONCE CONFIRMED: this binary executed on the device in this run')
    log('confirmed %s=%s' % (var, after))
    sys.exit(0)
print('NONCE NOT CONFIRMED: the run is not proven (old payload without nonce, or it did not execute)')
log('unconfirmed %s=%s nonce=%s' % (var, after, got))
sys.exit(3)
