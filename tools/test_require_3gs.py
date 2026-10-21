import unittest
from unittest import mock
import importlib.util, os

spec = importlib.util.spec_from_file_location('require_3gs', os.path.join(os.path.dirname(__file__), 'ipwndfu', 'require_3gs.py'))
r = importlib.util.module_from_spec(spec); spec.loader.exec_module(r)

S3GS = 'CPID:8920 CPRV:15 CPFM:03 SCEP:03 BDID:00 SRTG:[iBoot-359.3.2]'
SIPAD = 'CPID:8940 CPRV:21 CPFM:03 SCEP:11 BDID:04 IBFL:02'

SIP4 = 'CPID:8930 CPRV:20 CPFM:03 SCEP:01 BDID:00 IBFL:00 SRTG:[iBoot-574.4]'


def run(serials, argv=None):
    with mock.patch.object(r, 'serials_darwin', return_value=serials), mock.patch.object(r, 'serials_pyusb', return_value=serials):
        return r.main(argv if argv is not None else [])

class Require3gs(unittest.TestCase):
    def test_only_3gs(self): self.assertEqual(run([S3GS]), 0)
    def test_nothing(self): self.assertEqual(run([]), 1)
    def test_only_ipad(self): self.assertEqual(run([SIPAD]), 1)
    def test_two_devices_even_with_3gs(self): self.assertEqual(run([S3GS, SIPAD]), 1)
    def test_iphone4_is_refused_unless_asked_for(self):
        self.assertEqual(run([SIP4]), 1)                    # the default stays "a 3GS": boot.sh and go.sh never pass an argument
        self.assertEqual(run([SIP4], ['8930']), 0)
    def test_asking_for_the_iphone4_refuses_a_3gs_and_an_ipad(self):
        self.assertEqual(run([S3GS], ['8930']), 1)
        self.assertEqual(run([SIPAD], ['8930']), 1)
    def test_asking_for_the_iphone4_refuses_two_devices(self):
        self.assertEqual(run([SIP4, S3GS], ['8930']), 1)
    def test_unreadable_listing(self):
        with mock.patch.object(r, 'serials_darwin', side_effect=OSError('x')), mock.patch.object(r, 'serials_pyusb', side_effect=OSError('x')):
            self.assertEqual(r.main([]), 1)

if __name__ == '__main__':
    unittest.main()
