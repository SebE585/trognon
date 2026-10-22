#!/usr/bin/env python3
"""
One-off script, not part of ipwndfu: decrypts an img3/dfu file (an iBSS, say) with the pwned device's own GID
key, entirely read-only on the chip side. Never touches firmware: the file in and the file out are both given
on the command line, at the location the caller chose outside this repo -- this script holds none, the same way
tools/boot.sh never carries a 3GS iBSS. Trognon's rule stands: Apple firmware is extracted from the owner's own
IPSW at install time and never committed; a decrypted one is not committed either.

Usage: decrypt_image3.py CPID INPUT OUTPUT
    CPID    the expected chip (8920 3GS, 8922, 8930 iPhone 4) -- limera1n.py lists a DeviceConfig for each
    INPUT   an encrypted img3/dfu file, e.g. extracted with:
              unzip -p SOME_Restore.ipsw Firmware/dfu/iBSS.XXXXap.RELEASE.dfu > INPUT
    OUTPUT  where the decrypted image3 is written

Chain, all through dfuexec.py/image3.py, unmodified: newImage3(decrypted=True) -> getDecryptedPayload() ->
dfuexec.decrypt_keybag() -> device.aes(keybag, AES_DECRYPT, AES_GID_KEY) -> execute(). Nothing is ever written
to the device; only the decrypted KEYBAG (32 bytes) comes back over USB -- the GID key itself never leaves the
chip. ipwndfu caches it once decrypted, keyed by the keybag's own bytes, in aes-keys/S5L<CPID>-firmware (inside
the ipwndfu checkout, not this repo): the GID key is shared by every chip of that model, so the same firmware
decrypts without touching the device again.

Patches dfu.acquire_device() to reuse a single libusb backend/context for the whole run (see
tools/ipwndfu/try_limera1n.py for why, and docs/journal/2026-10-22.md for the data): without it, the pwn step
and the first execute() call are both unreliable on this host. Even with the fix, a SECOND active command on an
already-pwned session reliably degrades (same symptom seen on the 3GS and the iPhone 4, 2026-10-21/22): run this
once per fresh DFU cycle (a real Power+Home re-entry, not just a USB replug), not in a loop.
"""
import os, sys, time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
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


def main(argv):
    if len(argv) != 3:
        print('usage: decrypt_image3.py CPID INPUT OUTPUT')
        return 2
    cpid, in_path, out_path = argv

    d = dfu.acquire_device(timeout=5, match='CPID:%s' % cpid, fatal=False)
    if d is None:
        print('ERROR: no CPID:%s device in DFU mode.' % cpid)
        return 1
    already_pwned = 'PWND:[' in (d.serial_number or '')
    dfu.release_device(d)
    if not already_pwned:
        import limera1n
        limera1n.exploit()

    import image3
    with open(in_path, 'rb') as f:
        data = f.read()
    print('Read %d bytes from %s' % (len(data), in_path))

    img = image3.Image3(data)
    keybag = img.getKeybag()
    if keybag is None:
        print('ERROR: no KBAG tag in this image3 -- nothing to decrypt (already decrypted, or not an image3).')
        return 1

    decrypted = img.newImage3(decrypted=True)
    with open(out_path, 'wb') as f:
        f.write(decrypted)
    print('Decrypted image3 written: %d bytes -> %s' % (len(decrypted), out_path))
    return 0


sys.exit(main(sys.argv[1:]))
