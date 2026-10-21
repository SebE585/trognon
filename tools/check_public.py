#!/usr/bin/env python3
"""Fail if the public repository tracks something it must not publish.

    python3 -I tools/check_public.py

Trognon is a public repository. A .gitignore or .git/info/exclude does not stop a forced `git add -f`, a rename or a
copy from another repository, so the rules are enforced here, on what git actually tracks, and by CI.

Rules (each finding names the rule):
  private-path    files that stay local (any case): CLAUDE*.md, CODEX*.md, MEMORY.md, TODO.md, AGENTS.md, docs/journal/, .claude/, .codex/, .env, settings.local.json
  binary          a binary file whose extension is not in ALLOWED_BINARY, or a firmware/image extension in any form
  size            a tracked file over MAX_BYTES
  personal-path   an absolute home path of a person (/Users/<name>/, /home/<name>/)
  hardware-id     an ECID, a serial number (SRNM) or an IMEI that identifies a device
  secret          a private key, or a token with a well-known prefix
  private-name    the name of a private project (never cited in a public file)
  trailer         a commit message with a machine trailer (Co-Authored-By, Claude-Session)

A deliberate exception goes in ALLOWED, with the reason, so that a reviewer sees it. Nothing is skipped silently.
"""
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Binary formats that may be tracked. Adding one is a reviewed change to this file.
ALLOWED_BINARY = {'.png', '.jpg', '.jpeg', '.ico', '.woff2'}
# Never tracked, whatever the content: Apple firmware and device images, built payloads.
FORBIDDEN_EXT = {'.ipsw', '.img3', '.img4', '.dfu', '.dmg', '.im4p', '.im4m', '.bbfw', '.ipa', '.elf', '.bin', '.o'}
MAX_BYTES = 2_000_000

PRIVATE_PATH = re.compile(
    r'(^|/)((CLAUDE|CODEX)[^/]*\.md|MEMORY\.md|TODO\.md|AGENTS\.md|settings\.local\.json)$'
    r'|^docs/journal/|(^|/)\.(claude|codex)/|(^|/)\.env($|\.)', re.I)

PERSONAL_PATH = re.compile(r'/Users/[A-Za-z0-9._-]+/|/home/(?!runner/)[a-z][a-z0-9._-]*/')
HARDWARE_ID = re.compile(r'ECID:[0-9A-Fa-f]{12,}|SRNM:\[[A-Za-z0-9]{8,}\]|IMEI:\[[0-9]{14,}\]')
LEAK_PATTERNS = re.compile(
    r'-----BEGIN [A-Z ]*PRIVATE KEY-----|ghp_[A-Za-z0-9]{36}|AKIA[0-9A-Z]{16}|xox[baprs]-[A-Za-z0-9-]{10,}|sk-[A-Za-z0-9]{32,}')
PRIVATE_NAME = re.compile(r'\b(redevice|pepin)\b', re.I)
TRAILER = re.compile(r'^(Co-Authored-By|Claude-Session):', re.I | re.M)

LABEL = {'personal-path': 'a person\'s home path', 'hardware-id': 'a device identifier', 'secret': 'a secret',
         'private-name': 'the name of a private project'}

# This file and its test hold the patterns above as data; they are not scanned for content.
SELF = {'tools/check_public.py', 'tools/test_check_public.py'}

# (rule, path): reason. A path listed here is exempt from that one rule only.
ALLOWED = {
    ('hardware-id', 'tests/test_serial.c'):
        'USB serial strings copied from theapplewiki.com (a public page) as parser fixtures, not from our devices',
    ('hardware-id', 'include/trognon/serial.h'):
        'documentation example of the serial format with a placeholder ECID, not a device',
}


def check_path(path):
    """Findings for a tracked path, from its name alone."""
    out = []
    if PRIVATE_PATH.search(path):
        out.append((path, 'private-path', 'this file stays local'))
    ext = os.path.splitext(path)[1].lower()
    if ext in FORBIDDEN_EXT:
        out.append((path, 'binary', 'extension %s is never tracked (firmware, image or built payload)' % ext))
    return out


def check_blob(path, data):
    """Findings for a tracked file's bytes: binary format, size, then content if it is text."""
    out = []
    ext = os.path.splitext(path)[1].lower()
    if len(data) > MAX_BYTES:
        out.append((path, 'size', '%d bytes, over %d' % (len(data), MAX_BYTES)))
    if b'\0' in data[:8192]:
        if ext not in ALLOWED_BINARY and ext not in FORBIDDEN_EXT:
            out.append((path, 'binary', 'binary file with extension %r, allowed: %s' % (ext, ', '.join(sorted(ALLOWED_BINARY)))))
        return out
    if path in SELF:
        return out
    text = data.decode('utf-8', errors='replace')
    for rule, rx in (('personal-path', PERSONAL_PATH), ('hardware-id', HARDWARE_ID), ('secret', LEAK_PATTERNS), ('private-name', PRIVATE_NAME)):
        m = rx.search(text)
        if m:
            line = text.count('\n', 0, m.start()) + 1
            # Category and place only. The matched text is never printed: for a secret or a device id, a CI log is public.
            out.append((path, rule, 'line %d (%s)' % (line, LABEL[rule])))
    return out


def check_message(commit, message):
    return [(commit, 'trailer', 'machine trailer in the commit message')] if TRAILER.search(message) else []


def apply_allowed(findings, allowed=None):
    allowed = ALLOWED if allowed is None else allowed
    return [f for f in findings if (f[1], f[0]) not in allowed]


def git(*args):
    return subprocess.run(['git', '-C', ROOT] + list(args), capture_output=True, check=True).stdout


def main():
    findings = []
    # Tracked files AND untracked files that are not ignored: what `git add -A` would put in the next commit, so a new file
    # is checked before it is added, not after.
    for raw in git('ls-files', '-z', '--cached', '--others', '--exclude-standard').split(b'\0'):
        if not raw:
            continue
        path = raw.decode()
        findings += check_path(path)
        full = os.path.join(ROOT, path)
        if os.path.isfile(full) and not os.path.islink(full):
            with open(full, 'rb') as f:
                findings += check_blob(path, f.read())
    try:
        log = git('log', '--format=%H%x00%B%x01').decode(errors='replace')
        for entry in filter(None, log.split('\x01')):
            sha, _, msg = entry.strip('\n').partition('\0')
            findings += check_message(sha[:10], msg)
    except subprocess.CalledProcessError:
        pass   # no history available (a source archive): the file rules still ran
    findings = apply_allowed(findings)
    for path, rule, detail in findings:
        print('check_public: %-13s %s: %s' % (rule, path, detail))
    if findings:
        print('check_public: %d finding(s). A file that must stay local: `git rm --cached` it. A deliberate exception: ALLOWED in tools/check_public.py, with the reason.' % len(findings))
        return 1
    print('check_public: nothing private in what is tracked or would be added (%d allowed exception(s))' % len(ALLOWED))
    return 0


if __name__ == '__main__':
    sys.exit(main())
