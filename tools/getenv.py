"""Read iBSS environment variables from a device in Recovery. Usage: getenv.py NAME [NAME ...]
Run from the ipwndfu checkout (see tools/send.sh). Prints NAME=value, one per line."""
import sys
import usb.core
import recovery

device = recovery.acquire_device()
for name in sys.argv[1:]:
    try:
        recovery.send_command(device, 'getenv ' + name)
        value = bytes(device.ctrl_transfer(0xC0, 0, 0, 0, 256, 1000)).split(b'\0')[0].decode('ascii', 'replace')
    except usb.core.USBError as e:
        value = '<unset: %s>' % e
    print('%s=%s' % (name, value))
recovery.release_device(device)
