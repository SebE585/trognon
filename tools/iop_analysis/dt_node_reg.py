#!/usr/bin/env python3
"""Print a device-tree node's own `reg` property, decoded as (address, size)
pairs, and the CPU-visible address each one translates to under `/device-tree/
arm-io`'s own `ranges` rule.

Self-contained: takes the decrypted device tree as an explicit argument, no
hardcoded path. Never commit the device tree itself (Apple's, not ours) --
re-extract it from the owner's IPSW, see
docs/research/2026-10-16-wifi-iop-architecture.md for the method.

Usage:
    python3 dt_node_reg.py DEVICETREE.bin NODE_NAME

Example (what this project ran on 2026-10-16 to find the IOP's second
register window):
    python3 dt_node_reg.py /tmp/trognon_dt/devicetree_decrypted.bin iop

Expected output on this device's device tree (verified 2026-10-16):
    /device-tree/arm-io ranges: [{'child': '0x0', 'parent': '0x80000000', 'size': '0x40000000'}, {'child': '0x40000000', 'parent': '0x40000000', 'size': '0x40000000'}]
    /device-tree/arm-io/iop reg:
      pair 0: child=0x6300000 size=0x1000 -> cpu=0x86300000
      pair 1: child=0x3f300000 size=0x1000 -> cpu=0xbf300000
"""
import struct
import sys


def read_prop(data, off):
    name = data[off:off + 32].rstrip(b"\x00").decode("ascii", "replace")
    off += 32
    length = struct.unpack_from("<I", data, off)[0]
    real_len = length & 0x7FFFFFFF
    off += 4
    value = data[off:off + real_len]
    pad = (4 - (real_len % 4)) % 4
    off += real_len + pad
    return name, value, off


def read_node(data, off, path, target_name, found):
    n_props, n_children = struct.unpack_from("<II", data, off)
    off += 8
    props = {}
    for _ in range(n_props):
        name, value, off = read_prop(data, off)
        props[name] = value
    name = props.get("name", b"?").rstrip(b"\x00").decode("ascii", "replace")
    full = f"{path}/{name}"
    if name == "arm-io" and "ranges" in props:
        found["ranges"] = parse_ranges(props["ranges"])
        found["ranges_path"] = full
    if name == target_name:
        found.setdefault("nodes", []).append((full, props.get("reg", b"")))
    for _ in range(n_children):
        off = read_node(data, off, full, target_name, found)
    return off


def parse_ranges(raw):
    words = [struct.unpack_from("<I", raw, i)[0] for i in range(0, len(raw) - 3, 4)]
    return [(words[i], words[i + 1], words[i + 2]) for i in range(0, len(words) - 2, 3)]


def translate(addr, ranges):
    for child, parent, size in ranges:
        if child <= addr < child + size:
            return addr - child + parent
    return None  # outside every declared range -- do not guess


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        sys.exit(2)
    dt_path, target_name = sys.argv[1], sys.argv[2]
    data = open(dt_path, "rb").read()
    found = {}
    read_node(data, 0, "", target_name, found)

    ranges = found.get("ranges")
    if ranges:
        print(f"{found['ranges_path']} ranges:", [
            {"child": hex(c), "parent": hex(p), "size": hex(s)} for c, p, s in ranges
        ])
    else:
        print("WARNING: no /device-tree/arm-io ranges property found -- cannot translate addresses")

    for full, reg in found.get("nodes", []):
        print(f"{full} reg:")
        if not reg:
            print("  (no reg property)")
            continue
        words = [struct.unpack_from("<I", reg, i)[0] for i in range(0, len(reg) - 3, 4)]
        for i in range(0, len(words) - 1, 2):
            addr, size = words[i], words[i + 1]
            cpu = translate(addr, ranges) if ranges else None
            cpu_s = hex(cpu) if cpu is not None else "UNTRANSLATED (outside declared ranges)"
            print(f"  pair {i // 2}: child={hex(addr)} size={hex(size)} -> cpu={cpu_s}")

    if not found.get("nodes"):
        print(f"no node named {target_name!r} found")
        sys.exit(1)


if __name__ == "__main__":
    main()
