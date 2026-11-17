#!/usr/bin/env python3
"""Map geometry and palette animation derived from verified original resources.

No images are inferred from web screenshots. Original canvas, crop, indexed
pixels and object visit masks remain the authority for sprite placement.
"""
import hashlib
import json
from pathlib import Path
from h3_resources import Def, chunk
from original_resources import OriginalResources

ROOT = Path(__file__).resolve().parents[1]
resources = OriginalResources(ROOT)
manifest = json.loads((ROOT / 'assets/demo/manifest.json').read_text())
table = resources.read('objects.txt')
templates, creatures = {}, {}
for line in table.decode('latin1').splitlines()[1:]:
    cells = line.split()
    if len(cells) != 9:
        continue
    name, blocked, visits = cells[:3]
    key = name.lower()
    if key not in manifest['def']:
        continue
    visit = next(([-(i % 8), -(i // 8)]
                  for i, value in enumerate(visits) if value == '1'), [0, 0])
    templates.setdefault(key, {'visitOffset': visit, 'blockedMask': blocked,
                              'visitMask': visits, 'objectType': int(cells[5]),
                              'subtype': int(cells[6]), 'priority': int(cells[8])})
    if int(cells[5]) == 54:
        creatures.setdefault(cells[6], key)

# These are indexed-palette ranges, not substituted painted water frames.
# Format/range references are recorded in references/map-review-0.31/.
ranges = {'watrtl.def': [(229, 12), (242, 12)],
          'clrrvr.def': [(183, 12), (195, 6)]}
cycles = {}
for name, spans in ranges.items():
    phases = []
    for phase in range(12):
        pages = []
        for page_id, page in enumerate(manifest['def'][name]['pages']):
            original = (ROOT / page['file']).read_bytes()
            result, at = bytearray(original[:8]), 8
            while at < len(original):
                length = int.from_bytes(original[at:at + 4], 'big')
                kind = original[at + 4:at + 8]
                payload = original[at + 8:at + 8 + length]
                if kind == b'PLTE':
                    palette = bytearray(payload)
                    for start, count in spans:
                        for i in range(count):
                            source = start + (i + phase) % count
                            palette[(start+i)*3:(start+i+1)*3] = payload[source*3:(source+1)*3]
                    payload = bytes(palette)
                result.extend(chunk(kind, payload))
                at += 12 + length
            file = Path('assets/demo/map-palettes') / name[:-4] / f'{phase}-{page_id}.png'
            (ROOT / file).parent.mkdir(parents=True, exist_ok=True)
            (ROOT / file).write_bytes(result)
            pages.append(str(file))
        phases.append(pages)
    cycles[name] = phases

# Match the original transparent shapes to connected edge samples. This is an
# independent visual selector, not a copied map-editor connectivity table.
connections = {}
samples = [(1, 1), (16, 0), (30, 1), (31, 16), (30, 30), (16, 31), (1, 30), (0, 16)]
for name in ('clrrvr.def', 'dirtrd.def'):
    definition = Def(resources.read(name))
    _, alpha = definition.display_palette()
    candidates = []
    for index, f in enumerate(definition.groups[0]):
        pixels = definition.pixels(f)
        for transform in range(4):
            opacity = []
            for sx, sy in samples:
                x, y = (31-sx if transform & 1 else sx)-f.left, (31-sy if transform & 2 else sy)-f.top
                opacity.append(alpha[pixels[y*f.width+x]] if 0 <= x < f.width and 0 <= y < f.height else 0)
            candidates.append((index, bool(transform & 1), bool(transform & 2), opacity))
    matches = []
    for mask in range(256):
        best = min(candidates, key=lambda c: sum((c[3][i]-(255 if mask & (1 << i) else 0))**2*(4 if i%2 else 1) for i in range(8)))
        matches.append({'frame': best[0], 'flipX': best[1], 'flipY': best[2]})
    connections[name] = matches

data = {'schema': 1, 'objectTableSource': resources.source('objects.txt'),
        'objectTableSha256': hashlib.sha256(table).hexdigest(),
        'templates': templates, 'creatures': creatures,
        'paletteStepMilliseconds': 180, 'paletteRanges': ranges, 'paletteCycles': cycles,
        'connectionFrames': connections}
(ROOT / 'assets/demo/adventure.json').write_text(json.dumps(data, separators=(',', ':'))+'\n')
print(f'Generated {len(templates)} original object templates, {len(creatures)} creature mappings and 24 palette phases')
