#!/usr/bin/env python3
"""Archive the reported hero/exit regressions and measured 0.48 optimizations."""
import hashlib
import json
import re
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TMP = Path('/private/tmp')
OUT = ROOT / 'references/validation/0.48'
OUT.mkdir(parents=True, exist_ok=True)


def read(path):
    return json.loads(path.read_text())


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda: f.read(2 ** 20), b''):
            h.update(block)
    return h.hexdigest()


def copy(source, target):
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, target)


def write(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n')


tests = {}
for label, build in [('native', 'native'), ('release', 'distribution'),
                     ('sanitized', 'native-sanitized')]:
    log = TMP / f'heroesiii-0.48-{label}-ctest-final.log'
    text = log.read_text()
    assert '100% tests passed' in text and 'out of 11' in text, label
    detail = ROOT / 'build' / build / 'Testing/Temporary/LastTest.log'
    text = detail.read_text()
    for expected in ['321 passed, 0 failed', '40 kingdom checks passed',
                     '85 siege, story and strategy checks passed',
                     '83 adventure site checks passed',
                     '7 background save checks passed',
                     '185136 ground-layer checks passed',
                     '169 scenario checks passed across 15 downloaded maps',
                     'Ran 14 tests', 'Ran 9 tests',
                     '23 audio checks passed, 0 failed']:
        assert expected in text, (label, expected)
    locale = re.search(r'(\d+) localization checks passed', text)
    assert locale
    tests[label] = {'ctestSuites': 11, 'passed': True,
                    'localizationChecks': int(locale.group(1))}
    copy(log, OUT / log.name)
    copy(detail, OUT / f'{label}-detailed-test-results.log')

baseline_dir = TMP / 'heroesiii-0.48-baseline'
baseline = read(baseline_dir / 'performance-audit.json')
assert len(baseline['movingHeroPixelContributions']) == 64
assert max(baseline['movingHeroPixelContributions']) == 0
for path in baseline_dir.iterdir():
    if path.suffix in ['.png', '.json']:
        copy(path, OUT / 'baseline' / path.name)

performance, hashes = {}, {}
for label in ['native', 'release', 'sanitized']:
    directory = TMP / f'heroesiii-0.48-{label}-performance-final'
    audit = read(directory / 'performance-audit.json')
    pngs = sorted(directory.glob('*.png'))
    assert len(pngs) == 3
    hashes[label] = {path.name: digest(path) for path in pngs}
    assert len(audit['movingHeroPixelContributions']) == 64
    assert audit['minimumMovingHeroPixels'] >= 654
    assert audit['latestBackgroundSaveMatches']
    assert audit['writesFor20MarchingRequestsAndFinalFlush'] == 2
    assert not audit['originalSaveWritten']
    assert len(audit['exitChecks']) == 3
    for check in audit['exitChecks']:
        assert check['originalExitHitbox'] and check['oldOversizedHitboxRemoved']
        assert check['returnAndEscape']
    performance[label] = audit
    copy(directory / 'performance-audit.json', OUT / f'{label}-performance-audit.json')
    copy(Path(str(directory) + '.log'), OUT / f'{label}-performance.log')
    if label == 'release':
        for path in pngs:
            copy(path, OUT / 'fixed' / path.name)
assert hashes['native'] == hashes['release'] == hashes['sanitized']
write(OUT / 'performance-render-sha256.json', hashes)

regression = {}
for mode, count in [('interaction', 160), ('display', 190)]:
    directory = TMP / f'heroesiii-0.48-native-{mode}-final'
    log = Path(str(directory) + '.log')
    assert 'passed' in log.read_text()
    pngs = sorted(directory.glob('*.png'))
    assert len(pngs) == count, (mode, len(pngs))
    write(OUT / mode / 'render-sha256.json', {p.name: digest(p) for p in pngs})
    for path in directory.glob('*.json'):
        copy(path, OUT / mode / path.name)
    copy(log, OUT / f'{mode}.log')
    campaign = read(directory / 'campaign-flow-audit.json')
    assert campaign['completed'] and campaign['naturalProgression']
    assert campaign['noResourceOrArmyInjection']
    assert campaign['explicitScenarioResultAcknowledged']
    regression[mode] = {'passed': True, 'screens': count,
                        'completePracticeMap': True}
art = read(TMP / 'heroesiii-0.48-native-display-final/map-art-audit.json')
assert art['opaqueForegroundHidesHero'] and art['objectOrderIndependent']
assert art['gameStateAndRngUnchanged']

source_record = read(TMP / 'heroesiii-0.48-user/source-checksum.json')
source = Path(source_record['source'])
original = read(TMP / 'heroesiii-0.48-user/source.json')
live_dir = TMP / 'heroesiii-0.48-live'
live = read(live_dir / 'autosave.json')
for key in ['day', 'activeHeroId', 'scenarioId', 'resources']:
    assert original[key] == live[key], key
for key in ['army', 'mana', 'xp', 'pos']:
    assert original['hero'][key] == live['hero'][key], key
assert original['hero']['movement'] - live['hero']['movement'] == 400
source_record.update(sha256After=digest(source), taskWroteUserSave=False,
                     testSourceWasPrivateCopy=True)
source_record['unchanged'] = source_record['sha256'] == source_record['sha256After']
assert source_record['unchanged']
write(OUT / 'user-save-preservation.json', source_record)
trace = [json.loads(s) for s in (live_dir / 'input.jsonl').read_text().splitlines()]
assert any(t['sceneBefore'] != t['sceneAfter'] for t in trace)
copy(live_dir / 'input.jsonl', OUT / 'live-input.jsonl')
live_audit = {'privateSaveDirectory': str(live_dir), 'originalSaveWritten': False,
              'retinaOutputPixels': trace[-1]['outputPixels'],
              'windowPoints': trace[-1]['windowSize'],
              'observedFullscreen': True, 'originalExitClickedAndClosed': True,
              'northTwoThenSouthTwoSteps': True,
              'heroAndFlagVisibleAtReportedCliff': True,
              'finalPosition': live['hero']['pos'],
              'movementBefore': original['hero']['movement'],
              'movementAfter': live['hero']['movement'],
              'dayArmyResourcesXpManaPreserved': True,
              'quitViaNativeMenu': True}
write(OUT / 'live-audit.json', live_audit)

provenance_path = ROOT / 'references/hero-performance-0.48/ground-layer-provenance.json'
provenance = read(provenance_path)
assert provenance['spriteDefaults'] == 2776
assert provenance['positionSpecificOverrides'] == 13
for path, sha in provenance['sources'].items():
    assert digest(ROOT / path) == sha
copy(provenance_path, OUT / provenance_path.name)
reported_image = Path('/var/folders/43/5f4t56y94s972dbfftrb07m00000gn/T/'
                      'codex-clipboard-2bafc9c7-d868-4196-9582-b07d343ccc98.png')
copy(reported_image, OUT / 'reported-hero-window.png')

bundle = read(TMP / 'heroesiii-0.48-bundle-verify.log')
assert bundle['version'] == '0.48.0' and bundle['strictSignatureValid']
assert bundle['runtime']['startupFullscreen']
assert not bundle['runtime']['sourceFallbackEnabled']
assert bundle['verifiedAssetFiles'] == 11404
package = read(ROOT / 'dist/macos-package.json')
assert package['version'] == '0.48.0'
assert set(package['architectures']) == {'arm64', 'x86_64'}
for name, entry in package['files'].items():
    path = ROOT / 'dist' / name
    assert path.stat().st_size == entry['bytes'] and digest(path) == entry['sha256']
payload = read(TMP / 'heroesiii-0.48-package-payload.json')
assert payload['applicationPayload']
for name in ['bundle-verify', 'package']:
    copy(TMP / f'heroesiii-0.48-{name}.log', OUT / f'{name}.log')
copy(TMP / 'heroesiii-0.48-package-payload.json', OUT / 'package-payload.json')
copy(ROOT / 'dist/macos-package.json', OUT / 'macos-package.json')
entries = {line.split(None, 1)[1].strip(): line.split(None, 1)[0]
           for line in (ROOT / 'dist/SHA256SUMS').read_text().splitlines()
           if line.strip()}
entries.update({name: entry['sha256'] for name, entry in package['files'].items()})
(ROOT / 'dist/SHA256SUMS').write_text(''.join(f'{sha}  {name}\n'
                                              for name, sha in sorted(entries.items())))
sources = [ROOT / 'CMakeLists.txt', ROOT / 'README.md',
           ROOT / 'docs/hero-and-performance-0.48.md']
sources += [p for p in (ROOT / 'native').rglob('*')
            if p.is_file() and p.suffix in ['.cpp', '.hpp', '.inc', '.mm']]
sources += [ROOT / 'scripts' / name for name in [
    'generate-ground-layers.py', 'build-native.sh', 'localize-community-maps.py',
    'stage-runtime.py', 'package-macos.py', 'finalize-validation-048.py']]
sources += list((ROOT / 'assets/demo/localization').glob('*.json'))
write(OUT / 'source-sha256.json',
      {str(p.relative_to(ROOT)): digest(p) for p in sorted(sources)})
result = {'version': '0.48.0', 'tests': tests,
          'performanceBaseline': baseline, 'performance': performance,
          'identicalThreeBuildScreens': True, 'regression': regression,
          'groundLayers': provenance, 'userSave': source_record,
          'live': live_audit, 'opaqueForegroundOcclusionPreserved': True,
          'frameTimingIncludesSoftwarePresentationWait': True,
          'liveGpuFpsMeasured': False, 'fullWayfarerReplay': False,
          'originalGameParity': False, 'bundle': bundle,
          'package': package, 'payload': payload}
write(ROOT / 'data/native/validation-0.48.json', result)
print(json.dumps({'version': '0.48.0', 'ctestSuitesPerBuild': 11,
                  'identicalScreensPerBuild': 3,
                  'walkingFramesPerBuild': 64,
                  'nativeRegressionScreens': 350,
                  'userSaveUnchanged': source_record['unchanged'],
                  'package': list(package['files'])}, ensure_ascii=False, indent=2))
