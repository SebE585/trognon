"""
Extra iBSS patches for the pre-init clock-gate snapshot (STAGE 48). RAM only, like boot_ibss_only.py.

The iBSS 4.3.5 clock-gate wrapper at 0x8400dd58 begins with `push {r7, lr}; add r7, sp, #0` (4 bytes). It is called
twice (from 0x8400deec, then 0x8400e124). The first instruction pair is replaced by a branch to a stub placed in the
zero padding at 0x84000120, which copies the 52 gate words (0xBF100078 + 4 * i) into a stash at 0x410A0000, runs
the two displaced instructions and returns to 0x8400dd5c. The stash holds a magic word, a call counter and one
snapshot per call (the first two are kept). Nothing is written to the gate registers; the stub only reads them.

Used by boot_ibss_only.py when TROGNON_GATE_SNAP=1. Offsets are relative to the decrypted iBSS body (image minus
its 64-byte header), the same base as the existing patches; the original bytes are checked before patching.
"""
from keystone import Ks, KS_ARCH_ARM, KS_MODE_THUMB

BASE = 0x84000000
HOOK = 0xDD58            # wrapper entry
HOOK_BYTES = bytes.fromhex('80b500af')   # push {r7, lr}; add r7, sp, #0
RETURN = BASE + 0xDD5C
CAVE = 0x120             # zero padding 0xFC..0x200 in the 4.3.5 body
CAVE_LEN = 0xD0
STASH = 0x410A0000
MAGIC = 0x50534E47       # 'GNSP' little-endian
GATES = 0xBF100078
NGATES = 52
SNAP_SIZE = NGATES * 4
SNAP_OFF = 0x10
KEEP = 2


def _asm(src, addr):
    ks = Ks(KS_ARCH_ARM, KS_MODE_THUMB)
    code, _ = ks.asm(src, addr)
    return bytes(code)


def stub():
    src = f"""
    push {{r0-r5}}
    movw r0, #{STASH & 0xFFFF}
    movt r0, #{STASH >> 16}
    movw r1, #{MAGIC & 0xFFFF}
    movt r1, #{MAGIC >> 16}
    ldr r2, [r0]
    cmp r2, r1
    beq have
    str r1, [r0]
    movs r2, #0
    str r2, [r0, #4]
    have:
    ldr r2, [r0, #4]
    cmp r2, #{KEEP}
    bhs count
    movs r3, #{SNAP_SIZE}
    mul r3, r2, r3
    adds r3, r3, r0
    adds r3, #{SNAP_OFF}
    movw r4, #{GATES & 0xFFFF}
    movt r4, #{GATES >> 16}
    movs r5, #{NGATES}
    copy:
    ldr r1, [r4], #4
    str r1, [r3], #4
    subs r5, #1
    bne copy
    count:
    adds r2, #1
    str r2, [r0, #4]
    pop {{r0-r5}}
    push {{r7, lr}}
    add r7, sp, #0
    b.w #{RETURN}
    """
    return _asm(src, BASE + CAVE)


def patches(body):
    """Return the patch list for `body` after checking what is being replaced."""
    assert body[HOOK:HOOK + 4] == HOOK_BYTES, 'gate snapshot: wrapper prologue is not the 4.3.5 one'
    code = stub()
    assert len(code) <= CAVE_LEN, 'gate snapshot: stub too large for the padding'
    assert body[CAVE:CAVE + CAVE_LEN] == bytes(CAVE_LEN), 'gate snapshot: padding is not zero'
    hook = _asm(f'b.w #{BASE + CAVE}', BASE + HOOK)
    assert len(hook) == 4
    return [(CAVE, code), (HOOK, hook)]


if __name__ == '__main__':
    import sys
    from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB
    body = open(sys.argv[1], 'rb').read()
    md = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
    for off, data in patches(body):
        print('patch at %#x (%d bytes)' % (BASE + off, len(data)))
        for i in md.disasm(data, BASE + off):
            print('  %#x  %s %s' % (i.address, i.mnemonic, i.op_str))
