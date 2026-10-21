import importlib.util, os, unittest

spec = importlib.util.spec_from_file_location('check_public', os.path.join(os.path.dirname(__file__), 'check_public.py'))
cp = importlib.util.module_from_spec(spec); spec.loader.exec_module(cp)

def rules(findings): return sorted({f[1] for f in findings})

class Paths(unittest.TestCase):
    def test_private_names(self):
        for p in ('CLAUDE.md', 'sub/CLAUDE.local.md', 'MEMORY.md', 'TODO.md', 'AGENTS.md', 'docs/journal/2026-10-20.md',
                  '.claude/settings.json', 'tools/.claude/x', '.env', '.env.local', 'a/settings.local.json',
                  'Codex-session.md', 'CODEX.md', 'sub/codex.local.md', '.Codex/session.txt', '.codex/x', 'tools/.CODEX/y'):
            self.assertEqual(rules(cp.check_path(p)), ['private-path'], p)
    def test_public_names_pass(self):
        for p in ('README.md', 'docs/en/README.md', 'docs/research/TODO-notes.md', 'tools/payloads/trace.c', 'docs/journal.md'):
            self.assertEqual(cp.check_path(p), [], p)
    def test_forbidden_extensions(self):
        for p in ('fw/x.ipsw', 'a.img3', 'a.dfu', 'tools/payloads/trace.bin', 'x.elf', 'x.o'):
            self.assertIn('binary', rules(cp.check_path(p)), p)

class Blobs(unittest.TestCase):
    def test_png_allowed_unknown_binary_refused(self):
        self.assertEqual(cp.check_blob('a.png', b'\x89PNG\0\0'), [])
        self.assertEqual(rules(cp.check_blob('a.dat', b'\0\1\2')), ['binary'])
    def test_size(self):
        self.assertEqual(rules(cp.check_blob('big.txt', b'a' * (cp.MAX_BYTES + 1))), ['size'])
    def test_personal_path(self):
        self.assertEqual(rules(cp.check_blob('a.md', b'see /Users/someone/work/x')), ['personal-path'])
        self.assertEqual(rules(cp.check_blob('a.md', b'see /home/someone/x')), ['personal-path'])
        self.assertEqual(cp.check_blob('a.md', b'ci runs in /home/runner/work'), [])
    def test_hardware_ids(self):
        for s in (b'ECID:0000030A72095D2C', b'SRNM:[889224W43NP]', b'IMEI:[011950007000980]'):
            self.assertEqual(rules(cp.check_blob('a.md', s)), ['hardware-id'], s)
        self.assertEqual(cp.check_blob('a.md', b'CPID:8920 ECID is not recorded'), [])
    def test_secrets(self):
        for s in (b'-----BEGIN RSA PRIVATE KEY-----', b'ghp_' + b'a' * 36, b'AKIA' + b'A' * 16):
            self.assertEqual(rules(cp.check_blob('a.txt', s)), ['secret'], s)
    def test_private_project_names(self):
        self.assertEqual(rules(cp.check_blob('a.md', b'see ../Pepin for the rest')), ['private-name'])
        self.assertEqual(cp.check_blob('a.md', b'redeviceable is a word that merely starts with it'), [])
    def test_a_finding_never_prints_the_value_it_found(self):
        secret = 'ghp_' + 'A1b2C3d4E5f6G7h8I9j0K1l2M3n4O5p6Q7r8'
        cases = {'secret': secret, 'hardware-id': 'ECID:0000030A72095D2C', 'personal-path': '/Users/someone/work/x', 'private-name': 'pepin'}
        for rule, value in cases.items():
            findings = cp.check_blob('a.md', ('x ' + value + ' y').encode())
            self.assertEqual(rules(findings), [rule], rule)
            printed = ' '.join(str(part) for f in findings for part in f)
            self.assertNotIn(value, printed, rule)
            self.assertNotIn(value[:8], printed, rule)
            self.assertIn('line 1', printed)

    def test_the_checker_is_not_scanned_for_content(self):
        self.assertEqual(cp.check_blob('tools/check_public.py', b'/Users/someone/ pepin'), [])

class Messages(unittest.TestCase):
    def test_trailers(self):
        self.assertEqual(rules(cp.check_message('abc', 'fix\n\nCo-Authored-By: X <x@y>')), ['trailer'])
        self.assertEqual(rules(cp.check_message('abc', 'fix\n\nClaude-Session: https://x')), ['trailer'])
        self.assertEqual(cp.check_message('abc', 'fix\n\nmentions Co-Authored-By in prose, mid-line'), [])

class Exceptions(unittest.TestCase):
    def test_exception_is_per_rule_and_per_path(self):
        f = [('t.c', 'hardware-id', 'x'), ('t.c', 'secret', 'y'), ('u.c', 'hardware-id', 'z')]
        left = cp.apply_allowed(f, {('hardware-id', 't.c'): 'why'})
        self.assertEqual(left, [('t.c', 'secret', 'y'), ('u.c', 'hardware-id', 'z')])
    def test_every_exception_has_a_reason_and_is_needed(self):
        for (rule, path), reason in cp.ALLOWED.items():
            self.assertTrue(reason.strip(), (rule, path))
            with open(os.path.join(cp.ROOT, path), 'rb') as f:
                data = f.read()
            self.assertIn(rule, rules(cp.check_blob(path, data)), 'exception no longer needed: %s %s' % (rule, path))

if __name__ == '__main__':
    unittest.main()
