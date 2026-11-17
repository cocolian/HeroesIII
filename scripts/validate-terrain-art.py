#!/usr/bin/env python3
"""Compare selected transition geometry with withheld, unmodified SoD maps."""
import hashlib
import json
from pathlib import Path
from calibrate_terrain import ASSETS, code_of, geometry, observations
from h3m_terrain import read_terrain
from h3_resources import Def, png
from original_resources import OriginalResources

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT/'references/map-review-0.38'
old = json.loads((OUT/'baseline-terrain-0.37.json').read_text())['frames']
new = json.loads((ROOT/'assets/demo/terrain.json').read_text())['frames']
records, terrain_hashes = [], set()
for slug in ['seven-lakes','the-lone-knight','jennifers-dream-journey','jedi-story']:
    path = ROOT/'maps/community'/slug/'1.h3m'
    m = read_terrain(path)
    terrain_hash = hashlib.sha256(b''.join(bytes(t) for t in m['tiles'])).hexdigest()
    if terrain_hash in terrain_hashes:
        continue
    terrain_hashes.add(terrain_hash)
    counts = {'eligibleTransitionTiles':0,'beforeMatchingGeometry':0,'afterMatchingGeometry':0}
    for kind,digits,f,position in observations(m):
        code = code_of(kind,digits)
        if code == 0:
            continue
        counts['eligibleTransitionTiles'] += 1
        for key,table in [('beforeMatchingGeometry',old),('afterMatchingGeometry',new)]:
            counts[key] += geometry(kind,f) == geometry(kind,table[ASSETS[kind]][code][0])
    assert counts['afterMatchingGeometry']/counts['eligibleTransitionTiles'] > .90, slug
    assert counts['afterMatchingGeometry'] > counts['beforeMatchingGeometry'], slug
    records.append({'file':str(path.relative_to(ROOT)),
                    'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),
                    'terrainSha256':terrain_hash,**counts})
audit = {'schema':1,'method':'Compare first selected frame geometry with stored original view ID and XY flags; texture alternatives and inert flips grouped',
         'calibrationMapsExcluded':True, 'maps':records,
         'scope':'Only interior transition tiles with dirt/sand/grass/water neighbours. Other terrain types and border tiles excluded.',
         'pixelPerfectOriginalMapParity':False,
         'remainingDifferences':'Some corner choices depend on a wider neighbourhood than 3x3.'}
(OUT/'heldout-audit.json').write_text(json.dumps(audit,indent=2)+'\n')

# An independently decoded original crop: columns are 0.37, calibrated, H3M.
# No objects/roads/rivers are shown, so terrain discontinuities stay visible.
resources = OriginalResources(ROOT)
defs = {k:Def(resources.read(v)) for k,v in {0:'dirttl.def',1:'sandtl.def',2:'grastl.def',8:'watrtl.def'}.items()}
pixels = {}
def rgb(kind, f):
    key = kind,tuple(f)
    if key in pixels:
        return pixels[key]
    definition = defs[kind]
    frame = definition.groups[0][f[0]]
    raw = definition.pixels(frame)
    result = bytearray(32*32*3)
    for y in range(32):
        for x in range(32):
            sx,sy = 31-x if f[1] else x,31-y if f[2] else y
            index = raw[(sy-frame.top)*frame.width+sx-frame.left]
            result[(y*32+x)*3:(y*32+x+1)*3] = definition.palette[index*3:index*3+3]
    pixels[key] = result
    return result

m = read_terrain(ROOT/'maps/community/seven-lakes/1.h3m')
lookup = {pos:(kind,code_of(kind,digits),f) for kind,digits,f,pos in observations(m)}
cw,ch = 16,12
best = None
for y in range(1,m['size']-ch-1,2):
    for x in range(1,m['size']-cw-1,2):
        region = [m['tiles'][yy*m['size']+xx][0] for yy in range(y,y+ch) for xx in range(x,x+cw)]
        if any(k not in defs for k in region) or len(set(region)) < 3:
            continue
        score = sum(geometry(k,f) != geometry(k,old[ASSETS[k]][c][0]) and
                    geometry(k,f) == geometry(k,new[ASSETS[k]][c][0])
                    for (xx,yy,z),(k,c,f) in lookup.items()
                    if z==0 and x<=xx<x+cw and y<=yy<y+ch and c)
        if best is None or score > best[0]:
            best = score,x,y
assert best and best[0] > 10
_,ox,oy = best
width,height = cw*32*3,ch*32
canvas = bytearray(width*height*3)
for y in range(ch):
    for x in range(cw):
        t = m['tiles'][(oy+y)*m['size']+ox+x]
        kind = t[0]
        original = [t[1],bool(t[6]&1),bool(t[6]&2)]
        if kind == 1:
            variants = [[t[1]%12,False,False]]*2+[original]
        else:
            _,code,_ = lookup[ox+x,oy+y,0]
            variants = [old[ASSETS[kind]][code][0],new[ASSETS[kind]][code][0],original]
        for panel,f in enumerate(variants):
            data = rgb(kind,f)
            for row in range(32):
                at = ((y*32+row)*width+panel*cw*32+x*32)*3
                canvas[at:at+96] = data[row*96:(row+1)*96]
(OUT/'terrain-before-after-original.png').write_bytes(png(width,height,canvas))
(OUT/'crop.json').write_text(json.dumps({'map':'maps/community/seven-lakes/1.h3m',
    'origin':[ox,oy,0],'size':[cw,ch], 'panels':['0.37','0.38 calibrated','stored H3M IDs'],
    'objectsRoadsRiversOmitted':True,'usesOriginalUnmodifiedPixels':True},indent=2)+'\n')
print('Withheld map transitions:',sum(r['eligibleTransitionTiles'] for r in records),
      'before:',sum(r['beforeMatchingGeometry'] for r in records),
      'after:',sum(r['afterMatchingGeometry'] for r in records))
