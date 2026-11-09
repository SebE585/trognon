#!/usr/bin/env python3
"""
N90AP (iPhone 4) equivalent of boot_ibss_only.py (which refuses any CPID != 8920).
Reuses, unmodified: limera1n.exploit() for the pwn step, utilities.apply_patches for the two-byte patch,
dfu.send_data()/request_image_validation() for the send, recovery.acquire_device() (wrapped below) for the wait.
The only N90AP-specific things are the two patch offsets, sourced from
../../tools/iop_analysis/catalog/iBSS_n90ap_4.3.1/reset_command_table_0x84022cb4.yaml (IBSS90-RST-002):
  body offset 0x1d088 -> b'run\\x00'       ("reset" command string -> "run")
  body offset 0x22c78 -> u32le(0x41000001) ("reset" handler pointer -> LOAD_ADDRESS+1)
Same chain as dfuexec.PwnedDFUDevice.boot_ibss() (3GS); see docs/journal/2026-10-22.md for how these offsets
were found by comparison with the 3GS's own n88ap_iBSS_435_patches, and docs/journal/2026-11-09.md for the
first reliability data on this script.

Expects a GID-decrypted N90AP iBSS 4.3.1 at ~/tools/ipwndfu/fw-cache/ip4/iBSS.n90ap.decrypted_img3.bin (produce
it once with decrypt_image3.py, see tools/ipwndfu/README.md); its SHA-256 is checked before anything is sent.

Shared-libusb-context patch (docs/journal/2026-10-21.md, 2026-10-22.md: without it, pwn and the first execute()
are both unreliable on this host) applied to dfu.acquire_device() before anything else, same as try_limera1n.py.

RAM only: never calls flash_nor() or anything that writes NOR. Recoverable at any point by a full DFU cycle
(Power+Home), not just a USB replug.

Exit codes (no log parsing), mirroring boot_ibss_only.py:
  0   iPhone 4 reached Recovery Mode (0x1281) with the patched iBSS
  10  iPhone 4 is in DFU but not pwned after limera1n.exploit()
  11  anything else (no iPhone 4 in DFU, patched iBSS refused, never reached Recovery)
"""
import os, sys, time, hashlib, struct

IPWNDFU = os.path.expanduser('~/tools/ipwndfu')
sys.path.insert(0, IPWNDFU)
os.chdir(IPWNDFU)  # limera1n.py reads 'bin/limera1n-shellcode.bin' as a path relative to the CWD
import usb.core, usb.backend.libusb1
import dfu

_shared_backend = usb.backend.libusb1.get_backend(find_library=dfu._find_library)
if _shared_backend is not None:
    def _acquire_shared(timeout=5.0, match=None, fatal=True):
        start = time.time()
        once = False
        while not once or time.time() - start < timeout:
            once = True
            for device in usb.core.find(find_all=True, idVendor=0x5AC, idProduct=0x1227, backend=_shared_backend):
                if match is not None and match not in device.serial_number:
                    continue
                return device
            time.sleep(0.001)
        if fatal:
            print('ERROR: No Apple device in DFU Mode 0x1227 detected after %0.2f second timeout. Exiting.' % timeout)
            sys.exit(1)
        return None
    dfu.acquire_device = _acquire_shared

import limera1n, dfuexec, utilities, recovery

# Several iOS devices could sit in Recovery on this bench. recovery.acquire_device() takes the first 0x1281
# device whatever it is. Only CPID:8930 (iPhone 4) is accepted here.
def _acquire_8930(timeout=10):
    backend = usb.backend.libusb1.get_backend(find_library=recovery._find_library)
    end = time.time() + timeout
    while time.time() < end:
        for d in usb.core.find(find_all=True, idVendor=0x5AC, idProduct=0x1281, backend=backend):
            try:
                if 'CPID:8930' in (d.serial_number or ''):
                    return d
            except (usb.core.USBError, ValueError):
                pass
        time.sleep(0.1)
    print('ERROR: no iPhone 4 (CPID:8930) in Recovery Mode 0x1281 within %ds; other Recovery devices are ignored.' % timeout)
    sys.exit(1)
