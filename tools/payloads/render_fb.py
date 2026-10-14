"""Offline render of a payload's framebuffer writes with Unicorn.

Runs PAYLOAD.bin (Thumb, at 0x41000000) against a fake 320x480 RGB888
framebuffer at 0x4fd00000, stubs env_set (0x84011798), and saves the result as
a PNG. Verifies the LOGIC only: no caches, no timing, and the framebuffer
address is the one read on the device (docs/en/03-hardware-map.md).
The 1 MiB section 0x410xxxxx is mapped like on the device (payload, scratch, handler stack).
Unmapped accesses (a payload writing outside the framebuffer) are reported.

Usage: python3 render_fb.py PAYLOAD.bin OUT.png   (needs unicorn, Pillow)
"""
import sys
from unicorn import *
from unicorn.arm_const import *
from PIL import Image
FB, W, H = 0x4fd00000, 320, 480
code = open(sys.argv[1], 'rb').read()
mu = Uc(UC_ARCH_ARM, UC_MODE_THUMB)
mu.mem_map(0x41000000, 0x100000); mu.mem_write(0x41000000, code)
mu.mem_map(0x42000000, 0x10000)                 # stack + return sentinel
mu.mem_map(0x0, 0x4000)                         # vector page + a 16 KiB all-zero 'page table' at TTBR0=0
for a, v in ((0x24, 0x8400fa7c), (0x2c, 0x8400faec), (0x30, 0x8400fb28)):
    mu.mem_write(a, v.to_bytes(4, 'little'))
# Seed the fake page table (TTBR0=0): the payload's own sections in domain 0, one spare section
# (VA 0x50000000, domain 5, left unmapped here so Unicorn raises the abort the MMU would raise).
for idx, dom in ((0, 0), (0x410, 0), (0x4fd, 0), (0x840, 0), (0x500, 5)):
    mu.mem_write(idx * 4, ((dom << 5) | 2).to_bytes(4, 'little'))
# Fake peripherals, only for payloads that survey registers (harmless for the others): CLCD, GPIO, clock gates.
# GPIO is left unmapped on purpose so the survey's ABORT path is exercised.
for base in (0x85400000, 0xBF100000, 0xBF200000, 0xBF210000, 0xBF220000):
    mu.mem_map(base, 0x2000)
mu.mem_write(0x85401B14, (0x18ce0).to_bytes(4, 'little')); mu.mem_write(0x85400024, FB.to_bytes(4, 'little'))
mu.mem_map(FB, (W * H * 4 + 0xfff) & ~0xfff)
mu.mem_map(0x84011000, 0x1000); mu.mem_write(0x84011798, b'\x70\x47' * 2)
mu.mem_write(0x42000000, b'\x70\x47')
env = []
def cs(p):
    s = b''
    while (c := bytes(mu.mem_read(p, 1))) != b'\0': s += c; p += 1
    return s.decode()
def hook(uc, addr, size, _):
    env.append((cs(uc.reg_read(UC_ARM_REG_R0)), cs(uc.reg_read(UC_ARM_REG_R1))))
mu.hook_add(UC_HOOK_CODE, hook, begin=0x84011798, end=0x84011798)
mu.reg_write(UC_ARM_REG_SP, 0x42008000); mu.reg_write(UC_ARM_REG_LR, 0x42000001)
n = [0]
mu.hook_add(UC_HOOK_CODE, lambda *a: n.__setitem__(0, n[0] + 1))
# Exceptions are modelled by hand: Unicorn stops on UNDEF / unmapped access, we do the ARM exception
# entry (mode switch, banked LR/SPSR, jump to the vector) and resume emulation. The vector page holds
# the same `ldr pc,[pc,#0x18]` as the iBSS; the pointer words are whatever the payload wrote there.
for vec in (0x04, 0x0c, 0x10, 0x18):   # undef, prefetch abort, data abort, irq
    mu.mem_write(vec, (0xe59ff018).to_bytes(4, 'little'))
EXC = {UC_ERR_INSN_INVALID: ('UNDEF', 0x1b, 0x04), UC_ERR_READ_UNMAPPED: ('DABT', 0x17, 0x10),
       UC_ERR_WRITE_UNMAPPED: ('DABT', 0x17, 0x10), UC_ERR_EXCEPTION: ('PABT', 0x17, 0x0c)}   # EXCEPTION = BKPT here
