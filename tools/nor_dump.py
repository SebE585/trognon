"""Read-only backup of the iPhone 3GS NOR (1 MiB), Python 3 port of ipwndfu's nor_dump().

Prerequisite: device in *pwned DFU* (python3 ipwndfu -p), NOT Recovery.
It only calls the SecureROM block-device read function of 'nor0'; nothing is
written to the device. The dump is dumped TWICE and the SHA-256 compared.

The NOR holds Apple firmware (iBoot/LLB) and device-specific data (serial,
MACs): it never goes in this repository. Default output is outside the repo.

Usage (from ~/tools/ipwndfu, TROGNON_LIBUSB_PATH set):
    PYTHONPATH=. python3 <repo>/tools/nor_dump.py [OUT_DIR]   (default: ./nor-backups)
"""
import hashlib, os, struct, sys, datetime
import dfuexec

PART_SIZE, PARTS = 0x20000, 8

def dump(device, read, bdev):
    out = b''
    for i in range(PARTS):
        print('  part %d/%d' % (i + 1, PARTS), flush=True)
        (_, received) = device.execute(
            struct.pack('<6I', read, bdev, device.config.load_address + 8, i * PART_SIZE, 0, PART_SIZE), PART_SIZE)
        out += received
    return out

device = dfuexec.PwnedDFUDevice()
assert device.config.cpid == '8920', 'only the 3GS (S5L8920) is supported'
(bdev, _) = device.execute(struct.pack('<2I5s', device.config.get_block_device, device.config.load_address + 12, b'nor0\x00'), 0)
assert bdev != 0, 'pointer to nor0 block device is NULL'
(read,) = struct.unpack('<I', device.read_memory(bdev + 28, 4))
assert read != 0, 'block-device read function pointer is NULL'

print('dump 1'); a = dump(device, read, bdev)
print('dump 2'); b = dump(device, read, bdev)
ha, hb = hashlib.sha256(a).hexdigest(), hashlib.sha256(b).hexdigest()
print('sha256 #1', ha); print('sha256 #2', hb)
if ha != hb:
    print('ERROR: the two dumps differ, nothing saved'); sys.exit(1)
out_dir = sys.argv[1] if len(sys.argv) > 1 else 'nor-backups'
os.makedirs(out_dir, exist_ok=True)
path = os.path.join(out_dir, 'nor-%s-%s.dump' % (device.ecid_string(), datetime.datetime.now().strftime('%Y%m%d-%H%M%S')))
fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
with os.fdopen(fd, 'wb') as f: f.write(a)
print('saved %s (%d bytes, mode 0600)' % (path, len(a)))
print('head:', a[:16].hex(), a[:4])
