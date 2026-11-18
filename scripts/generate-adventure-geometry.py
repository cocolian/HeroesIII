#!/usr/bin/env python3
"""Compile verified OBJECTS.TXT masks for the core; no runtime file dependency."""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
source = ROOT/'assets/demo/adventure.json'
data = json.loads(source.read_text())
templates = dict(data['templates'])
# A few editor-only/classic structures (notably Market of Time) are absent
# from the demo OBJECTS.TXT but have complete masks in the downloaded H3M maps.
for scenario in sorted((ROOT/'assets/demo/scenarios').glob('*.json')):
    decoded = json.loads(scenario.read_text())
    for entry in decoded['templates']:
        name = entry['def'].lower()
        if name in templates:
            continue
        blocks, visits = [], []
        offset = None
        for index in range(48):
            row, col = 5-index//8, 7-index%8
            blocks.append('1' if entry['passability'][row] & (1 << col) else '0')
            visit = bool(entry['actionability'][row] & (1 << col))
            visits.append('1' if visit else '0')
            if visit and offset is None:
                offset = [col-7, row-5]
        templates[name] = {'blockedMask': ''.join(blocks), 'visitMask': ''.join(visits),
                           'visitOffset': offset or [0, 0]}
rows = ['// Generated from verified original OBJECTS.TXT.\n',
        '// adventure.json SHA256: '+hashlib.sha256(source.read_bytes()).hexdigest()+'\n']
for name,entry in sorted(templates.items()):
    blocked,visits = entry['blockedMask'],entry['visitMask']
    assert len(blocked)==len(visits)==48 and set(blocked+visits)<={'0','1'}
    block = sum(1<<i for i,c in enumerate(blocked) if c=='0')
    visit = sum(1<<i for i,c in enumerate(visits) if c=='1')
    x,y = entry['visitOffset']
    rows.append(f'{{"{name}",{{{x},{y}}},0x{block:012x}ULL,0x{visit:012x}ULL}},\n')
(ROOT/'native/generated/adventure_templates.inc').write_text(''.join(rows))
(ROOT/'native/generated/adventure_creatures.inc').write_text(''.join(
    f'{{{int(key)},"{value}"}},\n' for key,value in sorted(data['creatures'].items(),key=lambda p:int(p[0]))))
print('Compiled',len(templates),'original object footprints')
