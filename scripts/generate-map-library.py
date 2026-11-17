#!/usr/bin/env python3
"""Expose verified downloads, native scenario datasets and terrain previews to the game UI."""
import hashlib
import json
from pathlib import Path
from h3_resources import png
from community_terrain import verified_terrain

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT/'assets/demo/map-previews'
COLORS = [(116,82,48),(210,182,120),(64,114,48),(192,207,213),
          (95,104,55),(103,103,105),(117,76,45),(61,45,43),(35,77,135),(24,24,24)]

def thumbnail(tiles,size,level):
    pixels = bytearray()
    for y in range(288):
        for x in range(288):
            terrain,view,river,river_view,road,road_view,flags = tiles[level*size*size+(y*size//288)*size+x*size//288]
            color = COLORS[terrain]
            if river:
                color = (62,117,158)
            if road:
                color = (160,127,76)
            pixels.extend(color)
    return png(288,288,pixels)

def build():
    catalog = json.loads((ROOT/'maps/community/catalog.json').read_text())
    OUT.mkdir(parents=True,exist_ok=True)
    entries = [{'id':'border-practice','title':'边境练习地图','variant':'九种起始城镇',
                'playable':True,'width':36,'height':28,'levels':1,'author':'本项目练习图',
                'description':'发展城镇、占领矿山并击败黑鸦堡守军。',
                'file':'','previews':[],'source':'','reason':''}]
    for group in catalog['maps']:
        for i,item in enumerate(group['mapFiles']):
            relative = Path(item['file'])
            if relative.is_absolute() or '..' in relative.parts or relative.suffix.lower()!='.h3m':
                raise ValueError('Unsafe map path')
            path = ROOT/relative
            if hashlib.sha256(path.read_bytes()).hexdigest()!=item['sha256']:
                raise ValueError('Downloaded map checksum mismatch: '+str(relative))
            data = verified_terrain(item)
            assert [data['size']]*2==item['mapSize'] and data['levels']==item['levels']
            previews = []
            for level in range(data['levels']):
                preview = OUT/f'{relative.parent.name}-{i+1}-{level}.png'
                preview.write_bytes(thumbnail(data['tiles'],data['size'],level))
                previews.append(str(preview.relative_to(ROOT)))
            scenario=ROOT/'assets/demo/scenarios'/(relative.parent.name+'-'+str(i+1)+'.json')
            scenario_data=json.loads(scenario.read_text()) if scenario.is_file() else None
            if scenario_data and scenario_data['sourceSha256']!=item['sha256']:raise ValueError('Scenario source checksum mismatch')
            entries.append({'id':relative.parent.name+'-'+str(i+1),'title':group['title'],
                'variant':Path(item['originalArchiveMember']).name,'playable':scenario_data is not None,
                'mode':'original-exploration','scenarioFile':str(scenario.relative_to(ROOT)),
                'width':data['size'],'height':data['size'],'levels':data['levels'],
                'author':group['author'],'description':group['notes'],'file':str(relative),
                'sha256':item['sha256'],'source':group['source'],'previews':previews,
                'reason':'可探索、拾取、占矿与战斗；部分剧情、物件及英雄能力尚未完整支持。'})
    result = {'schema':1,'collectedOn':catalog['collectedOn'],'entries':entries,
              'previewScope':'Terrain overview; original object records and initial state are available in original-exploration mode.'}
    target = ROOT/'assets/demo/map-library.json'
    target.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
    print('Indexed',len(entries)-1,'downloaded maps and',sum(len(e['previews']) for e in entries),'terrain previews')
    return result

if __name__=='__main__':
    build()
