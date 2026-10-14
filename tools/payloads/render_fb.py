"""Offline render of a payload's framebuffer writes with Unicorn.

Runs PAYLOAD.bin (Thumb, at 0x41000000) against a fake 320x480 RGB888
framebuffer at 0x4fd00000, stubs env_set (0x84011798), and saves the result as
a PNG. Verifies the LOGIC only: no caches, no timing, and the framebuffer
address is the one read on the device (docs/journal/2026-10-15.md).
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
try:
    mu.emu_start(0x41000001, 0x42000000, count=20_000_000)
except UcError as e:
    print('EMULATION ERROR:', e, 'pc=%#x' % mu.reg_read(UC_ARM_REG_PC)); sys.exit(1)
fb = bytes(mu.mem_read(FB, W * H * 4))
# word 0x00RRGGBB, little-endian in memory: bytes are B, G, R, 0
Image.frombuffer('RGBA', (W, H), fb, 'raw', 'BGRA', 0, 1).convert('RGB').save(sys.argv[2])
print('instructions:', n[0], '| env_set calls:', env)
print('pixel(0,0) = %02x%02x%02x' % tuple(fb[2::-1]), '| pixel(W-1,H-1) = %02x%02x%02x' % tuple(fb[-2:-5:-1]))
