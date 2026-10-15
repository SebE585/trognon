"""Generate the evidence tables included by docs/{en,fr}/08-evidence.md from evidence/*/manifest.json.

The table is generated, never written by hand: it can only list bundles that exist.
Usage: python3 tools/evidence_index.py        (also: make evidence-index)
"""
import glob, json, os
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
URL = 'https://github.com/SebE585/trognon/tree/main/evidence/'
rows = []
for d in sorted(glob.glob(os.path.join(REPO, 'evidence', '*'))):
    mf = os.path.join(d, 'manifest.json')
    if not os.path.isfile(mf):
        continue
    m = json.load(open(mf))
    rows.append((os.path.basename(d), m))
HEAD = {
    'en': ('Bundle', 'Payload', 'SHA-256', 'Commit', 'Nonce', 'Screen rows'),
    'fr': ('Dossier', 'Payload', 'SHA-256', 'Commit', 'Nonce', 'Lignes d\'écran'),
}
for lang, head in HEAD.items():
    out = ['| ' + ' | '.join(head) + ' |', '|' + '---|' * len(head)]
    for name, m in rows:
        ok = 'confirmed' if (m.get('nonce_confirmed') and lang == 'en') else ('confirmé' if m.get('nonce_confirmed') else 'NOT confirmed')
        dirty = ' (dirty tree)' if m['git'].get('dirty') else ''
        out.append('| [`%s`](%s%s) | `%s` | `%s` | `%s`%s | %s | %d |' % (
            name, URL, name, m['payload'], m['sha256'][:12], m['git']['commit'][:8], dirty, ok, m.get('screen_rows', 0)))
    path = os.path.join(REPO, 'docs', 'snippets', 'evidence-table.%s.md' % lang)
    open(path, 'w', encoding='utf-8').write('\n'.join(out) + '\n')
print('%d bundles indexed' % len(rows))
