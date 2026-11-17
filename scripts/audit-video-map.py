#!/usr/bin/env python3
"""Check the downloaded RoE map independently against its runtime conversion."""
import hashlib
import json
from pathlib import Path
from community_terrain import verified_terrain

ROOT = Path(__file__).resolve().parents[1]
def read(relative): return json.loads((ROOT/relative).read_text())
catalog = read('maps/community/catalog.json')
entry = next(item for group in catalog['maps'] for item in group['mapFiles'] if 'dead-and-buried' in item['file'])
terrain = verified_terrain(entry)
original = read('references/maps-0.46/original-roe.json')
runtime = read('assets/demo/scenarios/dead-and-buried-1.json')
assert len(original['objects']) == len(runtime['objects']) == 755
for before, after in zip(original['objects'], runtime['objects']):
    template = runtime['templates'][after['template_idx']]
    assert [before[k] for k in ['x','y','z']] == [after['coordinates'][k] for k in ['x','y','z']]
    assert before['class'] == template['object_class'] and before['subclass'] == template['object_subclass']
    assert before['def'].lower() == template['def'].lower()
assert runtime['tiles'] == [list(tile) for tile in terrain['tiles']]
assert runtime['width'] == runtime['height'] == original['size'] == 36
assert runtime['levels'] == 2 and not original['events'] and not runtime['timedEvents']
result = {'sourceFile': entry['file'], 'sourceSHA256': entry['sha256'], 'conversion': entry['conversion'],
          'size':36, 'levels':2, 'objectsChecked':755, 'coordinatesClassesSpritesUnchanged':True,
          'originalTerrainBytesPreserved':True, 'tilesChecked':len(terrain['tiles']), 'timedMapEvents':0,
          'scope':'Object identity and terrain checked; this is not complete object-property or gameplay parity',
          'converterFiles': {p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (ROOT/'references/maps-0.46/h3m2json').glob('*') if p.is_file()}}
(ROOT/'references/maps-0.46/conversion-audit.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
print(json.dumps(result,ensure_ascii=False,indent=2))