# Minimal VIC0 model for the soft-IRQ payload: SOFTINT sets STATUS/RAW, SOFTINTCLEAR clears them,
# INTENCLEAR clears ENABLE bits, INTENABLE sets bits. When SOFTINT is written and the CPU has IRQs
# unmasked, the next instruction boundary takes an IRQ (entry done by hand, like the other exceptions).
V0 = 0xBF200000
irq = {'pending': False, 'taken': 0}
def rd(a): return int.from_bytes(mu.mem_read(a, 4), 'little')
def wr(a, v): mu.mem_write(a, (v & 0xffffffff).to_bytes(4, 'little'))
def vic_write(uc, access, addr, size, value, _):
    off = addr - V0
    if off == 0x018: wr(V0 + 0x000, value); wr(V0 + 0x008, value); irq['pending'] = True
    elif off == 0x01C: wr(V0 + 0x000, rd(V0) & ~value); wr(V0 + 0x008, rd(V0 + 8) & ~value); irq['pending'] = False
    elif off == 0x014: wr(V0 + 0x010, rd(V0 + 0x010) & ~value)
    elif off == 0x010: wr(V0 + 0x010, rd(V0 + 0x010) | value)
mu.hook_add(UC_HOOK_MEM_WRITE, vic_write, begin=V0, end=V0 + 0xfff)
wr(V0 + 0x010, 0x3FFA4040)       # what the iBSS leaves enabled (DEVICE, 2026-10-15)
def irq_check(uc, addr, size, _):
    if irq['pending'] and not (uc.reg_read(UC_ARM_REG_CPSR) & 0x80) and rd(V0 + 0x010) & rd(V0 + 0x000):
        irq['pending'] = False; irq['at'] = addr | (1 if uc.reg_read(UC_ARM_REG_CPSR) & 0x20 else 0); uc.emu_stop()
mu.hook_add(UC_HOOK_CODE, irq_check)
start, taken = 0x41000001, 0
while True:
    try:
        mu.emu_start(start, 0x42000000, count=300_000_000)
        if 'at' not in irq:
            break
        pc, cpsr = irq.pop('at') & ~1, mu.reg_read(UC_ARM_REG_CPSR)
        mu.reg_write(UC_ARM_REG_CPSR, (cpsr & ~0x3f) | 0x12 | 0x80)    # IRQ mode, ARM state, IRQ masked
        mu.reg_write(UC_ARM_REG_SPSR, cpsr)
        mu.reg_write(UC_ARM_REG_LR, pc + 4)
        irq['taken'] += 1
        print('irq %d: at pc=%#x -> LR=%#x, vector 0x18' % (irq['taken'], pc, pc + 4))
        start = 0x18
        continue
    except UcError as e:
        if e.errno not in EXC or taken >= 8:
            print('EMULATION STOP:', e, 'pc=%#x' % mu.reg_read(UC_ARM_REG_PC)); break
        name, mode, vec = EXC[e.errno]
        pc, cpsr = mu.reg_read(UC_ARM_REG_PC), mu.reg_read(UC_ARM_REG_CPSR)
        thumb = bool(cpsr & 0x20)
        lr = pc + (8 if name == 'DABT' else 4 if name == 'PABT' else (2 if thumb else 4))
        mu.reg_write(UC_ARM_REG_CPSR, (cpsr & ~0x3f) | mode | 0x80)   # enter mode, ARM state, IRQ masked
        mu.reg_write(UC_ARM_REG_SPSR, cpsr)
        mu.reg_write(UC_ARM_REG_LR, lr)
        taken += 1
        print('exception %d: %s at pc=%#x -> LR=%#x, vector %#x' % (taken, name, pc, lr, vec))
        start = vec
fb = bytes(mu.mem_read(FB, W * H * 4))
# word 0x00RRGGBB, little-endian in memory: bytes are B, G, R, 0
Image.frombuffer('RGBA', (W, H), fb, 'raw', 'BGRA', 0, 1).convert('RGB').save(sys.argv[2])
print('instructions:', n[0], '| env_set calls:', env)
print('vector words 24/2c/30:', ' '.join('%08x' % int.from_bytes(mu.mem_read(a, 4), 'little') for a in (0x24, 0x2c, 0x30)))
print('pixel(0,0) = %02x%02x%02x' % tuple(fb[2::-1]), '| pixel(W-1,H-1) = %02x%02x%02x' % tuple(fb[-2:-5:-1]))
