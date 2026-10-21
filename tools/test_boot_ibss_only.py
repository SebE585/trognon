"""Exit codes of tools/ipwndfu/boot_ibss_only.py (0 booted, 10 not pwned, 11 anything else), with ipwndfu faked out."""
import os, runpy, sys, types, unittest
from unittest import mock

SCRIPT = os.path.join(os.path.dirname(__file__), 'ipwndfu', 'boot_ibss_only.py')
SER_PWND = 'CPID:8920 CPRV:15 PWND:[limera1n]'
SER_PLAIN = 'CPID:8920 CPRV:15'

class Dev:
    def __init__(self, serial): self.serial_number = serial

def run(dfu_device, boot_ibss=lambda: None):
    fake = {}
    for name in ('usb', 'usb.core', 'usb.backend', 'usb.backend.libusb1', 'utilities', 'recovery', 'dfu', 'dfuexec'):
        fake[name] = types.ModuleType(name)
    fake['usb'].core = fake['usb.core']
    fake['usb.core'].USBError = type('USBError', (Exception,), {})
    fake['usb.core'].find = lambda **k: []
    fake['recovery']._find_library = None
    fake['recovery'].acquire_device = lambda *a, **k: None
    fake['utilities'].apply_patches = lambda b, p: b
    fake['dfu'].acquire_device = lambda **k: dfu_device
    fake['dfu'].release_device = lambda d: None
    class Pwned:
        def __init__(self):
            self.identifier = SER_PWND
            self.config = types.SimpleNamespace(version='359.3.2', cpid='8920')
        def boot_ibss(self): boot_ibss()
    fake['dfuexec'].PwnedDFUDevice = Pwned
    with mock.patch.dict(sys.modules, fake), mock.patch.dict(os.environ, {}, clear=False):
        os.environ.pop('TROGNON_GATE_SNAP', None)
        try:
            runpy.run_path(SCRIPT, run_name='__main__')
        except SystemExit as e:
            return e.code
    return None

def exits(code):
    def f(): sys.exit(code)
    return f

class BootIbssOnly(unittest.TestCase):
    def test_booted(self): self.assertEqual(run(Dev(SER_PWND)), 0)
    def test_not_pwned(self): self.assertEqual(run(Dev(SER_PLAIN)), 10)
    def test_no_3gs_in_dfu(self): self.assertEqual(run(None), 11)
    def test_never_reached_recovery(self): self.assertEqual(run(Dev(SER_PWND), exits(1)), 11)

if __name__ == '__main__':
    unittest.main()
