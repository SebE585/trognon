#!/usr/bin/env python3
"""Print every property of the device-tree nodes whose path contains a substring.

Self-contained, read-only. The device tree is Apple's: pass the decrypted file
you extracted from your own IPSW (method: docs/research/2026-10-16-wifi-iop-architecture.md),
never commit it.

Usage:
    python3 -I tools/dt_dump.py DEVICETREE.bin SUBSTRING [SUBSTRING ...]

Example:
    python3 -I tools/dt_dump.py /tmp/trognon_dt/devicetree_decrypted.bin multi-touch baseband
"""
import struct
import sys


def read_prop(data, off):
    name = data[off:off + 32].rstrip(b"\x00").decode("ascii", "replace")
    length = struct.unpack_from("<I", data, off + 32)[0] & 0x7FFFFFFF
    off += 36
    value = data[off:off + length]
    return name, value, off + length + (-length % 4)


def walk(data, off, path, out):
    n_props, n_children = struct.unpack_from("<II", data, off)
    off += 8
    props = []
    for _ in range(n_props):
        name, value, off = read_prop(data, off)
        props.append((name, value))
    nm = dict(props).get("name", b"?").rstrip(b"\x00").decode("ascii", "replace")
    full = path + "/" + nm if path != "/" else "/" + nm
    out.append((full, props))
    for _ in range(n_children):
        off = walk(data, off, full, out)
    return off


def show(value):
    text = value.rstrip(b"\x00")
    if text and all(32 <= c < 127 or c == 0 for c in value) and len(value) > 1:
        return repr(text.decode("ascii").replace("\x00", "|"))
    if len(value) % 4 == 0 and 0 < len(value) <= 64:
        return "0x" + " 0x".join("%x" % w for w in struct.unpack("<%dI" % (len(value) // 4), value))
    return "%d bytes: %s" % (len(value), value[:24].hex())


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    data = open(sys.argv[1], "rb").read()
    nodes = []
    walk(data, 0, "", nodes)
    for full, props in nodes:
        if any(k in full.lower() for k in map(str.lower, sys.argv[2:])):
            print(full)
            for name, value in props:
                print("   %-28s %s" % (name, show(value)))


if __name__ == "__main__":
    main()
