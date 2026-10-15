"""Send a Thumb payload to a 3GS already in Recovery via a patched iBSS and run it.

Prerequisite: device pwned and booted with ipwndfu's boot_ibss() (the patched iBSS maps 'run' to
0x41000001); see tools/boot.sh. Run from the ipwndfu checkout (tools/send.sh does that).

Proof of execution: a random 32-bit nonce is appended to the binary, the payload republishes it as
env variable `nonce` (hal/result.c), and this script checks it. A match proves that THIS binary, sent
THIS time, executed; a stale marker from an earlier run cannot match.

Evidence bundle (--evidence DIR): after a confirmed run the script also reads what the phone displayed
(the payload publishes its console text as variables s00..s25) and any extra variables (--vars), and
writes DIR/<UTC time>-<payload>/ with manifest.json (payload SHA-256, nonce, git commit, tool versions,
command), screen.txt (the exact text and colours the phone showed), screen.png (the console re-drawn from
that text, NOT a photo), vars.txt. The device serial's ECID is deliberately not recorded.

Usage: python3 run_payload.py PAYLOAD.bin [MARKER_VARIABLE] [--vars a,b] [--wait SECONDS] [--evidence DIR]
Exit status: 0 nonce confirmed, 3 nonce missing or different (the run is NOT proven), 1 on USB error.
The outcome is also appended to a log (default ~/trognon-runs.log, override with TROGNON_RUN_LOG).
"""
import argparse, ast, datetime, hashlib, json, os, re, subprocess, sys, time
import usb.core
import recovery

ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
ap.add_argument('payload')
ap.add_argument('marker', nargs='?', default='trognon')
ap.add_argument('--vars', default='', help='comma-separated extra env variables to read and record')
ap.add_argument('--wait', type=float, default=3.0, help='seconds to wait for the payload to finish (it may prompt a human)')
ap.add_argument('--evidence', default='', help='directory where the evidence bundle is written')
ap.add_argument('--soc', default='s5l8920')
ap.add_argument('--collect', default='', metavar='NONCE', help='do not send: only read back and verify an earlier run that used this nonce (hex, as printed by "nonce sent")')
args = ap.parse_args()

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
base = open(args.payload, 'rb').read()
nonce = bytes.fromhex(args.collect)[::-1] if args.collect else os.urandom(4)   # --collect: the nonce of an earlier send
payload = base + b'\0' * ((-len(base)) % 4) + nonce
nonce_hex = '%08X' % int.from_bytes(nonce, 'little')   # the ARM payload reads the 4 bytes as a little-endian u32
sha = hashlib.sha256(base).hexdigest()
name = os.path.basename(args.payload)

def getenv(device, var):
    # iBoot STALLs on getenv of an unset variable: report it instead of dying.
    try:
        recovery.send_command(device, 'getenv ' + var)
        return bytes(device.ctrl_transfer(0xC0, 0, 0, 0, 256, 1000)).split(b'\0')[0].decode('ascii', 'replace')
    except usb.core.USBError as e:
        return '<unset: %s>' % e

def log(outcome):
    path = os.environ.get('TROGNON_RUN_LOG', os.path.expanduser('~/trognon-runs.log'))
    try:
        with open(path, 'a') as f:
            f.write('%s %s sha256=%s nonce=%s %s\n' % (time.strftime('%Y-%m-%dT%H:%M:%S%z'), name, sha, nonce_hex, outcome))
    except OSError:
        pass

def git(*a):
    try:
        return subprocess.run(['git', '-C', REPO] + list(a), capture_output=True, text=True, timeout=10).stdout.strip()
    except Exception:
        return ''

print('payload sha256 %s (%d bytes, +4 nonce bytes)' % (sha, len(base)))
print('nonce sent     %s' % nonce_hex)
device = recovery.acquire_device()
cpid = ''
try:
    m = re.search(r'CPID:([0-9A-Fa-f]+)', device.serial_number or '')   # CPID only: the ECID stays out of the bundle
    cpid = m.group(1) if m else ''
except Exception:
    pass
print('before:', args.marker, '=', repr(getenv(device, args.marker)), '| nonce =', repr(getenv(device, 'nonce')))
if not args.collect:
    print('sending %d bytes' % len(payload), flush=True)
    try:
        recovery.send_data(device, payload)
        print('data sent, issuing run', flush=True)
        try:
            recovery.send_command(device, 'run')
            print('run issued', flush=True)
        except usb.core.USBError as e:
            if e.errno != 60:                       # a timeout means the payload is still running (a prompt for a human)
                raise
            print('run: the payload is still running (control transfer timed out), waiting for it', flush=True)
    except usb.core.USBError as e:
        print('USB error:', e)
        log('usb-error')
        sys.exit(1)

