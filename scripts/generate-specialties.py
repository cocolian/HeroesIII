#!/usr/bin/env python3
"""Compile factual classic hero specialties and SoD recipes, without engine code."""
import json,re
from pathlib import Path
R=Path(__file__).resolve().parents[1]
def load(p):
 return json.loads(re.sub(r'"(?:\\.|[^"\\])*"|//[^\n]*|/\*[\s\S]*?\*/',lambda m:m[0] if m[0].startswith('"') else '',p.read_text()))
units={u['key']:u['id'] for u in load(R/'data/native/creatures.json')['units']};units.update(ballista=141)
spells={u['key']:u['id'] for u in load(R/'data/native/spells.json')['spells']}
records={}
for p in (R/'references/secondary-skills-0.51').glob('*.json'):
 if p.name in ['sources.json','tree.json'] or p.name.startswith('portraits'):continue
 for key,row in load(p).items():
  if isinstance(row,dict) and 'index' in row:records[row['index']]=(key,row)
resources={'gold':0,'wood':1,'ore':2,'mercury':3,'sulfur':4,'crystal':5,'gems':6}
rows=[]
for i in range(156):
 key,row=records[i];s=row.get('specialty',{});kind=0;unit=spell=resource=-1;amount=0;bonus=[0]*4;targets=[];upgrade=-1
 if 'creature'in s:kind=1;unit=units[s['creature']];amount=s.get('creatureLevel',0)
 if 'spellScalingPercentage'in s:kind=2;spell=spells[s['spellScalingPercentage']]
 if 'spellFixedAdditive'in s:kind=3;spell=spells[s['spellFixedAdditive']]
 if i==9:kind=4;spell=spells['bless']
 if i in [3,122]:kind=12
 if i==136:kind=10;spell=spells['fireWall'];amount=100
 if i==138:kind=11;spell=spells['magicArrow'];amount=50
 if i==145:kind=9
 base=s.get('base',{});bonuses=s.get('bonuses',{})
 def limited(limits):
  out=[]
  for l in limits if isinstance(limits,list) else [limits]:
   if not isinstance(l,dict):continue
   if l.get('creature')in units:
    u=units[l['creature']];out.append(u)
    if l.get('includeUpgrades') and u<126 and u%2==0:out.append(u+1)
   elif l.get('bonusType')=='DRAGON_NATURE':out.extend([26,27,68,69,82,83,137,138,139,140])
  return sorted(set(out))
 if base.get('type')=='SPECIAL_UPGRADE':
  kind=8;upgrade=units[base['addInfo']]
  for b in bonuses.values():targets+=limited(b.get('limiters',[]))
 else:
  for b in bonuses.values():
   b={**base,**b};t=b.get('type')
   if t=='SPECIAL_FIXED_VALUE_ENCHANT':kind=3;spell=spells[b['subtype']]
   if t=='GENERATE_RESOURCE':kind=6;resource=resources[b['subtype']];amount=b['val']
   if t in ['PRIMARY_SKILL','CREATURE_DAMAGE','STACKS_SPEED']:
    kind=5 if i!=144 else 7;targets+=limited(b.get('limiters',[]))
    stat=0 if b.get('subtype')=='attack'else 1 if b.get('subtype')=='defence'else 2 if t=='CREATURE_DAMAGE'else 3
    bonus[stat]=b.get('val',0)
 rows.append({'id':i,'key':key,'kind':kind,'unit':unit,'spell':spell,'resource':resource,'amount':amount,'bonus':bonus,'targets':sorted(set(targets)),'upgrade':upgrade,'source':s})
(R/'native/generated/hero_specialties.inc').write_text('\n'.join(' {'+','.join(map(str,[x['kind'],x['unit'],x['spell'],x['resource'],x['amount']]))+',{'+','.join(map(str,x['bonus']))+'},{'+','.join(map(str,x['targets']))+'},'+str(x['upgrade'])+'},'for x in rows)+'\n')
(R/'data/native/hero-specialties.json').write_text(json.dumps({'scope':'Classic Complete 156 heroes; facts compiled from pinned hero records','heroes':rows},ensure_ascii=False,indent=2)+'\n')
art=load(R/'references/h3m-import-0.44/vcmi-artifacts.json');ids={k:v['index']for k,v in art.items() if isinstance(v,dict)and'index'in v}
recipes=[{'id':v['index'],'components':[ids[c]for c in v['components']],'bonuses':v.get('bonuses',v.get('instanceBonuses',{}))}for v in art.values()if isinstance(v,dict)and'components'in v]
assert len(recipes)==12
(R/'native/generated/artifact_combinations.inc').write_text('\n'.join(' {'+str(x['id'])+',{'+','.join(map(str,x['components']))+'}},'for x in recipes)+'\n')
(R/'data/native/artifact-combinations.json').write_text(json.dumps({'scope':'Twelve original Shadow of Death combinations, including occupied component slots','recipes':recipes},ensure_ascii=False,indent=2)+'\n')
print('Generated 156 hero specialties and 12 combination recipes')
