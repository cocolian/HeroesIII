#!/usr/bin/env python3
"""Archive the focused input and integration checks actually run for 0.43."""
import hashlib
import json
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TMP = Path('/private/tmp')
OUT = ROOT/'references/validation/0.43'
OUT.mkdir(parents=True, exist_ok=True)

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def read(path):
    return json.loads(path.read_text())

hashes = {}
audits = {}
for build in ['native', 'sanitized', 'release']:
    directory = TMP/f'heroesiii-0.43-{build}-pointer'
    assert 'Trackpad, native pointer and touch input passed' in (TMP/f'heroesiii-0.43-{build}-pointer.log').read_text()
    files = sorted(directory.glob('*.png'))
    assert len(files) == 13
    hashes[build] = {p.name:digest(p) for p in files}
    audits[build] = {p.name:read(p) for p in directory.glob('*audit*.json')}
    assert len(audits[build]) == 2
assert hashes['native'] == hashes['sanitized'] == hashes['release']
assert audits['native'] == audits['sanitized'] == audits['release']
assert len(audits['release']['map-gesture-audit.json']['windows']) == 4

interaction = TMP/'heroesiii-0.43-native-interaction'
assert 'Interaction, runtime and campaign flow passed' in (TMP/'heroesiii-0.43-native-interaction.log').read_text()
assert len(list(interaction.glob('*.png'))) == 140
assert len(list(interaction.glob('*audit*.json'))) == 10
campaign = read(interaction/'campaign-flow-audit.json')
assert campaign['completed'] and campaign['explicitScenarioResultAcknowledged']

for source, label in [(TMP/'heroesiii-0.43-release-pointer','input'), (interaction,'interaction')]:
    destination = OUT/label
    destination.mkdir(exist_ok=True)
    for path in source.iterdir():
        if path.is_file() and path.suffix in ('.png','.json'):
            shutil.copy2(path,destination/path.name)
for path in TMP.glob('heroesiii-0.43-*.log'):
    shutil.copy2(path,OUT/path.name)
results = ROOT/'build/distribution/Testing/Temporary/LastTest.log'
for expected in ['321 passed, 0 failed','22 audio checks passed, 0 failed', 'Ran 14 tests', 'Ran 8 tests']:
    assert expected in results.read_text(), expected
shutil.copy2(results,OUT/'release-detailed-test-results.log')
release_log = (TMP/'heroesiii-0.43-release-build.log').read_text()
for expected in ['100% tests passed', '"strictSignatureValid": true', '"sourceFallbackEnabled": false', '"systemLibrariesOnly": true']:
    assert expected in release_log, expected
assert 'is VALID' in (TMP/'heroesiii-0.43-dmg-verify.log').read_text()

# Preserve game-only evidence from the isolated UI session. Horizontal requests
# generated zero deltas, so they are not presented as hardware gesture coverage.
events = []
for line in (TMP/'heroesiii-0.43-live.jsonl').read_text().splitlines():
    try:
        event = json.loads(line)
    except json.JSONDecodeError:
        continue
    if event['event'] in ['down','up','wheel']:
        events.append({k:event[k] for k in ['event','source','logical','camera','sceneBefore','sceneAfter']}
                      | ({'wheel':event['wheel']} if event['event']=='wheel' else {}))
releases = [e for e in events if e['event']=='up']
wheels = [e for e in events if e['event']=='wheel' and any(e['wheel'])]
assert releases[0]['camera'][2] > 1.7 and abs(releases[1]['camera'][2]-1) < .001
assert wheels and all(e['source']=='cocoa' and abs(e['camera'][2]-releases[0]['camera'][2])<.001 for e in wheels)
assert wheels[0]['camera'][1] != releases[0]['camera'][1]
manual = {'isolatedSaveDirectory':True, 'dragUpZoomIn':True, 'dragDownZoomOut':True,
          'verticalScrollPansWithoutZooming':True, 'nativeCocoaWheelObserved':True,
          'horizontalUiRequestsGeneratedNonzeroEvents':False,
          'physicalTwoFingerHardwareTested':False, 'events':events}
(OUT/'native-ui-input.json').write_text(json.dumps(manual,ensure_ascii=False,indent=2)+'\n')

package = read(ROOT/'dist/macos-package.json')
assert package['version'] == '0.43.0'
for name,info in package['files'].items():
    path = ROOT/'dist'/name
    assert path.stat().st_size == info['bytes'] and digest(path) == info['sha256']
shutil.copy2(ROOT/'dist/macos-package.json',OUT/'macos-package.json')
entries = {line.split(None,1)[1].strip():line.split(None,1)[0]
           for line in (ROOT/'dist/SHA256SUMS').read_text().splitlines() if line.strip()}
entries.update({name:info['sha256'] for name,info in package['files'].items()})
(ROOT/'dist/SHA256SUMS').write_text(''.join(f'{sha}  {name}\n' for name,sha in sorted(entries.items())))
sources = [ROOT/'CMakeLists.txt', ROOT/'scripts/finalize-validation-043.py', ROOT/'scripts/build-release-macos.sh']
sources += [p for p in (ROOT/'native').rglob('*') if p.is_file() and p.suffix in ('.cpp','.hpp','.mm')]
(OUT/'source-sha256.json').write_text(json.dumps({str(p.relative_to(ROOT)):digest(p) for p in sorted(sources)},indent=2)+'\n')
(OUT/'input-render-sha256.json').write_text(json.dumps(hashes,indent=2)+'\n')
record = {
    'version':'0.43.0','recordedOn':'2026-11-16',
    'controls':{'scroll':'two-finger / wheel pans both axes','pressDrag':'up zooms in, down zooms out',
                'touchscreen':'single finger pans, pinch zooms'},
    'inputRegression':{'builds':['native','ASan/UBSan','universal release'],
                       'screensPerBuild':13,'auditsPerBuild':2,'matchingPngHashes':True,
                       'matchingAudits':True,'gestures':audits['release']['map-gesture-audit.json'],
                       'touch':audits['release']['touch-layout-audit.json']},
    'nativeIntegration':{'screens':140,'audits':10,
                         'campaign':{k:campaign[k] for k in ['completed','explicitScenarioResultAcknowledged','day','movementSteps','spells','levelChoices','xp','encounters']}},
    'releaseCtest':{'core':321,'resourceFormats':14,'h3mFormats':8,'audio':22,'passed':True},
    'manualMacUi':manual,'package':package,'dmgVerified':True,
    'scope':{'fullRenderMatrixRerun':False,'previousFullRenderBaseline':'data/native/validation-0.42.json',
             'realIpadTested':False,'fullOriginalParity':False},
    'evidenceDirectory':str(OUT.relative_to(ROOT))}
target = ROOT/'data/native/validation-0.43.json'
target.write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n')
print('Archived 13 matching input screens across 3 builds and 140 integration screens')
print('Validation:',target)
