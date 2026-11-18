#!/usr/bin/env python3
"""Compile numeric classic location facts; exclude third-party handlers and text."""
import csv
import hashlib
import io
import json
import re
from pathlib import Path
from original_resources import OriginalResources

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'references/object-rules-0.50'


def source(name):
    text = (SOURCE / 'config/objects' / (name + '.json')).read_text()
    return json.loads(re.sub(r',\s*([}\]])', r'\1', re.sub(r'//[^\n]*', '', text)))


units = json.loads((ROOT / 'data/native/creatures.json').read_text())['units']
by_key = {u['key']: u['id'] for u in units}
by_key['fairieDragon'] = by_key['faerieDragon']


def troops(rows):
    return [{'unit': by_key[r['type']], 'count': r['amount'],
             'upgradeChance': r.get('upgradeChance', 0)} for r in rows]


data = {'banks': {}, 'dwellings': {}, 'bankLayouts': {}, 'battleLayouts': {}}
for group in source('creatureBanks').values():
    for item in group['types'].values():
        rows = []
        for r in item['rewards']:
            chance = r.get('appearChance', {})
            rows.append({'min': chance.get('min', 0), 'max': chance.get('max', 100),
                         'guards': troops(r.get('guards', [])),
                         'resources': r.get('resources', {}),
                         'creatures': troops(r.get('creatures', [])),
                         'artifacts': [a['class'] for a in r.get('artifacts', [])]})
        data['banks'][str(group['index']) + ':' + str(item['index'])] = rows
        data['bankLayouts'][str(group['index']) + ':' + str(item['index'])] = item.get('guardsLayout', 'default')
for group in source('dwellings').values():
    for item in group['types'].values():
        data['dwellings'][str(group['index']) + ':' + str(item['index'])] = {
            'units': [[by_key[k] for k in row] for row in item['creatures']],
            'guards': troops(item.get('guards', []))}
resources = OriginalResources(ROOT)
config_text = (SOURCE / 'config/gameConfig.json').read_text()
config = json.loads(re.sub(r',\s*([}\]])', r'\1', re.sub(r'//[^\n]*', '', config_text)))
def layouts(node):
    if isinstance(node, dict):
        if 'attackerWarMachines' in node:
            data['warMachineLayouts'] = {name: node[name] for name in
                                        ['attackerWarMachines', 'defenderWarMachines']}
        for key, value in node.items():
            if key in ['creatureBankNarrow', 'creatureBankWide']:
                data['battleLayouts'][key] = {name: value[name] for name in ['attackerUnits', 'defenderUnits']}
            else:
                layouts(value)
layouts(config)
raw = resources.read('crtraits.txt')
creatures = [r for r in csv.reader(io.StringIO(raw.decode('cp1252')), delimiter='\t')
             if len(r) > 10 and r[10].strip().isdigit()]
data['aiValues'] = [int(creatures[u['originalId']][10]) for u in units]
raw_art = resources.read('artraits.txt')
arts = list(csv.reader(io.StringIO(raw_art.decode('cp1252')), delimiter='\t'))[2:]
data['artifactPrices'] = [int(r[1]) for r in arts if len(r) > 22 and r[1].strip().isdigit()]
data['artifactRarities'] = [r[21].strip() for r in arts if len(r) > 22 and r[1].strip().isdigit()]
assert len(data['artifactPrices']) >= 128
output = ROOT / 'data/native/adventure-locations.json'
output.write_text(json.dumps(data, ensure_ascii=False, indent=2) + '\n')
compiled = ROOT / 'native/generated/adventure_locations.inc'
compiled.write_text('// Generated factual numeric tables; see scripts/generate-adventure-locations.py.\n'
                    'R"H3LOC(' + json.dumps(data, separators=(',', ':')) + ')H3LOC"\n')
proof = {'sources': json.loads((SOURCE / 'sources.json').read_text()),
         'crtraits': resources.source('crtraits.txt'), 'artraits': resources.source('artraits.txt'),
         'outputSHA256': hashlib.sha256(output.read_bytes()).hexdigest()}
(SOURCE / 'numeric-facts.json').write_text(json.dumps(proof, ensure_ascii=False, indent=2) + '\n')
print('Compiled', len(data['banks']), 'bank tables and', len(data['dwellings']), 'dwelling tables')
