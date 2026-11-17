"""Prepare original town hit regions, outlines and hall metadata.

Three demo factions contain original TZ/TO masks. Other factions use explicit
foreground-derived fallbacks, never reported as original interaction masks.
"""
import hashlib
import json
import pathlib
import re
import struct
from h3_resources import Def, numbers, take, png
from original_resources import OriginalResources

root = pathlib.Path(__file__).resolve().parents[1]
bitmap = OriginalResources(root)
sprites = {key: (bitmap, key) for key in bitmap.entries if key.endswith('.def')}
demo = json.loads((root / 'assets/demo/manifest.json').read_text())
towns = json.loads((root / 'data/native/towns.json').read_text())['towns']
result = {'schema': 1, 'towns': {}, 'missingHallBackgrounds': [], 'derivedMasks': []}


def write(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists() or path.read_bytes() != data:
        path.write_bytes(data)


def indexed(name):
    data = bitmap.read(name)
    size, width, height = numbers(data, 0, 3)
    assert size == width * height
    pixels = take(data, 12, size)
    palette = take(data, 12 + size, 768)
    assert palette[:3] == b'\0\xff\xff', name
    return width, height, pixels, palette


def read_json(path):
    text = re.sub(r'//[^\n]*', '', path.read_text())
    return json.loads(re.sub(r',\s*([}\]])', r'\1', text))


for town in towns:
    key = town['key']
    facts = read_json(root / 'references/rules/vcmi/config/factions' / (key + '.json'))[key]['town']
    background = facts['hallBackground'].rsplit('.', 1)[0].lower() + '.pcx'
    if background not in demo['pcx']:
        result['missingHallBackgrounds'].append({'faction': key, 'file': background})
    record = {'hallBackground': background, 'hallIcons': facts['buildingsIcons'].lower(),
              'hallSlots': facts['hallSlots'], 'layers': {}}
    for structure, definition in town['structures'].items():
        animation = definition.get('animation', '').lower()
        if animation not in sprites:
            continue
        area = definition.get('area', '').rsplit('.', 1)[0].lower() + '.pcx'
        border = definition.get('border', '').rsplit('.', 1)[0].lower() + '.pcx'
        exact = area in bitmap.entries and border in bitmap.entries
        if exact:
            width, height, pixels, _ = indexed(area)
            mask = bytes(int(pixel != 0) for pixel in pixels)
            bw, bh, pixels, palette = indexed(border)
            assert (bw, bh) == (width, height), (key, structure)
            outline = png(width, height, pixels, palette, bytes([0] + [255] * 255))
        else:
            lod, name = sprites[animation]
            resource = Def(lod.read(name))
            frame = resource.groups[0][0]
            width, height = frame.full_width, frame.full_height
            mask = bytearray(width * height)
            pixels = resource.pixels(frame)
            for y in range(frame.height):
                row = (frame.top + y) * width + frame.left
                mask[row:row + frame.width] = bytes(int(pixel >= 8) for pixel in pixels[y * frame.width:(y + 1) * frame.width])
            edge = bytearray(width * height)
            for y in range(height):
                for x in range(width):
                    index = y * width + x
                    if mask[index]:
                        continue
                    edge[index] = int(any(0 <= x + dx < width and 0 <= y + dy < height and mask[(y + dy) * width + x + dx]
                                          for dx, dy in [(1, 0), (-1, 0), (0, 1), (0, -1)]))
            palette = b'\0\xff\xff\xd2\xb4\x6e' + bytes(762)
            outline = png(width, height, edge, palette, bytes([0, 255] + [0] * 254))
            result['derivedMasks'].append({'faction': key, 'structure': structure, 'animation': animation})
        stem = key + '-' + structure.lower()
        file = root / 'assets/demo/town-ui/masks' / (stem + '.mask')
        outline_file = root / 'assets/demo/town-ui/outlines' / (stem + '.png')
        write(file, b'H3MK' + struct.pack('<II', width, height) + mask)
        write(outline_file, outline)
        record['layers'][structure] = {'mask': str(file.relative_to(root)), 'outline': str(outline_file.relative_to(root)),
                                       'width': width, 'height': height, 'original': exact,
                                       'areaSource': area if exact else animation, 'borderSource': border if exact else None}
    result['towns'][key] = record
write(root / 'assets/demo/town-ui.json', (json.dumps(result, ensure_ascii=False, indent=2) + '\n').encode())
fort_originals = ['tpcascas.pcx', 'tpcasram.pcx', 'tpcastow.pcx', 'tpcasinf.pcx', 'tpcasnec.pcx', 'tpcasdun.pcx', 'tpcasstr.pcx', 'tpcasfor.pcx', 'tpcasele.pcx']
fort_recruit = ['crbkgcas.pcx', 'crbkgram.pcx', 'crbkgtow.pcx', 'crbkginf.pcx', 'crbkgnec.pcx', 'crbkgdun.pcx', 'crbkgstr.pcx', 'crbkgfor.pcx', 'crbkgele.pcx']
fort_backgrounds = [original if original in demo['pcx'] else fallback for original, fallback in zip(fort_originals, fort_recruit)]
report = {'source': 'references/demo/inventory.json', 'layoutFacts': 'references/rules/sources.json', 'screenshots': 'references/town-interface-references.json',
          'fortPreviewBackgrounds': [{'faction': town['key'], 'file': fort_backgrounds[index], 'dedicatedFortAsset': fort_backgrounds[index] == fort_originals[index]} for index, town in enumerate(towns)],
          'limitations': ['Missing hall backgrounds use a neutral wood strip until original assets are found.',
                          'Foreground-derived interaction masks and outlines are approximate.',
                          'Missing faction fort previews temporarily reuse their recruitment backgrounds.',
                          'Original text, right-click help, stationed heroes, town switching and full window states remain incomplete.'],
          'towns': result['towns'], 'missingHallBackgrounds': result['missingHallBackgrounds'],
          'derivedMasks': result['derivedMasks'], 'originalMasks': sum(layer['original'] for town in result['towns'].values() for layer in town['layers'].values())}
write(root / 'data/native/town-interface.json', (json.dumps(report, ensure_ascii=False, indent=2) + '\n').encode())
print('Prepared town interface data:', report['originalMasks'], 'original masks;', len(result['derivedMasks']), 'derived fallbacks;', len(result['missingHallBackgrounds']), 'unavailable hall backgrounds')
