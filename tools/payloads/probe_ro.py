"""Build the read-only S5L8920 register probe (Thumb, loaded at 0x41000000).

Reads the MIU/timer base, three CLCD clock gates and the first CLCD word,
formats them as hex, and publishes them with the iBSS (4.3.5, n88ap)
internal env_set() (0x84011798, found via the setenv handler at 0x84011808).
Result is read back with: getenv trognon. No register is written.

Usage: python3 probe_ro.py OUT.bin [ADDR ...]   (needs keystone-engine, capstone)
Optional hex ADDRs replace the default list (max 7: the result must fit the 68-byte stack buffer).
"""
import sys, keystone, capstone
ENV_SET = 0x84011799  # 0x84011798 | Thumb bit
ADDRS = [0xBF100000, 0xBF100078+4*0x2F, 0xBF100078+4*0x2B, 0xBF100078+4*0x1B, 0x85400000]
if len(sys.argv) > 2:
    ADDRS = [int(a, 16) for a in sys.argv[2:]]
assert len(ADDRS) <= 7
src = """
push {r4-r7, lr}
sub sp, #68
mov r4, sp
adr r5, table
movs r6, #%d
loop:
ldr r0, [r5]
adds r5, #4
ldr r1, [r0]
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
ldr r3, [r5, #%d]
blx r3
add sp, #68
pop {r4-r7, pc}
.align 2
table:
%s
envset:
.word %d
name:
.asciz "trognon"
""" % (len(ADDRS), 0, "\n".join(".word 0x%x"%a for a in ADDRS), ENV_SET)
# r5 ends just past table == envset, so offset 0
ks = keystone.Ks(keystone.KS_ARCH_ARM, keystone.KS_MODE_THUMB)
code, n = ks.asm(src)
code = bytes(code)
open(sys.argv[1], 'wb').write(code)
print(len(code), 'bytes')
md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
for i in md.disasm(code[:0x4a], 0x41000000): print(hex(i.address), i.mnemonic, i.op_str)
