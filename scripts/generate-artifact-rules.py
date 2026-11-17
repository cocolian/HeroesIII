#!/usr/bin/env python3
"""Extract the recorded primary/movement/spell-immunity artifact rule facts."""
import json
import csv
import io
import re
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
source=ROOT/'references/h3m-import-0.44/vcmi-artifacts.json'
pattern=r'"(?:\\.|[^"\\])*"|//[^\n]*|/\*[\s\S]*?\*/'
data=json.loads(re.sub(pattern,lambda m:m[0] if m[0].startswith('"') else '',source.read_text()))
names=json.loads((ROOT/'assets/demo/scenarios/wayfarer-1.json').read_text())['spellNames']
rules=[]
for artifact in data.values():
    if not isinstance(artifact,dict) or 'index' not in artifact:continue
    stats=[0]*4;movement=0;immunities=[]
    for bonus in artifact.get('bonuses',{}).values():
        if not isinstance(bonus,dict):continue
        kind,sub=bonus.get('type'),bonus.get('subtype')
        primary={'attack':0,'defence':1,'defense':1,'spellpower':2,'spellPower':2,'knowledge':3}
        if kind=='PRIMARY_SKILL' and sub in primary:stats[primary[sub]]+=bonus.get('val',0)
        if kind=='MOVEMENT' and sub=='heroMovementLand':movement+=bonus.get('val',0)
        if kind=='SPELL_IMMUNITY' and isinstance(sub,str):
            key=re.sub(r'(?<!^)(?=[A-Z])','_',sub).lower()
            if key in names:immunities.append(names.index(key))
    if any(stats) or movement or immunities:rules.append((artifact['index'],stats,movement,immunities))
target=ROOT/'native/generated/artifact_rules.inc'
target.write_text('// Rule facts from references/h3m-import-0.44/vcmi-artifacts.json.\n'+''.join(' {%d,{%s},%d,{%s}},\n'%(i,','.join(map(str,s)),m,','.join(map(str,a))) for i,s,m,a in sorted(rules)))
print('Generated',len(rules),'basic artifact rules')
rows=list(csv.reader(io.StringIO((source.parent/'artraits.txt').read_bytes().decode('cp1252')),delimiter='\t'))
slots={12:'feet',13:'left_ring',14:'right_ring',15:'torso',16:'left_hand',17:'right_hand',18:'neck',19:'shoulders',20:'head',7:'misc5',8:'misc4',9:'misc3',10:'misc2',11:'misc1'}
catalog=[]
for i,row in enumerate(rows[2:143]):
    catalog.append(' {%d,%s,%s,{%s}},\n'%(i,json.dumps(row[0]),json.dumps(row[21]),','.join(json.dumps(slot) for column,slot in slots.items() if row[column].strip()=='x')))
(ROOT/'native/generated/artifact_catalog.inc').write_text('// Names, equipment slots and rarities from verified Mac Complete demo ARTRAITS.TXT.\n'+''.join(catalog))