recovery.acquire_device = _acquire_8930

DECRYPTED_IBSS_PATH = os.path.join(IPWNDFU, 'fw-cache/ip4/iBSS.n90ap.decrypted_img3.bin')
DECRYPTED_IBSS_SHA256 = '985d4940e7132e4fcbc35895ba323a163a87993c773d9a00e66a2c0e8e1bf4e1'  # recorded 2026-10-22

N90AP_IBSS_431_PATCHES = [
    (0x1d088,                     b'run\x00'),                     # 'reset' command string -> 'run'
    (0x22c78, struct.pack('<I', 0x41000001)),                      # 'reset' handler -> LOAD_ADDRESS + 1
]

EXIT_OK, EXIT_NOT_PWNED, EXIT_FAILED = 0, 10, 11

def main():
    print('Hold Power now. Starting in:')
    for n in (5, 4, 3, 2, 1):
        print(' ', n)
        time.sleep(1)
    print('Go.')
    d = dfu.acquire_device(timeout=5, match='CPID:8930', fatal=False)
    if d is None:
        print('ERROR: no iPhone 4 (CPID:8930) in DFU mode.')
        return EXIT_FAILED
    already_pwned = 'PWND:[' in (d.serial_number or '')
    print('Device in DFU:', d.serial_number)
    dfu.release_device(d)

    if not already_pwned:
        print('Not pwned yet: calling limera1n.exploit().')
        try:
            limera1n.exploit()
        except SystemExit as e:
            if e.code not in (0, None):
                print('limera1n.exploit() exited with code %r.' % e.code)
                return EXIT_NOT_PWNED

    device = dfuexec.PwnedDFUDevice()
    print('Pwned device confirmed:', device.identifier)
    print('Config version:', device.config.version, 'cpid:', device.config.cpid)
    if device.config.cpid != '8930':
        print('ERROR: expected CPID:8930 (iPhone 4), got CPID:%s.' % device.config.cpid)
        return EXIT_FAILED

    with open(DECRYPTED_IBSS_PATH, 'rb') as f:
        decryptediBSS = f.read()
    got_hash = hashlib.sha256(decryptediBSS).hexdigest()
    if got_hash != DECRYPTED_IBSS_SHA256:
        print('ERROR: %s hash mismatch (got %s, expected %s). Not sending.' % (DECRYPTED_IBSS_PATH, got_hash, DECRYPTED_IBSS_SHA256))
        return EXIT_FAILED
    print('Decrypted iBSS N90AP 4.3.1 loaded and hash-verified:', got_hash)

    patchediBSS = decryptediBSS[:64] + utilities.apply_patches(decryptediBSS[64:], N90AP_IBSS_431_PATCHES)

    dev = dfu.acquire_device()
    assert device.identifier == dev.serial_number
    dfu.reset_counters(dev)
    dfu.request_image_validation(dev)
    dfu.release_device(dev)

    time.sleep(0.5)

    dev = dfu.acquire_device()
    assert device.identifier == dev.serial_number
    print('Sending patched iBSS (%d bytes).' % len(patchediBSS))
    dfu.send_data(dev, patchediBSS)
    dfu.request_image_validation(dev)
    dfu.release_device(dev)

    time.sleep(0.5)

    print('Waiting for iBSS to enter Recovery Mode (hold Power now if not already held).')
    rec = recovery.acquire_device()
    recovery.release_device(rec)
    print('Reached Recovery Mode (0x1281, CPID:8930).')
    print('Stopping here deliberately: not calling flash_nor() or anything that writes NOR.')
    return EXIT_OK

try:
    code = main()
except SystemExit as e:
    code = EXIT_OK if e.code in (0, None) else EXIT_FAILED
sys.exit(code)
