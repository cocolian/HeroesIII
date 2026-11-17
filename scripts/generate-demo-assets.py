"""Verify and decode the public demos into original-pixel PNG atlases.

Archive data stays in references/demo; C++ reads the generated asset manifest.
Expansion sprite entries override base entries by case-insensitive filename.
No external image library, parser, game code or executable is used.
"""
import collections
import hashlib
import json
import pathlib
from h3_resources import Def, pcx, png
from original_resources import OriginalResources

root = pathlib.Path(__file__).resolve().parents[1]
destination = root / 'assets/demo'
resources = OriginalResources(root)


def write(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists() or path.read_bytes() != data:
        path.write_bytes(data)


def atlas(definition, key):
    frames = {}
    for sequence in definition.groups.values():
        for frame in sequence:
            frames.setdefault(frame.offset, frame)
    pages, placement = [], {}
    # Crop rectangles retain original full-canvas offsets. Packing never scales
    # pixels, and offset aliases share one atlas rectangle.
    width = max(1024, max(frame.width for frame in frames.values()))
    height_limit = min(4096, 16777216 // width)
    x, y, row_height = 0, 0, 0
    page = []
    for frame in frames.values():
        if frame.height > height_limit:
            raise ValueError('Frame exceeds atlas limit: ' + key)
        if x + frame.width > width:
            x, y, row_height = 0, y + row_height + 1, 0
        if y + frame.height > height_limit:
            pages.append((page, y + row_height))
            page, x, y, row_height = [], 0, 0, 0
        placement[frame.offset] = (len(pages), x, y)
        page.append((frame, x, y))
        x += frame.width + 1
        row_height = max(row_height, frame.height)
    if page:
        pages.append((page, y + row_height))
    palette, alpha = definition.display_palette()
    output = []
    for index, (contents, height) in enumerate(pages):
        pixels = bytearray(width * height)
        for frame, x, y in contents:
            decoded = definition.pixels(frame)
            for row in range(frame.height):
                start = (y + row) * width + x
                pixels[start:start + frame.width] = decoded[row * frame.width:(row + 1) * frame.width]
        file = destination / 'def' / key[:-4] / f'{index}.png'
        write(file, png(width, height, pixels, palette, alpha))
        output.append({'file': str(file.relative_to(root)), 'width': width, 'height': height})
    groups = {}
    for group, sequence in definition.groups.items():
        groups[str(group)] = []
        for frame in sequence:
            page, x, y = placement[frame.offset]
            groups[str(group)].append({'page': page, 'rect': [x, y, frame.width, frame.height],
                                      'canvas': [frame.full_width, frame.full_height], 'offset': [frame.left, frame.top], 'name': frame.name})
    return {'kind': definition.kind, 'pages': output, 'groups': groups}


manifest = {'schema': 1, 'edition': 'Mac Complete 4.0 with verified Windows RoE demo supplements',
            'source': 'references/demo/source.json', 'sources': ['references/demo/source.json'] + (['references/windows-demo/source.json'] if resources.windows else []),
            'priority': 'Mac Complete overrides Windows RoE; only absent resources are supplemented', 'pcx': {}, 'def': {}}
counts = collections.Counter()
for key in sorted(resources.entries):
    data, provenance = resources.read(key), resources.source(key)
    if key.endswith('.pcx'):
        w, h, encoded = pcx(data)
        file = destination / 'pcx' / (key[:-4] + '.png')
        write(file, encoded)
        manifest['pcx'][key] = {'file': str(file.relative_to(root)), 'width': w, 'height': h, 'source': provenance}
        counts['pcx'] += 1
    elif key.endswith('.def'):
        definition = Def(data)
        result = atlas(definition, key)
        result['source'] = provenance
        manifest['def'][key] = result
        counts['def'] += 1
        for sequence in definition.groups.values():
            for frame in sequence:
                counts['frames'] += 1
                counts['format' + str(frame.format)] += 1
    if (counts['pcx'] + counts['def']) % 400 == 0:
        print(dict(counts), flush=True)

write(destination / 'manifest.json', (json.dumps(manifest, ensure_ascii=False, separators=(',', ':')) + '\n').encode())
report = {'archiveChecksums': resources.archives, 'entries': resources.inventory, 'effectiveEntries': len(resources.entries), 'decoded': dict(counts),
          'executed': False, 'limitations': ['Demo data coverage only; full commercial scenarios/campaigns/music are not present.',
          'Original shadow/selection compositing, player colors, palette cycling and animation timing still require independent comparison.']}
write(root / 'references/demo/inventory.json', (json.dumps(report, indent=2) + '\n').encode())
record = json.loads((root / 'references/demo/source.json').read_text())
record['coverage'] = dict(counts)
record['coverageScope'] = 'Combined runtime catalog, including Windows demo supplements; individual entries retain provenance'
by_edition = {}
for category in ('pcx', 'def'):
    for resource in manifest[category].values():
        edition = resource['source']['edition']
        summary = by_edition.setdefault(edition, {'pcx': 0, 'def': 0, 'frames': 0})
        summary[category] += 1
        if category == 'def':
            summary['frames'] += sum(len(sequence) for sequence in resource['groups'].values())
record['coverageByEdition'] = by_edition
record['dataChecksumsVerified'] = True
write(root / 'references/demo/source.json', (json.dumps(record, indent=2) + '\n').encode())
print('Decoded original demo graphics:', dict(counts))
