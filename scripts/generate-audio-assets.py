"""Export verified demo SND audio and losslessly decode Mac IMA4 music.

Homebrew FFmpeg is used only as an offline media decoder. Runtime playback
and mixing are C++/SDL; no downloaded executable or decoder script is run.
"""
import hashlib
import json
import pathlib
import re
import shutil
import subprocess
import wave
from h3_resources import Snd
from original_resources import windows_files

root = pathlib.Path(__file__).resolve().parents[1]
source = root / 'references/demo/extracted/Heroes III Demo'
destination = root / 'assets/audio'
archive = source / 'data/heroes3.snd'
if hashlib.md5(archive.read_bytes()).hexdigest() != 'd032957a7dd6b1a9c2e3a62b7ed7f73d':
    raise ValueError('Demo SND checksum mismatch')
converter = shutil.which('ffmpeg')
if not converter:
    raise RuntimeError('Audio preparation requires FFmpeg from a trusted package registry')


def save(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists() or path.read_bytes() != data:
        path.write_bytes(data)


def facts(path):
    with wave.open(str(path), 'rb') as audio:
        if audio.getsampwidth() != 2 or audio.getnchannels() not in (1, 2):
            raise ValueError('Expected signed 16-bit PCM')
        return {'channels': audio.getnchannels(), 'sampleRate': audio.getframerate(), 'frames': audio.getnframes(),
                'seconds': audio.getnframes() / audio.getframerate()}


def decode(path, target):
    target.parent.mkdir(parents=True, exist_ok=True)
    original = hashlib.sha256(path.read_bytes()).hexdigest()
    stamp = target.with_suffix('.source')
    if not target.exists() or not stamp.exists() or stamp.read_text().strip() != original:
        subprocess.run([converter, '-nostdin', '-hide_banner', '-loglevel', 'error', '-y', '-i', str(path), '-vn', '-c:a', 'pcm_s16le', '-fflags', '+bitexact', str(target)], check=True)
        stamp.write_text(original + '\n')
    return {**facts(target), 'file': str(target.relative_to(root)), 'sha256': hashlib.sha256(target.read_bytes()).hexdigest(),
            'source': {'file': str(path.relative_to(root)), 'sha256': original}, 'decoder': 'FFmpeg offline decoding to signed PCM without extra lossy encoding'}


windows = windows_files(root)
manifest = {'schema': 1, 'sounds': {}, 'music': {}, 'units': {}, 'spells': {}, 'towns': {},
            'source': 'references/demo/source.json', 'sources': ['references/demo/source.json'] + (['references/windows-demo/source.json'] if windows else [])}
sounds = {}
for input_file, edition in ([(windows['heroes3.snd'], 'windows-roe-demo')] if windows else []) + [(archive, 'mac-complete-demo')]:
    snd = Snd(input_file)
    for name in snd.entries:
        sounds[name] = (snd.read(name), str(input_file.relative_to(root)), edition)
for name, (data, input_file, edition) in sorted(sounds.items()):
    target = destination / 'sfx' / name
    save(target, data)
    manifest['sounds'][name] = {'file': str(target.relative_to(root)), 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest(),
                                'source': input_file, 'edition': edition, 'encoding': 'Original IMA ADPCM WAVE'}
music_keys = {'StrongHold': 'town.stronghold', 'DIRT': 'terrain.dirt', 'WATER': 'terrain.water', 'COMBAT01': 'combat.1',
              'AITheme0': 'ai.1', 'Win Battle': 'report.win', 'LoseCombat': 'report.loss', 'Retreat Battle': 'report.retreat',
              'Surrender Battle': 'report.surrender', 'Defend Castle': 'report.siege-win', 'LoseCastle': 'report.siege-loss'}
for filename, key in music_keys.items():
    manifest['music'][key] = decode(source / 'music' / filename, destination / 'music' / (key + '.wav'))

if windows:
    for filename, key in {'mainmenu.mp3': 'menu.main', 'win scenario.mp3': 'report.scenario-win', 'ultimatelose.mp3': 'report.ultimate-loss'}.items():
        manifest['music'][key] = decode(windows[filename], destination / 'music' / (key + '.wav'))
        manifest['music'][key]['source']['record'] = 'references/windows-demo/source.json'
        manifest['music'][key]['source']['type'] = 'Original Windows RoE demo MP3 from the verified archive'


public_catalog = root / 'references/audio/soundtrack.json'
if public_catalog.exists():
    for track in json.loads(public_catalog.read_text())['tracks']:
        if track['key'] in manifest['music']:
            continue
        input_file = root / track['file']
        if hashlib.sha256(input_file.read_bytes()).hexdigest() != track['sha256']:
            raise ValueError('Public source checksum mismatch: ' + track['key'])
        manifest['music'][track['key']] = decode(input_file, destination / 'music' / (track['key'] + '.wav'))
        manifest['music'][track['key']]['source']['record'] = 'references/audio/soundtrack.json'
        manifest['music'][track['key']]['source']['type'] = 'Public third-party rip, not authenticated commercial bytes'


def read_json(path):
    text = re.sub(r'//[^\n]*', '', path.read_text())
    return json.loads(re.sub(r',\s*([}\]])', r'\1', text))


creature_data = {}
for file in sorted((root / 'references/rules/vcmi/config/creatures').glob('*.json')):
    creature_data.update(read_json(file))
by_index = {record['index']: record for record in creature_data.values() if isinstance(record, dict) and 'index' in record}
units = json.loads((root / 'data/native/creatures.json').read_text())['units']
for unit in units:
    sounds = by_index[unit['originalId']].get('sound', {})
    manifest['units'][str(unit['id'])] = {key: filename.lower() for key, filename in sounds.items() if isinstance(filename, str)}
for spell in json.loads((root / 'data/native/spells.json').read_text())['spells']:
    sound = spell.get('sounds', {}).get('cast', '')
    manifest['spells'][str(spell['id'])] = sound.lower() + '.wav' if sound else ''
for town in json.loads((root / 'data/native/towns.json').read_text())['towns']:
    facts_ = read_json(root / 'references/rules/vcmi/config/factions' / (town['key'] + '.json'))[town['key']]['town']
    manifest['towns'][town['key']] = {'key': 'town.' + town['key'], 'originalTheme': facts_.get('client', facts_)['musicTheme']}
missing_units = [{'unit': int(unit), 'action': key, 'file': name} for unit, sounds in manifest['units'].items() for key, name in sounds.items() if name not in manifest['sounds']]
missing_spells = [{'spell': int(spell), 'file': name} for spell, name in manifest['spells'].items() if name and name not in manifest['sounds']]
required = ['menu.main'] + ['town.' + name for name in manifest['towns']] + ['terrain.' + name for name in ['grass', 'dirt', 'sand', 'water']] + ['combat.' + str(i) for i in range(1, 5)]
report = {'source': 'references/demo/source.json', 'sources': manifest['sources'], 'soundCount': len(manifest['sounds']), 'musicCount': len(manifest['music']),
          'unitMappings': len(manifest['units']), 'missingUnitSounds': missing_units, 'missingSpellSounds': missing_spells,
          'missingRequiredMusic': [key for key in required if key not in manifest['music']],
          'limitations': ['Original cue timing, fades, all map ambient emitters and complete scenario/campaign audio require independent comparison.',
                          'Demo music is incomplete; absent original tracks are not replaced with unrelated music.']}
save(destination / 'manifest.json', (json.dumps(manifest, ensure_ascii=False, indent=2) + '\n').encode())
save(root / 'data/native/audio-coverage.json', (json.dumps(report, ensure_ascii=False, indent=2) + '\n').encode())
print('Prepared', len(manifest['sounds']), 'sound files and', len(manifest['music']), 'music tracks;', len(missing_units), 'unavailable unit cues;', len(missing_spells), 'unavailable spell cues')
