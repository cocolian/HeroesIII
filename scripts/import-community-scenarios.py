#!/usr/bin/env python3
"""Convert verified H3M downloads into bounded runtime scenario data.

The MIT-licensed h3mtxt build tool decodes object/property records; original
seven-byte terrain records are independently checked by our terrain reader.
No demo-map coordinates, obstacles or encounters are substituted here.
"""
import hashlib
import json
import re
import subprocess
from pathlib import Path
from community_terrain import verified_terrain, scenario_source

ROOT = Path(__file__).resolve().parents[1]
REFERENCE = ROOT/'references/h3m-import-0.46'
PARSER = ROOT/'build/h3mtxt/src/h3mtxt/h3mtxt'
OUT = ROOT/'assets/demo/scenarios'
COMMENTS = re.compile(r'"(?:\\.|[^"\\])*"|//[^\n]*|/\*[\s\S]*?\*/')

def decode_json(path,encoding):
    text = path.read_bytes().decode('latin1')
    text = COMMENTS.sub(lambda m:m[0] if m[0].startswith('"') else '',text)
    def strings(value):
        if isinstance(value,str):
            try:
                raw=value.encode('latin1')
            except UnicodeEncodeError:
                return value
            for codec in ['utf-8',encoding,'cp1252']:
                try:return raw.decode(codec)
                except UnicodeDecodeError:pass
            return raw.decode(encoding,errors='replace')
        if isinstance(value,list):return [strings(v) for v in value]
        if isinstance(value,dict):return {k:strings(v) for k,v in value.items()}
        return value
    return strings(json.loads(text))

def build():
    catalog=json.loads((ROOT/'maps/community/catalog.json').read_text())
    OUT.mkdir(parents=True,exist_ok=True)
    audit=[]
    for group in catalog['maps']:
        for index,item in enumerate(group['mapFiles']):
            relative=Path(item['file'])
            if relative.is_absolute() or '..' in relative.parts or relative.suffix.lower()!='.h3m':raise ValueError('Unsafe map path')
            source=ROOT/relative
            if hashlib.sha256(source.read_bytes()).hexdigest()!=item['sha256']:raise ValueError('Map checksum mismatch')
            key=relative.parent.name+'-'+str(index+1)
            raw=REFERENCE/'decoded'/(key+'.json')
            stamp=raw.with_suffix('.source.json')
            cached=json.loads(stamp.read_text()) if stamp.exists() else {}
            if not raw.exists() or cached.get('sourceSha256')!=item['sha256'] or cached.get('decodedSha256')!=hashlib.sha256(raw.read_bytes()).hexdigest():
                raw.parent.mkdir(parents=True,exist_ok=True)
                subprocess.run([str(PARSER),str(scenario_source(item)),str(raw)],check=True,stdout=subprocess.DEVNULL)
                stamp.write_text(json.dumps({'sourceSha256':item['sha256'],'decodedSha256':hashlib.sha256(raw.read_bytes()).hexdigest()},indent=2)+'\n')
            encoding='cp1251' if 'RUS' in item['originalArchiveMember'] else 'gb18030' if 'CHI' in item['originalArchiveMember'] or key.startswith('ira-the-mage') else 'cp1251' if key.startswith('dead-and-buried') else 'cp1252'
            decoded=decode_json(raw,encoding)
            terrain=verified_terrain(item)
            assert terrain['size']==decoded['basic_info']['map_size']
            assert len(terrain['tiles'])==terrain['size']**2*terrain['levels']
            result={'schema':1,'id':key,'title':group['title'],'variant':Path(item['originalArchiveMember']).name,
                    'sourceSha256':item['sha256'],'width':terrain['size'],'height':terrain['size'],
                    'levels':terrain['levels'],'tiles':terrain['tiles'],'players':decoded['players'],
                    'additionalInfo':decoded['additional_info'],'templates':decoded['objects_templates'],
                    'objects':decoded['objects'],'timedEvents':decoded['timed_events'],
                    'description':decoded['basic_info']['description'],
                    'spellNames':list(decoded['additional_info']['disabled_spells'])[:70]}
            (OUT/(key+'.json')).write_text(json.dumps(result,ensure_ascii=False,separators=(',',':'))+'\n')
            humans=[k for k,v in decoded['players'].items() if v['can_be_human']]
            types={}
            for o in decoded['objects']:
                cls=decoded['objects_templates'][o['template_idx']]['object_class'];types[cls]=types.get(cls,0)+1
            audit.append({'id':key,'width':terrain['size'],'levels':terrain['levels'],
                          'objects':len(decoded['objects']),'templates':len(decoded['objects_templates']),
                          'humans':humans,'classes':types,'sourceSha256':item['sha256']})
    (REFERENCE/'conversion-audit.json').write_text(json.dumps(audit,ensure_ascii=False,indent=2)+'\n')
    print('Imported',len(audit),'real H3M scenario datasets')
    return audit

if __name__=='__main__':build()
