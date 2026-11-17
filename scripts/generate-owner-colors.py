#!/usr/bin/env python3
"""Resolve original adventure owner markers separately from combat palettes.

Adventure objects use special index 5, not the last 32 colors: those colors
contain ordinary stone/wood in map DEFs. Indexed pixels are never modified.
"""
import hashlib, json, struct
from pathlib import Path
from h3_resources import Def, chunk
from original_resources import OriginalResources

ROOT = Path(__file__).resolve().parents[1]
resources = OriginalResources(ROOT)
manifest = json.loads((ROOT / 'assets/demo/manifest.json').read_text())
pal = resources.read('players.pal')
assert pal[:4] == b'RIFF' and pal[8:16] == b'PAL data'
version, count = struct.unpack_from('<HH', pal, 20)
assert version == 0x300 and count == 256
players = [list(pal[24+i*4:27+i*4]) for i in range(count)]
colors = {
    'blue': {'rgb': [0, 0, 255], 'paletteIndex': 1, 'heroFlag': 'af01.def'},
    'red': {'rgb': [255, 0, 0], 'paletteIndex': 0, 'heroFlag': 'af00.def'},
    'neutral': {'rgb': [128, 128, 128], 'paletteIndex': -1},
    'tan': {'rgb': [210, 180, 140], 'paletteIndex': 2, 'heroFlag': 'af02.def'},
    'green': {'rgb': [0, 255, 0], 'paletteIndex': 3, 'heroFlag': 'af03.def'},
    'orange': {'rgb': [255, 128, 0], 'paletteIndex': 4, 'heroFlag': 'af04.def'},
    'purple': {'rgb': [128, 0, 255], 'paletteIndex': 5, 'heroFlag': 'af05.def'},
    'teal': {'rgb': [0, 255, 255], 'paletteIndex': 6, 'heroFlag': 'af06.def'},
    'pink': {'rgb': [255, 128, 255], 'paletteIndex': 7, 'heroFlag': 'af07.def'},
}
towns = ['avccasx0.def', 'avcramx0.def', 'avctowx0.def', 'avcinfx0.def', 'avcnecx0.def',
         'avcdunx0.def', 'avcstrx0.def', 'avcftrx0.def', 'avchforx.def']
mines = ['avmgold0.def', 'avmsawd0.def', 'avmore0.def', 'avmalch0.def', 'avmsulf0.def', 'avmcrys0.def', 'avmgems0.def']
flaggable_sites = ['avxlths0.def']
assets = {}
for name in towns + mines + flaggable_sites + ['cmflagl.def', 'cmflagr.def']:
    definition = Def(resources.read(name))
    is_map = name in towns + mines + flaggable_sites
    entry = {'mode': 'adventure-marker-5' if is_map else 'player-palette-224-255',
             'source': resources.source(name), 'pages': {}, 'samples': []}
    if is_map:
        for frame in definition.groups[0]:
            pixels = definition.pixels(frame)
            points = [(i % frame.width, i // frame.width) for i, value in enumerate(pixels) if value == 5]
            assert points, 'Missing owner marker: ' + name
            center = (sum(x for x, y in points) / len(points), sum(y for x, y in points) / len(points))
            point = min(points, key=lambda p: (p[0]-center[0])**2 + (p[1]-center[1])**2)
            entry['samples'].append({'point': [frame.left+point[0], frame.top+point[1]], 'pixels': len(points)})
    for color, info in colors.items():
        outputs = []
        for page_id, page in enumerate(manifest['def'][name]['pages']):
            original = (ROOT / page['file']).read_bytes()
            result, at = bytearray(original[:8]), 8
            while at < len(original):
                length = int.from_bytes(original[at:at+4], 'big')
                kind, payload = original[at+4:at+8], original[at+8:at+8+length]
                if kind == b'PLTE':
                    modified = bytearray(payload)
                    if is_map:
                        modified[15:18] = bytes(info['rgb'])
                    else:
                        for index in range(32):
                            rgb = players[info['paletteIndex']*32+index] if info['paletteIndex'] >= 0 else [48+index*5]*3
                            modified[(224+index)*3:(225+index)*3] = bytes(rgb)
                    payload = modified
                if is_map and kind == b'tRNS':
                    modified = bytearray(payload);modified[5] = 255;payload = modified
                result.extend(chunk(kind, payload));at += 12 + length
            output = Path('assets/demo/owner-colors') / color / name[:-4] / f'{page_id}.png'
            (ROOT / output).parent.mkdir(parents=True, exist_ok=True)
            (ROOT / output).write_bytes(result);outputs.append(str(output))
        entry['pages'][color] = outputs
    assets[name] = entry

data = {'schema': 1, 'ownerColors': dict(zip(map(str,range(-1,8)),['neutral','blue','red','tan','green','orange','purple','teal','pink'])),
        'colors': colors, 'playersPaletteSource': resources.source('players.pal'), 'assets': assets}
(ROOT / 'assets/demo/ownership.json').write_text(json.dumps(data, separators=(',', ':'))+'\n')
reference = ROOT / 'references/ownership-0.45';reference.mkdir(exist_ok=True)
(reference / 'source.json').write_text(json.dumps({
    'originalResourcesVerified': True, 'playersPalette': resources.source('players.pal'),
    'ownershipDataSHA256': hashlib.sha256((ROOT / 'assets/demo/ownership.json').read_bytes()).hexdigest(),
    'ownerToClassicColor': data['ownerColors'],
    'formatReference': 'https://forum.vcmi.eu/t/archived-i-want-to-help/35',
    'mapMarker': 5, 'combatPlayerPaletteRange': [224, 255],
    'mapNonMarkerPixelsChanged': False, 'indexedImageDataChanged': False,
    'heroFlags': {name: resources.source(info['heroFlag']) for name,info in colors.items() if 'heroFlag' in info},
    'assets': {name: {'source': info['source'], 'mode': info['mode'], 'flagPixelCounts': [p['pixels'] for p in info['samples']]} for name, info in assets.items()},
}, ensure_ascii=False, indent=2)+'\n')
print(f'Generated ownership variants for {len(towns)} towns, {len(mines)} mines, {len(flaggable_sites)} sites and 2 battle flags')
