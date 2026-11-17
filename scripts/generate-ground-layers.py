#!/usr/bin/env python3
"""Compile original H3M is_ground flags, including instance-specific overrides.

Used to restore drawing layers when loading pre-0.48 progress. New maps and
saves retain the original flag on each individual object.
"""
import collections,json,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
defaults=[];instances=[];sources={}
for p in sorted((ROOT/'assets/demo/scenarios').glob('*.json')):
    data=json.loads(p.read_text());scenario=data['id'];flags=collections.defaultdict(set)
    for t in data['templates']:flags[t['def'].lower()].add(bool(t['is_ground']))
    for sprite,values in flags.items():
        if values=={True}:defaults.append((scenario,sprite))
    for o in data['objects']:
        t=data['templates'][o['template_idx']];sprite=t['def'].lower()
        if len(flags[sprite])>1 and t['is_ground']:
            c=o['coordinates'];instances.append((scenario,sprite,c['x'],c['y'],c['z']))
    sources[str(p.relative_to(ROOT))]=hashlib.sha256(p.read_bytes()).hexdigest()
quote=lambda s:json.dumps(s,ensure_ascii=False)
text='// Generated from original H3M template is_ground fields.\n'
text+='static constexpr std::pair<std::string_view,std::string_view> GroundDefaults[]={\n'+''.join('{'+quote(m)+','+quote(s)+'},\n' for m,s in sorted(set(defaults)))+'};\n'
text+='static constexpr std::tuple<std::string_view,std::string_view,int,int,int> GroundInstances[]={\n'+''.join('{'+quote(m)+','+quote(s)+f',{x},{y},{z}'+'},\n' for m,s,x,y,z in sorted(set(instances)))+'};\n'
(ROOT/'native/generated/ground_layers.inc').write_text(text)
out=ROOT/'references/hero-performance-0.48';out.mkdir(exist_ok=True)
(out/'ground-layer-provenance.json').write_text(json.dumps({'sources':sources,'spriteDefaults':len(set(defaults)),'positionSpecificOverrides':len(set(instances))},indent=2)+'\n')
print('Ground defaults:',len(set(defaults)),'instance overrides:',len(set(instances)))