# The payload may take a while (a prompt for a human, a timeout): poll until the nonce comes back.
got, deadline = '', time.time() + max(args.wait, 0.5)
time.sleep(0.5)
while True:
    try:
        device = recovery.acquire_device()
        got = getenv(device, 'nonce')
        if got == nonce_hex or time.time() > deadline:
            break
        recovery.release_device(device)
    except usb.core.USBError:
        if time.time() > deadline:
            break
    except Exception:
        if time.time() > deadline:
            break
    time.sleep(0.5)
after = getenv(device, args.marker)
print('after: ', args.marker, '=', repr(after))
print('nonce read     %s' % got)
if got != nonce_hex:
    print('NONCE NOT CONFIRMED: the run is not proven (old payload without nonce, or it did not execute)')
    log('unconfirmed %s=%s nonce=%s' % (args.marker, after, got))
    sys.exit(3)
print('NONCE CONFIRMED: this binary executed on the device in this run')
log('confirmed %s=%s' % (args.marker, after))

if args.evidence:
    rows = []
    for r in range(26):
        v = getenv(device, 's%02d' % r)
        if v.startswith('<unset') or len(v) <= 7:     # absent, or published as an empty row
            continue
        rows.append({'row': r, 'fg': v[:6], 'text': v[7:]})
    extra = {}
    for var in [x for x in args.vars.split(',') if x]:
        extra[var] = getenv(device, var)
    recovery.release_device(device)
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ')
    out = os.path.join(args.evidence, '%s-%s' % (stamp, os.path.splitext(name)[0]))
    os.makedirs(out, exist_ok=True)
    ipw = os.environ.get('IPWNDFU', os.path.expanduser('~/tools/ipwndfu'))
    manifest = {
        'payload': name, 'sha256': sha, 'bytes': len(base), 'soc': args.soc,
        'nonce_sent': nonce_hex, 'nonce_read': got, 'nonce_confirmed': True,
        'marker': {args.marker: after}, 'variables': extra,
        'device': {'cpid': cpid or None, 'note': 'ECID deliberately not recorded'},
        'git': {'commit': git('rev-parse', 'HEAD'), 'dirty': bool(git('status', '--porcelain', '--', 'tools', 'include', 'src'))},
        'tools': {'ipwndfu_commit': subprocess.run(['git', '-C', ipw, 'rev-parse', 'HEAD'], capture_output=True, text=True).stdout.strip(),
                  'python': sys.version.split()[0], 'host': sys.platform},
        'time_utc': stamp,
        'command': ' '.join(sys.argv),
        'screen_rows': len(rows),
    }
    json.dump(manifest, open(os.path.join(out, 'manifest.json'), 'w'), indent=2)
    with open(os.path.join(out, 'screen.txt'), 'w') as f:
        for r in rows:
            f.write('%02d #%s %s\n' % (r['row'], r['fg'], r['text']))
    with open(os.path.join(out, 'vars.txt'), 'w') as f:
        for k, v in extra.items():
            f.write('%s=%s\n' % (k, v))
    try:
        from PIL import Image
        src = open(os.path.join(HERE, 'payloads', 'gen_font.py'), encoding='utf-8').read()
        tree = ast.parse(src)
        G = next(ast.literal_eval(n.value) for n in tree.body if isinstance(n, ast.Assign) and getattr(n.targets[0], 'id', '') == 'G')
        im = Image.new('RGB', (320, 480), (0x10, 0x18, 0x20))
        px = im.load()
        for x in range(320):
            for y in range(6):
                px[x, y] = (0x0D, 0x32, 0xB2)
        for r in rows:
            color = tuple(int(r['fg'][i:i + 2], 16) for i in (0, 2, 4))
            for col, ch in enumerate(r['text'][:26]):
                glyph = G.get(ch.upper())
                if not glyph:
                    continue
                for gy, line in enumerate(glyph.split('|')):
                    for gx, bit in enumerate(line):
                        if bit == '#':
                            for dy in range(2):
                                for dx in range(2):
                                    px[4 + col * 12 + gx * 2 + dx, 12 + r['row'] * 18 + gy * 2 + dy - 6] = color
        im.save(os.path.join(out, 'screen.png'))
    except ImportError:
        print('Pillow not installed: screen.png skipped')
    print('evidence bundle: %s (%d screen rows, %d variables)' % (out, len(rows), len(extra)))
else:
    recovery.release_device(device)
sys.exit(0)
