#!/usr/bin/env python3
"""Generate original class primary skills and names from verified demo tables."""
import csv
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'references/siege-rules-0.46'
def rows(name):
    return list(csv.reader((SOURCE/name).read_text(encoding='cp1252').splitlines(), delimiter='\t'))
classes = rows('hctraits.txt')
start = next(i for i, row in enumerate(classes) if row and row[0] == 'Knight')
primary = [[int(v) for v in row[2:6]] for row in classes[start:start+18]]
names = [row[0] for row in rows('hotraits.txt')[2:] if row and row[0]]
assert len(primary) == 18 and len(names) == 156
target = ROOT / 'native/generated/hero_defaults.inc'
target.write_text('const std::array<std::array<int,4>,18> HeroPrimary={{'+','.join('{'+','.join(map(str,row))+'}' for row in primary)+'}};\nconst char* HeroNames[]={'+','.join(json.dumps(name) for name in names)+'};\n')
(SOURCE/'hero-defaults-provenance.json').write_text(json.dumps({
    'source': 'Previously verified official Heroes III demo resource tables',
    'scope': 'Primary class statistics and English names; secondary skills and specialties are incomplete',
    'files': {name: hashlib.sha256((SOURCE/name).read_bytes()).hexdigest() for name in ['hctraits.txt','hotraits.txt']},
    'generatedSHA256': hashlib.sha256(target.read_bytes()).hexdigest(),
}, indent=2)+'\n')
print('Generated 18 classes and 156 names')
