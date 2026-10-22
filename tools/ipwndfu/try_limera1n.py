#!/usr/bin/env python3
"""
One-off script, not part of ipwndfu: calls limera1n.exploit() directly on an iPhone 4 (CPID:8930). ipwndfu's own
`-p` selector routes CPID:8930 to SHAtter.exploit() (ipwndfu:58); limera1n.py lists a DeviceConfig for '574.4'/8930
(limera1n.py:123) but the CLI never reaches it. This calls it anyway, to compare the two exploits on the same
device and host without touching the pinned ipwndfu checkout.

Patches dfu.acquire_device() to reuse a single libusb backend/context for the whole run, instead of the stock
behaviour of calling usb.backend.libusb1.get_backend() (and so libusb_init(), a brand-new context) on every
acquire. That churn made both exploits' pwn step unreliable on this host (0/16 SHAtter pwns and 7/7 failed
limera1n overflows recorded on 2026-10-21, both while rapidly creating and tearing down contexts); with ONE
shared context, limera1n pwns an iPhone 4 reliably (3/3 fresh pwns on 2026-10-22, each followed by a full,
hash-verified SecureROM dump via dfuexec.PwnedDFUDevice().securerom_dump()). See docs/journal/2026-10-22.md.

After a successful pwn, does exactly ONE further DFU request (dfuexec.PwnedDFUDevice(), which reads the config
over one control transfer) and stops there. Does not call boot_ibss() or anything that writes NOR. Repeating
further active requests on an already-pwned session degrades reliably on this host (same symptom on the 3GS
with limera1n and on the iPhone 4 with SHAtter, both documented 2026-10-21/22): a fresh DFU cycle (not just a
USB replug) is needed before the next attempt.
"""
import os, sys, time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import usb.core, usb.backend.libusb1
import dfu, limera1n

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

# Exit codes (no log parsing):
#   0   pwned, and the one extra DFU request after succeeded
#   10  not pwned (retry): the device left pwned DFU mode, or was never reached
#   11  pwned, but the one extra DFU request after failed: points at the receive loop, not at the exploit's pwn step
EXIT_OK, EXIT_NOT_PWNED, EXIT_LOOP_FAILED = 0, 10, 11

def main():
    try:
        limera1n.exploit()
    except SystemExit as e:
        if e.code not in (0, None):
            print('limera1n.exploit() exited with code %r.' % e.code)
            return EXIT_NOT_PWNED
    try:
        d = dfu.acquire_device()
    except Exception as e:
        print('ERROR: no device in DFU after limera1n.exploit(): %r' % e)
        return EXIT_NOT_PWNED
    pwned = 'PWND:[' in (d.serial_number or '')
    print('After limera1n.exploit():', d.serial_number)
    dfu.release_device(d)
    if not pwned:
        return EXIT_NOT_PWNED

    import dfuexec
    try:
        device = dfuexec.PwnedDFUDevice()
        print('One extra DFU request OK. Config version:', device.config.version, 'cpid:', device.config.cpid)
    except Exception as e:
        print('ERROR: the one extra DFU request failed: %r' % e)
        return EXIT_LOOP_FAILED
    print('Stopping here deliberately: not calling boot_ibss() or flash_nor().')
    return EXIT_OK

try:
    code = main()
except SystemExit as e:
    code = EXIT_OK if e.code in (0, None) else EXIT_NOT_PWNED
sys.exit(code)
