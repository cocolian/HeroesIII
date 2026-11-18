#!/usr/bin/env python3
"""Compile numeric creature ability facts from the pinned classic catalog."""
import json,re
from pathlib import Path
R=Path(__file__).resolve().parents[1]
def load(p):return json.loads(re.sub(r'"(?:\\.|[^"\\])*"|//[^\n]*|/\*[\s\S]*?\*/',lambda m:m[0]if m[0].startswith('"')else'',p.read_text()))
sp={s['key']:s['id']for s in load(R/'data/native/spells.json')['spells']}
custom=['disease','bind','stoneGaze','paralyze','poison','age','acidBreath','acidBreathDamage','deathStare','lifeDrain','rebirth','thunderbolt','dispelHelpful']
sp.update({k:70+i for i,k in enumerate(custom)});sp['summonDemons']=1000
flags={'ATTACKS_ALL_ADJACENT':1,'THREE_HEADED_ATTACK':2,'TWO_HEX_ATTACK_BREATH':4,'RETURN_AFTER_STRIKE':32,'CATAPULT':64,'CATAPULT_EXTRA_SHOTS':128,'NO_WALL_PENALTY':256,'NO_TERRAIN_PENALTY':512,'VISIONS':1024,'DRAGON_NATURE':2048,'ENCHANTER':4096,'RANDOM_SPELLCASTER':8192}
records={}
for p in (R/'references/rules/vcmi/config/creatures').glob('*.json'):records.update(load(p))
rows=[]
for u in load(R/'data/native/creatures.json')['units']:
 r=records[{'faerieDragon':'fairieDragon'}.get(u['key'],u['key'])];raw=r.get('abilities',{});raw=list(raw.values())if isinstance(raw,dict)else raw;f=casts=power=duration=fear=mirror=aura=channel=critical=rebirth=0;spells=[];procs=[]
 for b in raw:
  t=b['type'];v=b.get('val',0);sub=b.get('subtype','');f|=flags.get(t,0)
  if t=='SPELL_LIKE_ATTACK':f|=8 if sub=='fireballAbility' else 16
  if t=='CASTS':casts=v
  if t=='CREATURE_SPELL_POWER':power=v
  if t=='CREATURE_ENCHANT_POWER':duration=v
  if t=='SPECIFIC_SPELL_POWER'and sub=='summonDemons':power=v
  if t=='SPELLCASTER':spells.append([sp[sub],v,b.get('addInfo',1)if isinstance(b.get('addInfo',1),int) else 1])
  if t=='ENCHANTER':spells.append([sp[sub],v,b.get('addInfo',{}).get('weight',1)])
  if t=='SPELL_AFTER_ATTACK':procs.append([sp[sub],v])
  if t=='FEARFUL':fear=max(fear,v)
  if t=='MAGIC_MIRROR':mirror=v
  if t=='SPELL_RESISTANCE_AURA':aura=v
  if t=='MANA_CHANNELING':channel=v
  if t=='DOUBLE_DAMAGE_CHANCE':critical=v
  if t=='COMBAT_EVENT_TRIGGER'and sub=='lifeDrain':procs.append([sp[sub],v])
  if t=='COMBAT_EVENT_TRIGGER'and sub=='deathStare':procs.append([sp[sub],v])
  if t=='COMBAT_EVENT_TRIGGER'and sub=='rebirth':rebirth=v
 row={'id':u['id'],'flags':f,'casts':casts,'power':power,'duration':duration,'fear':fear,'mirror':mirror,'aura':aura,'channel':channel,'critical':critical,'rebirth':rebirth,'spells':spells,'procs':procs,'facts':raw};rows.append(row)
cpp=[]
for x in rows:
 vals=[x[k]for k in ['flags','casts','power','duration','fear','mirror','aura','channel','critical','rebirth']]
 cpp.append(' {'+','.join(map(str,vals))+',{'+','.join('{'+','.join(map(str,a))+'}'for a in x['spells'])+'},{'+','.join('{'+','.join(map(str,a))+'}'for a in x['procs'])+'}},')
(R/'native/generated/creature_abilities.inc').write_text('\n'.join(cpp)+'\n')
(R/'data/native/creature-abilities.json').write_text(json.dumps({'scope':'Classic Complete 141 creatures, factual ability parameters','customEffects':custom,'units':rows},ensure_ascii=False,indent=2)+'\n')
print('Generated',len(rows),'creature ability records')
