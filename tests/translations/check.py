#!/usr/bin/env python3
"""Source checks, or check receipts from the installed application's loader."""
import json
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET
here = Path(__file__).resolve().parent
expected = json.loads((here / 'expected.json').read_text())
locales = dict(en='en_US', pl='pl_PL', pt_BR='pt_BR', fr='fr_FR', de='de_DE',
               ja='ja_JP', ko='ko_KR', zh_CN='zh_CN', zh_TW='zh_TW')
failures = []
for lang, words in expected.items():
    if len(sys.argv) > 1:
        row = json.loads((Path(sys.argv[1]) / (locales[lang] + '.json')).read_text())
        for i, word in enumerate(words):
            entry = row['strings'][i]
            if entry['application'] != word:
                failures.append((lang, i, entry['application'], word))
            assert entry['context'] == ('Gui' if i == 0 else 'Mode')
            assert not entry['wrong_context']
        if lang != 'en' and not row['loaded']:
            failures.append((lang, 'catalog not loaded'))
    elif lang != 'en':
        path = here.parent.parent / 'i18n' / ('bino_' + lang + '.ts')
        if not path.exists():
            failures.append((lang, 'missing catalog'))
            continue
        root = ET.parse(path).getroot()
        for i, word in enumerate(words):
            ctx = 'Gui' if i == 0 else 'Mode'
            matches = [m for c in root.findall('context') if c.findtext('name') == ctx
                       for m in c.findall('message') if m.findtext('source') == expected['en'][i]]
            if len(matches) != 1:
                failures.append((lang, ctx, expected['en'][i], 'missing or duplicate'))
                continue
            tr = matches[0].find('translation')
            assert tr is not None and not tr.attrib.get('type'), (lang, i)
            assert tr.text == word and word.strip(), (lang, i)
            assert re.findall(r'%[Ln]?\d+|%n', word) == re.findall(r'%[Ln]?\d+|%n', expected['en'][i])
print(json.dumps({'passed': not failures, 'failures': failures}, ensure_ascii=False, indent=2))
sys.exit(bool(failures))
