"""Record actual original asset availability separately from playback support."""
import json
import pathlib

root = pathlib.Path(__file__).resolve().parents[1]
demo = json.loads((root / 'assets/demo/manifest.json').read_text())
units = json.loads((root / 'data/native/creatures.json').read_text())['units']
towns = json.loads((root / 'data/native/town-runtime.json').read_text())
interface = json.loads((root / 'data/native/town-interface.json').read_text())
records = []
for unit in units:
    name = unit['animation']
    available = name in demo['def']
    groups = {key: len(value) for key, value in demo['def'].get(name, {}).get('groups', {}).items()}
    records.append({'id': unit['id'], 'name': unit['name'], 'portraitFrame': unit['portraitFrame'], 'animation': name, 'available': available, 'groups': groups,
                    'runtime': 'idle and final death frame' if available else 'previous static sprite-sheet fallback'})
missing = [record for record in records if not record['available']]
layers = [{'town': town['key'], 'animation': layer['animation'], 'available': layer['animation'] in demo['def'],
           'staticFallback': bool(layer['art'])} for town in towns['towns'] for layer in town['layers']]
result = {'sources': demo.get('sources', ['references/demo/source.json']) + ['references/demo/inventory.json', 'references/rules/sources.json'],
          'staticImages': len(demo['pcx']), 'spriteFiles': len(demo['def']),
          'frames': sum(len(sequence) for animation in demo['def'].values() for sequence in animation['groups'].values()),
          'creaturesWithAnimations': len(records) - len(missing), 'creaturesMissingAnimations': len(missing),
          'creatures': records, 'missingCreatureAnimations': missing, 'townLayers': layers,
          'unresolvedTownLayers': towns['missingArtwork'],
          'townInterface': {'originalMaskDefinitions': interface['originalMasks'], 'derivedMaskDefinitions': len(interface['derivedMasks']),
                            'missingHallBackgrounds': interface['missingHallBackgrounds'], 'fortPreviewBackgrounds': interface['fortPreviewBackgrounds'],
                            'details': 'data/native/town-interface.json'},
          'remaining': ['movement/attack/hit/turn/death/revival event sequencing', 'projectiles, hero and spell animations',
                        'original timing, palette cycling, player colors and exact shadow compositing',
                        'original battlefield anchoring and all window layouts/states']}
(root / 'data/native/graphics-coverage.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
print('Graphics coverage:', len(records) - len(missing), 'creatures with original animation data;', len(missing), 'missing;', len(towns['missingArtwork']), 'unresolved town layers')
