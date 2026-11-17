"""Inventory all classic spell IDs without treating metadata as implemented behavior."""
import collections
import json
import pathlib
import re

root = pathlib.Path(__file__).resolve().parents[1]
connected = {spell['key']: spell for spell in json.loads(
    (root / 'data/native/spells.json').read_text())['spells']}
audio = json.loads((root / 'assets/audio/manifest.json').read_text())
animated = {'mirth', 'sorrow', 'fortune', 'misfortune'}
rows = []
for category in ['adventure', 'offensive', 'timed', 'other']:
    path = root / 'references/rules/vcmi/config/spells' / (category + '.json')
    definitions = json.loads(re.sub(r',\s*([}\]])', r'\1',
        re.sub(r'//[^\n]*', '', path.read_text())))
    for key, definition in definitions.items():
        if 'index' not in definition:
            continue  # Internal trigger definitions are not separate classic spells.
        spell = connected.get(key)
        rows.append({
            'key': key,
            'originalId': definition['index'],
            'scope': 'adventure' if category == 'adventure' else 'combat',
            'effects': 'connected' if spell and spell['implemented'] else 'missing',
            'learning': 'guild' if spell and not spell['special'] else 'missing',
            'animation': 'partial' if key in animated else 'missing',
            'sound': 'cast-mapped' if spell and spell['implemented'] and
                     audio['spells'].get(str(spell['id'])) in audio['sounds'] else 'missing',
            'nativeId': spell['id'] if spell else None,
        })
rows.sort(key=lambda row: row['originalId'])
assert [row['originalId'] for row in rows] == list(range(70))
record = {
    'edition': 'Classic Complete / Shadow of Death',
    'meaning': {
        'connected': 'Core behavior is connected; this is not proof of original-game parity.',
        'guild': 'Town guild generation, spell book and Wisdom learning are connected; original executable comparison remains outstanding.',
        'partial': 'Original affect DEF frames play on successful targets, including mass casts; hero casting and exact timing/placement remain incomplete.',
        'cast-mapped': 'The original cast sound is mapped to SpellCast; complete timing, mixing and creature-caster behavior are not verified.',
        'missing': 'The corresponding behavior or acquisition route is not implemented.',
    },
    'limitations': [
        'No spell is marked fully verified against the original executable.',
        'Titan lightning is implemented for controlled tests, but its combination artifact and acquisition are missing.',
        'Creature casters, spell artifacts, hero specialties and complete resistance interactions are unfinished.',
        'Only Mirth, Sorrow, Fortune and Misfortune have affect animation playback; hero casting, projectiles, all other effects and exact timing/placement are unfinished.',
        'Available original cast sounds are mapped separately from animation coverage; a mapping is not proof of complete audio fidelity.',
    ],
    'spells': rows,
}
(root / 'data/native/spell-coverage.json').write_text(
    json.dumps(record, ensure_ascii=False, indent=2) + '\n')
print(dict(collections.Counter(row['effects'] for row in rows)))
