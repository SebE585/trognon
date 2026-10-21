"""tools/go.sh decides from boot.sh's exit code only: 0 go on, 10 pwn again (at most 6), anything else stop."""
import os, shutil, subprocess, tempfile, unittest

TOOLS = os.path.dirname(os.path.abspath(__file__))

FAKE_BOOT = '#!/bin/sh\ncode=$(head -1 "$SEQ"); sed -e 1d "$SEQ" > "$SEQ.n"; mv "$SEQ.n" "$SEQ"; echo "fake boot -> $code"; exit "$code"\n'

def run(seq):
    d = tempfile.mkdtemp()
    try:
        os.makedirs(os.path.join(d, 'ipwndfu'))
        shutil.copy(os.path.join(TOOLS, 'go.sh'), d)
        with open(os.path.join(d, 'ipwndfu', 'require_3gs.py'), 'w') as f: f.write('import sys; sys.exit(0)\n')
        for name, body in (('boot.sh', FAKE_BOOT), ('send.sh', '#!/bin/sh\necho SEND\n')):
            with open(os.path.join(d, name), 'w') as f: f.write(body)
            os.chmod(os.path.join(d, name), 0o755)
        with open(os.path.join(d, 'seq'), 'w') as f: f.write('\n'.join(str(c) for c in seq) + '\n')
        env = dict(os.environ, SEQ=os.path.join(d, 'seq'), TMPDIR=d)
        p = subprocess.run(['sh', os.path.join(d, 'go.sh'), 'x.bin'], env=env, capture_output=True, text=True, timeout=30)
        return p.returncode, p.stdout.count('fake boot'), p.stdout.count('SEND')
    finally:
        shutil.rmtree(d)

class GoSh(unittest.TestCase):
    def test_booted_first_time(self): self.assertEqual(run([0]), (0, 1, 1))
    def test_not_pwned_twice_then_ok(self): self.assertEqual(run([10, 10, 0]), (0, 3, 1))
    def test_fatal_stops_at_once(self): self.assertEqual(run([11]), (11, 1, 0))
    def test_other_code_is_passed_through(self): self.assertEqual(run([3]), (3, 1, 0))
    def test_gives_up_after_six(self): self.assertEqual(run([10] * 7), (1, 6, 0))

if __name__ == '__main__':
    unittest.main()
