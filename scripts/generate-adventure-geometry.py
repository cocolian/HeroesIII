#!/usr/bin/env python3
"""Compile verified OBJECTS.TXT masks for the core; no runtime file dependency."""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
source = ROOT/'assets/demo/adventure.json'
data = json.loads(source.read_text())
rows = ['// Generated from verified original OBJECTS.TXT.\n',
        '// adventure.json SHA256: '+hashlib.sha256(source.read_bytes()).hexdigest()+'\n']
for name,entry in sorted(data['templates'].items()):
    blocked,visits = entry['blockedMask'],entry['visitMask']
    assert len(blocked)==len(visits)==48 and set(blocked+visits)<={'0','1'}
    block = sum(1<<i for i,c in enumerate(blocked) if c=='0')
    visit = sum(1<<i for i,c in enumerate(visits) if c=='1')
    x,y = entry['visitOffset']
    rows.append(f'{{"{name}",{{{x},{y}}},0x{block:012x}ULL,0x{visit:012x}ULL}},\n')
(ROOT/'native/generated/adventure_templates.inc').write_text(''.join(rows))
(ROOT/'native/generated/adventure_creatures.inc').write_text(''.join(
    f'{{{int(key)},"{value}"}},\n' for key,value in sorted(data['creatures'].items(),key=lambda p:int(p[0]))))
print('Compiled',len(data['templates']),'original object footprints')
