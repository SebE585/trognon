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

With BUNDLE_ID `--decompress-only`, writes the decompressed Mach-O kernel instead (for symbol
lookups, e.g. `r2 -qc 'e asm.bits=16; is~withAddress' OUT.bin`).

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
            if i >= len(data):
                raise ValueError("lzss: input ends inside a literal")
            c = data[i]
            i += 1
            out.append(c)
            ring[r] = c
            r = (r + 1) & (N - 1)
        else:
            if i + 1 >= len(data):
                raise ValueError("lzss: input ends inside a match token")
            a, b = data[i], data[i + 1]
            i += 2
            pos, length = a | ((b & 0xF0) << 4), THRESH + (b & 0x0F)
            for k in range(length):
                c = ring[(pos + k) & (N - 1)]
                out.append(c)
                ring[r] = c
                r = (r + 1) & (N - 1)
    if len(out) < out_size:
        raise ValueError("lzss: produced %d bytes, header announced %d (truncated input?)" % (len(out), out_size))
    return bytes(out[:out_size])


def objects(d):
    """Yield (offset, size, first segment vaddr) of each ARM MH_OBJECT, with every
    load command and segment checked to lie inside the buffer; a malformed one is skipped."""
    i = 0
    while True:
        i = d.find(b"\xce\xfa\xed\xfe", i)
        if i < 0:
            return
        if i + 28 <= len(d):
            _, cpu, _, ftype, ncmds, sizeofcmds = struct.unpack_from("<6I", d, i)[0:6]
            if cpu == 12 and ftype == 1 and 0 < ncmds < 20 and i + 28 + sizeofcmds <= len(d):
                off, end, vaddr, ok = i + 28, 0, None, True
                for _ in range(ncmds):
                    if off + 8 > len(d):
                        ok = False
                        break
                    cmd, size = struct.unpack_from("<II", d, off)
                    if size < 8 or off + size > i + 28 + sizeofcmds:
                        ok = False
                        break
                    if cmd == 1:
                        # LC_SEGMENT is 56 bytes (cmd, cmdsize, 16-byte name, 8 words) followed
                        # by nsects 68-byte section records, all inside the command.
                        if size < 56:
                            ok = False
                            break
                        nsects = struct.unpack_from("<I", d, off + 48)[0]
                        if size < 56 + 68 * nsects:
                            ok = False
                            break
                        va, _, fo, fs = struct.unpack_from("<4I", d, off + 24)
                        if i + fo + fs > len(d):
                            ok = False
                            break
                        vaddr = va if vaddr is None else vaddr
                        end = max(end, fo + fs)
                    off += size
                if ok and end:
                    yield i, end, vaddr
        i += 4


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    raw = open(sys.argv[1], "rb").read()
    if raw[:8] == b"complzss":
        _, _, usize, csize = struct.unpack_from(">8sIII", raw, 0)
        if 0x180 + csize > len(raw):
            sys.exit("complzss header announces %d compressed bytes, file holds %d" % (csize, len(raw) - 0x180))
        raw = lzss(raw[0x180:0x180 + csize], usize)
    if sys.argv[2] == "--decompress-only":
        open(sys.argv[3], "wb").write(raw)
        print("decompressed kernel: %d bytes, sha256 %s" % (len(raw), hashlib.sha256(raw).hexdigest()))
        return
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
