#!/usr/bin/env python3
"""Generate original class primary skills and names from verified demo tables."""
import csv
import hashlib
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'references/siege-rules-0.46'
def rows(name):
    return list(csv.reader((SOURCE/name).read_text(encoding='cp1252').splitlines(), delimiter='\t'))
classes = rows('hctraits.txt')
start = next(i for i, row in enumerate(classes) if row and row[0] == 'Knight')
primary = [[int(v) for v in row[2:6]] for row in classes[start:start+18]]
names = [row[0] for row in rows('hotraits.txt')[2:] if row and row[0]]
units=json.loads((ROOT/'data/native/creatures.json').read_text())['units']
unitids={u['key'].lower():u['id']for u in units}
aliases={'lightcrossbowman':'archer','apprenticegremlin':'gremlin','irongolem':'ironGolem','zombie':'walkingDead','primitivelizardman':'lizardman','serpentfly':'serpentFly','serpent fly':'serpentFly','dragonfly':'serpentFly','goblin wolf rider':'goblinWolfRider','goblinwolfrider':'goblinWolfRider','goblins':'goblin','pixies':'pixie','air elementals':'airElemental','water elementals':'waterElemental','sharpshooters':'sharpshooter','enchanters':'enchanter'}
armies=[]
for row in rows('hotraits.txt')[2:]:
 if not row or not row[0]:continue
 army=[]
 for col in (1,4,7):
  key=row[col+2].lower();key=aliases.get(key,key).lower()
  unit=141 if key=='ballista'else 142 if key=='firstaidtent'else unitids[key]
  army.append([unit,int(row[col]),int(row[col+1])])
 armies.append(army)
assert len(primary) == 18 and len(names) == 156
target = ROOT / 'native/generated/hero_defaults.inc'
records={}
for path in (ROOT/'references/secondary-skills-0.51').glob('*.json'):
 if path.name.startswith('portraits') or path.name=='sources.json':continue
 text=path.read_text();data=json.loads(re.sub(r'"(?:\\.|[^"\\])*"|//[^\n]*|/\*[\s\S]*?\*/',lambda m:m[0]if m[0].startswith('"')else'',text))
 for key,value in data.items():
  if isinstance(value,dict)and'index'in value:records[value['index']]=value
skills=['offence','archery','armorer','logistics','wisdom','intelligence','mysticism','leadership','airMagic','fireMagic','waterMagic','earthMagic','luck','pathfinding','scouting','diplomacy','navigation','ballistics','eagleEye','necromancy','estates','scholar','tactics','artillery','learning','sorcery','resistance','firstAid']
spells={r['key']:r['id']for r in json.load(open(ROOT/'data/native/spells.json'))['spells']}
classes=['knight','cleric','ranger','druid','alchemist','wizard','demoniac','heretic','deathKnight','necromancer','overlord','warlock','barbarian','battleMage','beastmaster','witch','planeswalker','elementalist']
starting=[];books=[];specialties=[];classids=[]
for id in range(156):
 row=records[id];values=[0]*28
 for skill in row.get('skills',[]):values[skills.index(skill['skill'])]=['','basic','advanced','expert'].index(skill['level'])
 starting.append(values);books.append([spells[key]for key in row.get('spellbook',[])])
 specialties.append(skills.index(row['specialty']['secondary'])if row.get('specialty',{}).get('secondary')in skills else -1)
 classids.append([x.lower()for x in classes].index(row['class'].lower()))
cpp='const std::array<std::array<int,4>,18> HeroPrimary={{'+','.join('{'+','.join(map(str,row))+'}' for row in primary)+'}};\nconst char* HeroNames[]={'+','.join(json.dumps(name) for name in names)+'};\n'
cpp+='const std::array<std::array<int,28>,156> HeroStartingSkills={{'+','.join('{'+','.join(map(str,row))+'}'for row in starting)+'}};\n'
cpp+='const std::array<std::vector<int>,156> HeroStartingSpells={{'+','.join('{'+','.join(map(str,row))+'}'for row in books)+'}};\n'
cpp+='const std::array<int,156> HeroClass={'+','.join(map(str,classids))+'};\n'
cpp+='const std::array<int,156> HeroSecondarySpecialty={'+','.join(map(str,specialties))+'};\n'
cpp+='const std::array<std::array<std::array<int,3>,3>,156> HeroStartingArmies={{'+','.join('{{'+','.join('{'+','.join(map(str,s))+'}'for s in a)+'}}'for a in armies)+'}};\n'
target.write_text(cpp)
(ROOT/'data/native/hero-defaults.json').write_text(json.dumps({'sources':['references/secondary-skills-0.51/sources.json','Original HOTRAITS.TXT'], 'startingStackChances':[100,88,25],'heroes':[{'id':i,'name':names[i],'class':classids[i],'skills':starting[i],'spells':books[i],'secondarySpecialty':specialties[i],'startingArmy':armies[i]}for i in range(156)]},ensure_ascii=False,indent=2)+'\n')
(SOURCE/'hero-defaults-provenance.json').write_text(json.dumps({
    'source': 'Previously verified official Heroes III demo resource tables',
    'scope': 'Primary class statistics and English names; all 156 starting skill/spell sets and secondary specialty identities',
    'files': {name: hashlib.sha256((SOURCE/name).read_bytes()).hexdigest() for name in ['hctraits.txt','hotraits.txt']},
    'generatedSHA256': hashlib.sha256(target.read_bytes()).hexdigest(),
}, indent=2)+'\n')
print('Generated 18 classes and 156 names')
