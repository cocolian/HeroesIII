#!/usr/bin/env python3
"""Calibrate original terrain geometry with H3M view IDs and original pixels.

The format reference identifies the native/transition frame ranges. This
selector is independently derived from the verified DEF pixels, rather than
copying an editor's pattern-matching implementation. It never repaints tiles.
"""
import hashlib
import itertools
import json
from collections import Counter
from pathlib import Path
from h3_resources import Def
from original_resources import OriginalResources
from calibrate_terrain import calibrate

ROOT = Path(__file__).resolve().parents[1]
resources = OriginalResources(ROOT)
names = ['grastl.def', 'dirttl.def', 'sandtl.def', 'watrtl.def']
definitions = [Def(resources.read(name)) for name in names]
native = [range(49, 57), range(21, 29), range(0, 12), range(20, 33)]
histograms = []
for definition, frames in zip(definitions, native):
    colors = Counter()
    for index in frames:
        for pixel in definition.pixels(definition.groups[0][index]):
            colors[definition.palette[pixel*3:pixel*3+3]] += 1
    total = sum(colors.values())
    histograms.append({color: count/total for color, count in colors.items()})

def material(color):
    probabilities = [hist.get(color, 0) for hist in histograms]
    if not max(probabilities):
        probabilities = [max((weight / (1 + sum((a-b)**2 for a, b in zip(color, other)))
                              for other, weight in hist.items()), default=0)
                         for hist in histograms]
    return max(range(4), key=lambda i: probabilities[i])

# NW, N, NE, E, SE, S, SW, W, in original tile coordinates.
samples = [(1, 1), (16, 1), (30, 1), (30, 16),
           (30, 30), (16, 30), (1, 30), (1, 16)]
tables, audit = {}, {}
for terrain, (name, definition) in enumerate(zip(names, definitions)):
    if terrain == 2:
        continue
    external = [1, 2] if terrain == 0 else [2]
    allowed = (list(range(49)) + list(range(73, 79)) if terrain == 0
               else list(range(21)) + [45] if terrain == 1 else list(range(20)))
    candidates = []
    classes = [material(definition.palette[p*3:p*3+3]) for p in range(256)]
    for index in allowed:
        f = definition.groups[0][index]
        pixels = definition.pixels(f)
        for transform in range(4):
            profiles = []
            for sx, sy in samples:
                colors = Counter()
                for dy in range(-1, 2):
                    for dx in range(-1, 2):
                        x = 31-(sx+dx) if transform & 1 else sx+dx
                        y = 31-(sy+dy) if transform & 2 else sy+dy
                        colors[classes[pixels[(y-f.top)*f.width+x-f.left]]] += 1
                profiles.append([colors[i]/9 for i in range(4)])
            # Canonical straight/corner frame geometry is part of the DEF
            # format. Use it as a hard priority: texture flecks at one edge
            # sample must never turn a straight boundary into a diagonal.
            bank = 2 if terrain in (1, 3) or 20 <= index < 40 else 1
            local = index-20 if terrain == 0 and 20 <= index < 40 else index
            sides = ([bank, 0, 0, bank] if local < 4 or 16 <= local < 18
                     else [0, 0, 0, bank] if local < 8
                     else [bank, 0, 0, 0] if local < 12
                     else [0, 0, 0, 0])  # N,E,S,W; 0 means native.
            if terrain == 0 and index >= 40:
                sides = {40:[0,0,0,0],41:[0,0,0,0],42:[0,0,0,0],
                         43:[0,1,0,0],44:[0,0,1,0],45:[0,1,0,0],
                         46:[0,0,1,0],47:[0,2,0,0],48:[0,0,2,0],
                         73:[0,1,1,0],74:[0,2,2,0],75:[1,0,0,1],
                         76:[2,0,0,2],77:[0,1,1,0],78:[0,1,1,0]}[index]
            if transform & 1:
                sides[1], sides[3] = sides[3], sides[1]
            if transform & 2:
                sides[0], sides[2] = sides[2], sides[0]
            sides = [terrain if side == 0 else side for side in sides]
            candidates.append((index, bool(transform & 1), bool(transform & 2), profiles, sides))
    table = []
    base = len(external)+1
    # Least significant digit is the NW neighbour, matching the runtime.
    for code in range(base**8):
        value = code
        desired = []
        for _ in range(8):
            digit = value % base; value //= base
            desired.append(terrain if digit == 0 else external[digit-1])
        if code == 0:
            table.append([[index, False, False] for index in native[terrain]])
            continue
        weights = [1.0 if i % 2 == 0 else 5.0 for i in range(8)]
        for corner in range(0, 8, 2):
            sides = [desired[(corner-1) % 8], desired[(corner+1) % 8]]
            borders = [kind for kind in sides if kind != terrain]
            if borders:
                desired[corner] = borders[0] if len(set(borders)) == 1 else desired[corner]
                weights[corner] = 0.25
        ranked = sorted((20*sum(c[4][side] != desired[2*side+1] for side in range(4))
                         + sum((1-c[3][i][desired[i]])*weights[i] for i in range(8)), c[:3])
                        for c in candidates)
        best = ranked[0][0]
        # Keep comparable native alternatives for deterministic texture variety.
        choices = [list(c) for cost, c in ranked if cost <= best+0.12][:8]
        table.append(choices)
    tables[name] = table
    audit[name] = {'originalSource': resources.source(name), 'patterns': len(table),
                   'nativeFrames': list(native[terrain]), 'transitionFrames': allowed}

calibration = calibrate(ROOT, tables)
output = {'schema': 1, 'neighbourOrder': ['NW','N','NE','E','SE','S','SW','W'],
          'selection': 'original H3M transition calibration with indexed-pixel fallback', 'frames': tables,
          'calibration': calibration,
          'sources': audit}
path = ROOT/'assets/demo/terrain.json'
path.write_text(json.dumps(output, separators=(',', ':'))+'\n')
reference = ROOT/'references/map-review-0.38'
reference.mkdir(parents=True, exist_ok=True)
(reference/'source.json').write_text(json.dumps({
    'formatReference': 'https://raw.githubusercontent.com/vcmi/vcmi/develop/config/terrainViewPatterns.json',
    'formatReferenceSha256': hashlib.sha256((ROOT/'references/terrain-0.37/terrainViewPatterns.json').read_bytes()).hexdigest(),
    'h3mFormatReference': {
        'url': 'https://raw.githubusercontent.com/alexanderbelous/h3mtxt/master/doc/h3m_specification.MD',
        'sha256': hashlib.sha256((reference/'h3m_specification.MD').read_bytes()).hexdigest(),
        'readOnlyScope': 'Header and seven-byte terrain records only; no object/event decoding'},
    'generatedSha256': hashlib.sha256(path.read_bytes()).hexdigest(),
    'unmodifiedOriginalFrames': True, 'externalEngineCodeImported': False,
    'sources': audit, 'calibration': calibration}, indent=2)+'\n')
print('Generated', sum(len(t) for t in tables.values()), 'terrain neighbourhoods')
