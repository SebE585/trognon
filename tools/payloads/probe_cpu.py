"""Build a read-only CPU-state probe (Thumb, loaded at 0x41000000).

Publishes, as 6 hex words via the iBSS env_set() (0x84011798, 4.3.5 n88ap):
  MIDR  SCTLR  CPSR  TTBR0  DACR  SP-at-entry
Only registers that exist on every Cortex-A8 (no VBAR/ACTLR: they may be
UNDEFINED depending on the security state, and an undefined instruction here
resets the device). Nothing is written. Read back with: getenv trognon

Usage: python3 probe_cpu.py OUT.bin   (needs keystone-engine, capstone)
"""
import sys, keystone, capstone
ENV_SET = 0x84011799
src = """
push {r4-r7, lr}
sub sp, #100
add r4, sp, #72
mrc p15, 0, r0, c0, c0, 0
str r0, [r4]
mrc p15, 0, r0, c1, c0, 0
str r0, [r4, #4]
mrs r0, cpsr
str r0, [r4, #8]
mrc p15, 0, r0, c2, c0, 0
str r0, [r4, #12]
mrc p15, 0, r0, c3, c0, 0
str r0, [r4, #16]
add r0, sp, #120
str r0, [r4, #20]
mov r5, r4
mov r4, sp
movs r6, #6
loop:
ldr r1, [r5]
adds r5, #4
movs r2, #8
hexloop:
lsrs r3, r1, #28
cmp r3, #10
bge alpha
adds r3, #48
b store
alpha:
adds r3, #87
store:
strb r3, [r4]
adds r4, #1
lsls r1, r1, #4
subs r2, #1
bne hexloop
movs r3, #32
strb r3, [r4]
adds r4, #1
subs r6, #1
bne loop
subs r4, #1
movs r3, #0
strb r3, [r4]
adr r0, name
mov r1, sp
movs r2, #1
adr r3, envset
ldr r3, [r3]
blx r3
add sp, #100
pop {r4-r7, pc}
.align 2
envset:
.word %d
name:
.asciz "trognon"
""" % ENV_SET
ks = keystone.Ks(keystone.KS_ARCH_ARM, keystone.KS_MODE_THUMB)
code, n = ks.asm(src)
code = bytes(code)
open(sys.argv[1], 'wb').write(code)
print(len(code), 'bytes')
md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
for i in md.disasm(code, 0x41000000):
    if i.mnemonic in ('mrc', 'mrs', 'add', 'sub', 'push', 'pop', 'adr', 'blx') or i.address < 0x30: print(hex(i.address), i.mnemonic, i.op_str)
