#!/usr/bin/env python3
"""Original projectile identities, CRANIM offsets and creature-effect canvases."""
import json,re
from pathlib import Path
R=Path(__file__).resolve().parents[1]
def load(p):
 s=re.sub(r'"(?:\\.|[^"\\])*"|//[^\n]*|/\*[\s\S]*?\*/',lambda m:m[0]if m[0].startswith('"')else'',p.read_text());return json.loads(re.sub(r',\s*([}\]])',r'\1',s))
m=load(R/'assets/demo/manifest.json')['def'];u=load(R/'data/native/creatures.json')['units'];raw={}
for p in (R/'references/rules/vcmi/config/creatures').glob('*.json'):raw.update(load(p))
timings=load(R/'data/native/battle-layout.json')['creatures'];rows=[]
for unit in range(145):
 key=u[unit]['key']if unit<141 else ['ballista','firstAidTent','ammoCart','catapult'][unit-141];key={'faerieDragon':'fairieDragon'}.get(key,key)
 name=raw.get(key,{}).get('graphics',{}).get('missile',{}).get('projectile','').lower();name=name if name.endswith('.def')or not name else name+'.def'
 if unit==141:name='smbalx.def'
 if unit==144:name='smcatx.def'
 asset=m.get(name,{});frames=asset.get('groups',{}).get('0',[]);w,h=frames[0]['canvas']if frames else [0,0];t=timings[unit]if unit<len(timings)else {};rows.append({'unit':unit,'animation':name if frames else None,'frames':len(frames),'width':w,'height':h,'offsets':t.get('missileOffsets',[0]*6),'angles':t.get('missileAngles',[0]*12)})
(R/'native/generated/projectiles.inc').write_text('\n'.join(' {'+(json.dumps(x['animation'])if x['animation']else'nullptr')+','+','.join(map(str,[x['frames'],x['width'],x['height']]))+',{'+','.join(map(str,x['offsets']))+'},{'+','.join(map(str,x['angles']))+'}},'for x in rows)+'\n')
(R/'data/native/projectiles.json').write_text(json.dumps({'source':'Original CRANIM and pinned creature graphics identities','flightPixelsPerSecond':1000,'timingProvisional':True,'projectiles':rows},indent=2)+'\n')
facts=load(R/'references/video-review-0.52/creature-ability-facts.json');native=load(R/'data/native/creature-abilities.json')['customEffects'];native+=['fear'];cpp=[];effects=[]
for key in native:
 an=facts.get(key,{}).get('animation',{});options=an.get('affect',an.get('hit',[]));names=[x if isinstance(x,str)else x.get('defName','')for x in options];names=[x.lower()+(''if x.lower().endswith('.def')else'.def')for x in names if x];name=next((x for x in names if x in m),None)
 if key in ['lifeDrain','rebirth']:name='c01spe0.def'
 asset=m.get(name,{});frames=asset.get('groups',{}).get('0',[]);w,h=frames[0]['canvas']if frames else[0,0];cpp.append(' {'+(json.dumps(name)if name else'nullptr')+','+','.join(map(str,[len(frames),w,h]))+',100},');effects.append({'key':key,'animation':name,'frames':len(frames),'canvas':[w,h]})
(R/'native/generated/creature_effects.inc').write_text('\n'.join(cpp)+'\n');(R/'data/native/creature-effects.json').write_text(json.dumps({'effects':effects,'frameMs':100,'timingProvisional':True},indent=2)+'\n')
# Damage and resurrection spells use their original hit/affect sprites too.
cpp=[];effects=[]
for spell in load(R/'data/native/spells.json')['spells']:
 an=spell.get('animation',{});options=an.get('affect',an.get('hit',[]));names=[x if isinstance(x,str)else x.get('defName','')for x in options];names=[x.lower()+(''if x.lower().endswith('.def')else'.def')for x in names if x];name=next((x for x in reversed(names)if x in m),None)
 asset=m.get(name,{});frames=asset.get('groups',{}).get('0',[]);w,h=frames[0]['canvas']if frames else[0,0];cpp.append(' {'+(json.dumps(name)if name else'nullptr')+','+','.join(map(str,[len(frames),w,h]))+',100},')
 if name:effects.append({'spell':spell['id'],'animation':name,'frames':len(frames),'canvas':[w,h]})
(R/'native/generated/spell_effects.inc').write_text('\n'.join(cpp)+'\n');(R/'data/native/spell-effects.json').write_text(json.dumps({'source':'Original spell hit and affect sprite names','effects':effects,'timing':'100ms provisional; no claim of original executable frame parity'},indent=2)+'\n')
print('Generated',sum(bool(x['animation'])for x in rows),'projectiles,',len(native),'creature effects,',len(effects),'spell effects')
