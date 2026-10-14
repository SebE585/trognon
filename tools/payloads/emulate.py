"""Offline check of a payload with Unicorn: fake peripherals, env_set stub.

Usage: python3 emulate.py PAYLOAD.bin   (needs unicorn)
"""
import sys, struct
from unicorn import *
from unicorn.arm_const import *
code = open(sys.argv[1],'rb').read()
mu = Uc(UC_ARCH_ARM, UC_MODE_THUMB)
mu.mem_map(0x41000000, 0x1000); mu.mem_write(0x41000000, code)
mu.mem_map(0x42000000, 0x10000)   # stack
mu.mem_map(0xBF100000, 0x1000)    # MIU/clock
mu.mem_map(0x85400000, 0x1000)    # CLCD
mu.mem_map(0x84011000, 0x1000)    # env_set stub
mu.mem_write(0xBF100000, struct.pack('<I', 0x12345602))
for g,v in ((0x2F,0xABCDEF0F),(0x2B,0x0000000F),(0x1B,0x80000000)):
    mu.mem_write(0xBF100078+4*g, struct.pack('<I', v))
mu.mem_write(0x85400000, struct.pack('<I', 0xDEADBEEF))
mu.mem_write(0x84011798, b'\x70\x47'*2)  # bx lr
calls=[]
def hook(uc, addr, size, _):
    if addr == 0x84011798:
        r0,r1,r2 = (uc.reg_read(r) for r in (UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2))
        rd=lambda p:bytes(iter(lambda q=[p]:(uc.mem_read(q[0],1)[0:1] if not q.__setitem__(0,q[0]+1) else b''),b'')) 
        def cs(p):
            s=b''
            while True:
                c=bytes(uc.mem_read(p,1)); 
                if c==b'\0': return s
                s+=c; p+=1
        calls.append((cs(r0),cs(r1),r2, uc.reg_read(UC_ARM_REG_SP)))
mu.hook_add(UC_HOOK_CODE, hook, begin=0x84011798, end=0x84011798)
sp=0x42008000
mu.reg_write(UC_ARM_REG_SP, sp)
sentinel=0x42000001
mu.reg_write(UC_ARM_REG_LR, sentinel)
for r in (UC_ARM_REG_R4,UC_ARM_REG_R5,UC_ARM_REG_R6,UC_ARM_REG_R7): mu.reg_write(r, 0x1111)
mu.mem_write(0x42000000, b'\x70\x47')
mu.emu_start(0x41000001, 0x42000000, count=2000)
print(calls)
print('sp restored:', mu.reg_read(UC_ARM_REG_SP)==sp, 'r4-r7 restored:', [hex(mu.reg_read(r)) for r in (UC_ARM_REG_R4,UC_ARM_REG_R5,UC_ARM_REG_R6,UC_ARM_REG_R7)])
print('sp at call %% 8 = %d' % (calls[0][3] % 8))
