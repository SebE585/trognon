#!/usr/bin/env python3
"""
One-off script, not part of ipwndfu: calls PwnedDFUDevice.boot_ibss() in
isolation and stops there. dfuexec.py only ever calls boot_ibss() as the
first step of flash_nor(), which goes on to permanently write NOR flash
afterward. We deliberately never call flash_nor() or anything past
boot_ibss() here, to stay RAM-only: this sends a patched iBSS over DFU
and waits for the device to reach Recovery mode, nothing more.
"""
import os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import time
import usb.core
import dfuexec, utilities, recovery

# Several iOS devices can sit in Recovery on this bench (an iPad 2 in the same session). recovery.acquire_device()
# takes the first 0x1281 device whatever it is, so it can wait on, or send to, the wrong one. Only the 3GS
# (CPID:8920) is accepted here, and nothing else is ever used.
def _acquire_3gs(timeout=10):
    import usb.backend.libusb1
    backend = usb.backend.libusb1.get_backend(find_library=recovery._find_library)
    end = time.time() + timeout
    while time.time() < end:
        for d in usb.core.find(find_all=True, idVendor=0x5AC, idProduct=0x1281, backend=backend):
            try:
                if 'CPID:8920' in (d.serial_number or ''):
                    return d
            except (usb.core.USBError, ValueError):   # unreadable serial: not proven to be the 3GS, so skipped
                pass
        time.sleep(0.1)
    print('ERROR: no iPhone 3GS (CPID:8920) in Recovery Mode 0x1281 within %ds; other Recovery devices are ignored.' % timeout)
    sys.exit(1)
recovery.acquire_device = _acquire_3gs

# TROGNON_GATE_SNAP=1: also hook the iBSS clock-gate wrapper to snapshot the gate words (STAGE 48, read-only).
if os.environ.get('TROGNON_GATE_SNAP') == '1':
    import gate_snapshot
    _apply = utilities.apply_patches

    def _apply_with_snapshot(binary, patches):
        if any(data == b'run\x00' for _, data in patches):   # the boot_ibss() patch list, nothing else
            patches = list(patches) + gate_snapshot.patches(binary)
            print('Gate snapshot hook added (TROGNON_GATE_SNAP=1).')
        return _apply(binary, patches)
    utilities.apply_patches = _apply_with_snapshot

# Exit codes, read by boot.sh and go.sh (no log parsing):
#   0   a 3GS reached Recovery with the patched iBSS (the only success)
#   10  a 3GS is in DFU but not pwned: pwn it again, then retry
#   11  anything else (no 3GS in DFU, iBSS refused, the 3GS never reached Recovery): stop, put it back in DFU by hand
EXIT_OK, EXIT_NOT_PWNED, EXIT_FAILED = 0, 10, 11

def main():
    import dfu
    d = dfu.acquire_device(timeout=5, match='CPID:8920', fatal=False)
    if d is None:
        print('ERROR: no iPhone 3GS (CPID:8920) in DFU mode.')
        return EXIT_FAILED
    pwned = 'PWND:[' in (d.serial_number or '')
    dfu.release_device(d)
    if not pwned:
        print('The 3GS is in DFU but not pwned.')
        return EXIT_NOT_PWNED
    device = dfuexec.PwnedDFUDevice()
    print('Pwned device confirmed:', device.identifier)
    print('Config version:', device.config.version, 'cpid:', device.config.cpid)
    device.boot_ibss()   # waits for a CPID:8920 device in Recovery (recovery.acquire_device above); exits if none comes
    print('boot_ibss() returned: device should now be in Recovery Mode (0x1281).')
    print('Stopping here deliberately: not calling flash_nor() or anything that writes NOR.')
    return EXIT_OK

try:
    code = main()
except SystemExit as e:   # ipwndfu and our helpers sys.exit(1) on errors
    code = EXIT_OK if e.code in (0, None) else EXIT_FAILED
sys.exit(code)
