#!/usr/bin/env python3
"""
One-off script, not part of ipwndfu: on an already-pwned iPhone 4 (CPID:8930), reads 4 bytes at MMIO
0xbf106000 and prints bits [15:8] -- the byte IBSS90-RST-006/-008's own SecureROM reading
(tools/iop_analysis/catalog/iBSS_n90ap_4.3.1/reset_command_table_0x84022cb4.yaml) found gates a 15-entry
device-class dispatch table (0x5270): values 0-0xe are handled, anything else makes the function return
before ever reaching the button-sentinel override that would otherwise force device-index 3 (DFU). This
script settles, with a single read, whether that override path can even be reached on THIS specific unit --
cheaper than a full boot_ibss() hardware cycle with a physical button held at the right moment.

Read-only: uses dfuexec.PwnedDFUDevice().read_memory(), the same memmove-based primitive this project's own
nor_dump.py and securerom_dump() already rely on. Writes nothing to the device, to NOR or anywhere else.

Patches dfu.acquire_device() to reuse a single libusb backend/context for the whole run, same fix and same
reason as tools/ipwndfu/try_limera1n.py (context churn made every pwn/DFU step on this host unreliable
before this fix, 2026-10-22).

2026-10-22: first attempt blocked by unusually poor USB reliability that same evening (confirmed general, not
specific to this address -- the same timeout reproduced on a known-good read, 0x0/SecureROM's own base,
already dumped successfully earlier the same session). Recovered, same session, without a physical
replug: `device.reset()` (libusb_reset_device(), see RECOVER_WITH_USB_RESET below) on the stuck device, then
a short wait for re-enumeration -- the pwned state survives this (it is a USB-level reset, not a power
cycle). Result obtained: bits[15:8] = 0x0d (13), IN the supported range -- recorded as IBSS90-RST-009.

Usage (from ~/tools/ipwndfu, TROGNON_LIBUSB_PATH set, device already pwned or about to be via limera1n):
    PYTHONPATH=. python3 <repo>/tools/ipwndfu/read_chip_revision.py
"""
import os, sys, time, struct

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import usb.core, usb.backend.libusb1
import dfu, dfuexec

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


def recover_with_usb_reset():
    """libusb_reset_device() on the current DFU device: a USB-level reset (re-enumeration), not a power
    cycle -- the device's own pwned state survives it. Found 2026-10-22: the exact "no langid"/control-
    transfer-timeout symptom that otherwise needed a physical cable replug also clears with this, with no
    need to touch the device at all. Call it once, right after a read/execute call fails with exactly that
    symptom; retry the call after it returns."""
    d = dfu.acquire_device(fatal=False)
    if d is None:
        return False
    try:
        d.reset()
    except Exception as e:
        print('recover_with_usb_reset: reset() itself failed: %r' % e)
        return False
    time.sleep(2)
    return dfu.acquire_device(timeout=5.0, fatal=False) is not None


REGISTER = 0xbf106000
SUPPORTED_MAX = 0xe  # inclusive; IBSS90-RST-006/-008's 0x5270 table has exactly 15 entries, 0..0xe


def read_chip_revision_byte():
    device = dfuexec.PwnedDFUDevice()
    if device.config.cpid != '8930':
        print('ERROR: this is CPID:%s, not the iPhone 4 (8930).' % device.config.cpid)
        sys.exit(1)
    return device.read_memory(REGISTER, 4)


try:
    data = read_chip_revision_byte()
except Exception as e:
    print('First attempt failed (%r); trying recover_with_usb_reset() before giving up.' % e)
    if not recover_with_usb_reset():
        print('ERROR: recovery failed too -- a physical cable replug is still the fallback.')
        sys.exit(1)
    data = read_chip_revision_byte()
(raw,) = struct.unpack('<I', data)
byte = (raw >> 8) & 0xff

print('raw 0x%08x = 0x%08x' % (REGISTER, raw))
print('bits[15:8] = %d (0x%02x)' % (byte, byte))
if byte <= SUPPORTED_MAX:
    print('IN the 0x5270 table\'s supported range (0-0x%x): the button-sentinel override to device-index 3 '
          '(DFU) CAN fire on this unit.' % SUPPORTED_MAX)
else:
    print('OUT of the 0x5270 table\'s supported range (0-0x%x): main() returns before ever reaching the '
          'button-sentinel override -- holding a button cannot force device-index 3 through this path on '
          'this unit.' % SUPPORTED_MAX)
