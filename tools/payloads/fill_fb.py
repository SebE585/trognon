"""Build a solid-colour fill of the S5L8920 CLCD window-0 framebuffer (Thumb, loaded at 0x41000000).

Writes COLOR (0x00RRGGBB, RGB888 32 bpp, assumed channel order) over
FB..FB+0x96000 (320x480x4), cleans the D-cache by MVA over the range, drains
the write buffer (dsb), then sets env 'trognon' = 'fb-filled' via the iBSS internal
env_set() (0x84011798, iBSS 4.3.5 n88ap) as proof the payload ran to the end.
FB comes from CLCD_BASE+0x24, read on the device 2026-10-15 (docs/journal/2026-10-15.md).

Usage: python3 fill_fb.py OUT.bin [COLOR_HEX [noclean]]   (default 0D32B2; needs keystone-engine, capstone)
'noclean' omits the D-cache clean loop (to test whether it is needed).
"""
import sys, keystone, capstone
FB = 0x4fd00000
WORDS = 320 * 480
LINES = WORDS * 4 // 32          # clean at 32-byte stride: safe for 32- or 64-byte lines
COLOR = int(sys.argv[2], 16) if len(sys.argv) > 2 else 0x0D32B2
NOCLEAN = len(sys.argv) > 3 and sys.argv[3] == 'noclean'
ENV_SET = 0x84011799             # 0x84011798 | Thumb bit
CLEAN = "" if NOCLEAN else """mov r0, r4
ldr r1, [r5, #16]
clean:
mcr p15, 0, r0, c7, c10, 1
adds r0, #32
subs r1, #1
bne clean"""
src = """
push {r4, r5, lr}
adr r5, data
ldr r0, [r5]
ldr r2, [r5, #4]
ldr r1, [r5, #12]
mov r4, r0
fill:
str r2, [r0]
adds r0, #4
subs r1, #1
bne fill
%s
dsb sy
adr r0, name
adr r1, val
movs r2, #1
ldr r3, [r5, #8]
blx r3
pop {r4, r5, pc}
.align 2
data:
.word %d
.word %d
.word %d
.word %d
.word %d
name:
.asciz "trognon"
val:
.asciz "fb-filled"
""" % (CLEAN, FB, COLOR, ENV_SET, WORDS, LINES)
ks = keystone.Ks(keystone.KS_ARCH_ARM, keystone.KS_MODE_THUMB)
code, n = ks.asm(src)
code = bytes(code)
open(sys.argv[1], 'wb').write(code)
print(len(code), 'bytes')
md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
for i in md.disasm(code[:0x3c], 0x41000000): print(hex(i.address), i.mnemonic, i.op_str)
