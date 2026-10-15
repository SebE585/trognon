#!/usr/bin/env python3
"""
One-off script, not part of ipwndfu: calls PwnedDFUDevice.boot_ibss() in
isolation and stops there. dfuexec.py only ever calls boot_ibss() as the
first step of flash_nor(), which goes on to permanently write NOR flash
afterward. We deliberately never call flash_nor() or anything past
boot_ibss() here, to stay RAM-only: this sends a patched iBSS over DFU
and waits for the device to reach Recovery mode, nothing more.
"""
import os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dfuexec

device = dfuexec.PwnedDFUDevice()
print('Pwned device confirmed:', device.identifier)
print('Config version:', device.config.version, 'cpid:', device.config.cpid)

device.boot_ibss()
print('boot_ibss() returned: device should now be in Recovery Mode (0x1281).')
print('Stopping here deliberately: not calling flash_nor() or anything that writes NOR.')
