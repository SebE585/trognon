#!/usr/bin/env python3
"""
Guard run by tools/boot.sh before ipwndfu. ipwndfu's dfu.acquire_device() takes the first DFU device (PID 0x1227)
without comparing the serial, so with another device in DFU it could act on the wrong one. This refuses to go on
unless exactly one device is in DFU and it has the expected CPID: 8920 (iPhone 3GS) by default, or the one given as
the first argument (`require_3gs.py 8930` for an iPhone 4). Exit 0 = go ahead, 1 = do not. The file keeps its name
because boot.sh, go.sh and the tests call it; they never pass an argument, so they still mean "a 3GS".

On macOS the serials come from ioreg (no USB traffic: a device someone else is working on is not touched).
Elsewhere they come from pyusb.
"""
import re, subprocess, sys

DFU_PID = 0x1227

def serials_darwin():
    out = subprocess.run(['ioreg', '-p', 'IOUSB', '-l', '-w0'], capture_output=True, text=True, timeout=20).stdout
    found = []
    # one block per device: "idProduct" and "USB Serial Number" appear in the same node
    for blk in re.split(r'\n\s*[|+ ]*-o ', out):
        pid = re.search(r'"idProduct" = (\d+)', blk)
        vid = re.search(r'"idVendor" = (\d+)', blk)
        ser = re.search(r'"USB Serial Number" = "([^"]*)"', blk)
        if pid and vid and ser and int(vid.group(1)) == 0x5AC and int(pid.group(1)) == DFU_PID:
            found.append(ser.group(1))
    return found

def serials_pyusb():
    import usb.core
    return [d.serial_number or '' for d in usb.core.find(find_all=True, idVendor=0x5AC, idProduct=DFU_PID)]

NAMES = {'8920': 'iPhone 3GS', '8930': 'iPhone 4'}


def main(argv=None):
    argv = sys.argv[1:] if argv is None else argv
    cpid = argv[0] if argv else '8920'
    name = NAMES.get(cpid, 'CPID:' + cpid)
    try:
        serials = serials_darwin() if sys.platform == 'darwin' else serials_pyusb()
    except Exception as e:
        print('require_3gs: cannot list USB devices (%s); refusing.' % e)
        return 1
    if not serials:
        print('require_3gs: no device in DFU mode. Put the %s in DFU and rerun.' % name)
        return 1
    if len(serials) > 1:
        print('require_3gs: %d devices in DFU mode (%s); ambiguous, refusing. Leave only the %s in DFU.'
              % (len(serials), ', '.join(re.search(r'CPID:\w+', s).group(0) if re.search(r'CPID:\w+', s) else '?' for s in serials), name))
        return 1
    if 'CPID:%s' % cpid not in serials[0]:
        m = re.search(r'CPID:\w+', serials[0])
        print('require_3gs: the device in DFU is %s, not an %s (CPID:%s); refusing.' % (m.group(0) if m else 'unidentified', name, cpid))
        return 1
    print('require_3gs: one device in DFU, %s (CPID:%s).' % (name, cpid))
    return 0

if __name__ == '__main__':
    sys.exit(main())
