#!/usr/bin/env python3
"""Extract one kext Mach-O object from a decrypted iOS 32-bit kernelcache.

Handles the `complzss` wrapper, finds every ARM MH_OBJECT in the prelinked
image, and writes the one whose bytes contain the given bundle identifier.
Prints the SHA-256, the size, and the virtual address of its first segment, so
the result can be checked against a catalog `tool_coverage.json`.

The kernelcache and the extracted kexts are Apple's: re-extract them from the
owner's IPSW, never commit them. Run with `python3 -I`.

Usage:
    python3 -I tools/iop_analysis/extract_kext.py KERNELCACHE.bin BUNDLE_ID OUT.bin

Example (verified 2026-10-16):
    python3 -I tools/iop_analysis/extract_kext.py /tmp/trognon_dt/kernelcache_decrypted.bin \\
        com.apple.driver.EmbeddedIOP /tmp/trognon_dt/EmbeddedIOP.kext.bin
"""
import hashlib
import struct
import sys

N, F, THRESH = 4096, 18, 3


def lzss(data, out_size):
    ring = bytearray(b" " * N)
    r, out, i, flags = N - F, bytearray(), 0, 0
    while len(out) < out_size and i < len(data):
        flags >>= 1
        if not flags & 0x100:
            flags = data[i] | 0xFF00
            i += 1
        if flags & 1:
            c = data[i]
            i += 1
            out.append(c)
            ring[r] = c
            r = (r + 1) & (N - 1)
        else:
            a, b = data[i], data[i + 1]
            i += 2
            pos, length = a | ((b & 0xF0) << 4), THRESH + (b & 0x0F)
            for k in range(length):
                c = ring[(pos + k) & (N - 1)]
                out.append(c)
                ring[r] = c
                r = (r + 1) & (N - 1)
    return bytes(out[:out_size])


def objects(d):
    i = 0
    while True:
        i = d.find(b"\xce\xfa\xed\xfe", i)
        if i < 0:
            return
        _, cpu, _, ftype, ncmds = struct.unpack_from("<5I", d, i)
        if cpu == 12 and ftype == 1 and ncmds < 20:
            off, end, vaddr = i + 28, 0, None
            for _ in range(ncmds):
                cmd, size = struct.unpack_from("<II", d, off)
                if cmd == 1:
                    va, _, fo, fs = struct.unpack_from("<4I", d, off + 24)
                    vaddr = va if vaddr is None else vaddr
                    end = max(end, fo + fs)
                off += size
            yield i, end, vaddr
        i += 4


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    raw = open(sys.argv[1], "rb").read()
    if raw[:8] == b"complzss":
        _, _, usize, csize = struct.unpack_from(">8sIII", raw, 0)
        raw = lzss(raw[0x180:0x180 + csize], usize)
    want = sys.argv[2].encode()
    hits = [(i, n, va) for i, n, va in objects(raw) if want in raw[i:i + n]]
    if len(hits) != 1:
        sys.exit("expected exactly one kext containing %r, found %d" % (want, len(hits)))
    i, n, va = hits[0]
    blob = raw[i:i + n]
    open(sys.argv[3], "wb").write(blob)
    print("kernelcache offset %#x, %d bytes, first segment vaddr %#x" % (i, n, va))
    print("sha256", hashlib.sha256(blob).hexdigest())


if __name__ == "__main__":
    main()
