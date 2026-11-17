#!/usr/bin/env python3
"""Archive completed 0.40 validation without changing historical releases."""
import hashlib
import json
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TMP = Path('/private/tmp')
OUT = ROOT/'references/validation/0.40'
OUT.mkdir(parents=True,exist_ok=True)
builds = {name:TMP/f'heroesiii-0.40-{name}-verified'
          for name in ['native','sanitized','release']}
manifests = {}
audits = None
for name,directory in builds.items():
    log = TMP/f'heroesiii-0.40-{name}-verified.log'
    assert 'Native UI flow passed. Rendered 391 screens' in log.read_text(),name
    files = sorted(directory.glob('*.png'))
    assert len(files)==391,name
    manifests[name] = {p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
    values = {p.name:json.loads(p.read_text()) for p in directory.glob('*audit*.json')}
    assert len(values)==20,name
    if audits is not None:
        assert values==audits,name+' audit mismatch'
    audits = values
assert manifests['native']==manifests['sanitized']==manifests['release']
for path in builds['release'].iterdir():
    if path.suffix in ('.png','.json'):
        shutil.copy2(path,OUT/path.name)
for pattern in ['heroesiii-0.40-*-build.log','heroesiii-0.40-*-build-final.log','heroesiii-0.40-*-ctest.log',
                'heroesiii-0.40-*-full.log','heroesiii-0.40-*-final.log','heroesiii-0.40-*-verified.log','heroesiii-0.40-h3m-tests.log',
                'heroesiii-0.40-dmg-verify.log']:
    for path in TMP.glob(pattern):
        shutil.copy2(path,OUT/path.name)
assert 'is VALID' in (TMP/'heroesiii-0.40-dmg-verify.log').read_text()
for name in ['native','sanitized']:
    assert '100% tests passed' in (TMP/f'heroesiii-0.40-{name}-ctest.log').read_text()
assert '100% tests passed' in (TMP/'heroesiii-0.40-release-build.log').read_text()
assert 'Ran 8 tests' in (TMP/'heroesiii-0.40-h3m-tests.log').read_text()
for name,directory in [('native','native'),('sanitized','native-sanitized'),('release','distribution')]:
    path = ROOT/'build'/directory/'Testing/Temporary/LastTest.log'
    results = path.read_text()
    for expected in ['310 passed, 0 failed','22 audio checks passed, 0 failed',
                     'Ran 14 tests','Ran 8 tests']:
        assert expected in results,(name,expected)
    shutil.copy2(path,OUT/(name+'-detailed-test-results.log'))

source_files = [ROOT/'CMakeLists.txt',ROOT/'assets/demo/terrain.json']
source_files += sorted((ROOT/'native').rglob('*.cpp'))+sorted((ROOT/'native').rglob('*.hpp'))
source_files += [ROOT/'scripts'/n for n in ['h3m_terrain.py','calibrate_terrain.py',
    'generate-terrain-art.py','validate-terrain-art.py','test_h3m_terrain.py',
    'collect-community-maps.py','collect-video-reference-040.py','finalize-validation-040.py']]
source_hashes = {str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest()
                 for p in source_files}
(OUT/'source-sha256.json').write_text(json.dumps(source_hashes,indent=2)+'\n')
(OUT/'render-sha256.json').write_text(json.dumps(manifests,indent=2)+'\n')

package = json.loads((ROOT/'dist/macos-package.json').read_text())
assert package['version']=='0.40.0'
for name,info in package['files'].items():
    path = ROOT/'dist'/name
    assert path.stat().st_size==info['bytes']
    assert hashlib.sha256(path.read_bytes()).hexdigest()==info['sha256']
terrain = ROOT/'build/distribution/HeroesIII.app/Contents/Resources/GameData/assets/demo/terrain.json'
assert terrain.read_bytes()==(ROOT/'assets/demo/terrain.json').read_bytes()
shutil.copy2(ROOT/'dist/macos-package.json',OUT/'macos-package.json')

catalog = json.loads((ROOT/'maps/community/catalog.json').read_text())
assert len(catalog['maps'])==10 and not catalog['errors']
assert sum(len(m['mapFiles']) for m in catalog['maps'])==14
bundle = ROOT/'dist/HeroesIII-SoD-community-maps-20261115-expanded.zip'
bundle_info = {'file':str(bundle.relative_to(ROOT)),'bytes':bundle.stat().st_size,
               'sha256':hashlib.sha256(bundle.read_bytes()).hexdigest()}
lines = (ROOT/'dist/SHA256SUMS').read_text().splitlines()
entries = {line.split(None,1)[1].strip():line.split(None,1)[0] for line in lines if line.strip()}
entries.update({name:info['sha256'] for name,info in package['files'].items()})
entries[bundle.name]=bundle_info['sha256']
(ROOT/'dist/SHA256SUMS').write_text(''.join(f'{digest}  {name}\n' for name,digest in sorted(entries.items())))
campaign = audits['campaign-flow-audit.json']
heldout = json.loads((ROOT/'references/map-review-0.38/heldout-audit.json').read_text())
record = {
    'version':'0.40.0','recordedOn':'2026-11-15',
    'changes':['Persistent, explicitly acknowledged scenario victory and defeat dialogs',
               'Original adventure horse, visit, hero, town and travel-day cursors',
               'Original directional melee, shooting, distance penalty, movement and spell cursors',
               'Shared read-only attack plan used by both cursor preview and combat execution',
               'Bounded public gameplay video reviewed with HD interface limitations recorded'],
    'rules':{'allThreeBuildsPassed':True,'h3mFormatAndReferenceTests':8,
             'resourceFormatCases':14,'audioCases':22,
             'coreRegressionCases':310,'newCombatPlanRegressionCases':4},
    'render':{'developer':391,'asanUbsan':391,'universalRelease':391,
              'allSharedPngHashesIdentical':True,'all20SharedAuditsIdentical':True,
              'screenshotHashes':str((OUT/'render-sha256.json').relative_to(ROOT))},
    'terrain':audits['terrain-audit.json'],
    'independentTerrainValidation':heldout,
    'campaign':{k:campaign[k] for k in ['completed','explicitScenarioResultAcknowledged','day','movementSteps','spells','levelChoices','xp','encounters']},
    'communityMaps':{'groups':10,'h3mFiles':14,'newGroups':0,'newH3mFiles':0,
                     'terrainBlocksValidated':True,'fullScenarioImported':False,
                     'playtestedInThisEngine':False,'bundle':bundle_info},
    'package':package,'dmgChecksumVerified':True,'bundledTerrainMatchesSource':True,
    'buttonHelp':audits['button-help-audit.json'],'guardEncounterFlow':audits['encounter-flow-audit.json'],
    'outcome':audits['outcome-audit.json'],'combatCursors':audits['combat-cursor-audit.json'],
    'adventureCursors':audits['cursor-audit.json'],
    'videoReview':'references/video-review-0.40/source.json',
    'sourceHashes':str((OUT/'source-sha256.json').relative_to(ROOT)),
    'scope':{'fullOriginalParity':False,'manualMacWindowPlaythroughThisRelease':False,
             'automatedOffscreenPlaythrough':True,'ipadDeviceValidationDeferred':True,
             'remainingGaps':['Full H3M object/event import','Other terrain types and wider corner context',
                              'Battle obstacle blocking','Neutral creature join/negotiate/flee encounters',
                              'Original victory movie and complete scoring screen',
                              'Mouse-selected melee approach side and full cursor coverage outside gameplay']}}
target = ROOT/'data/native/validation-0.40.json'
target.write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n')
print('Archived 391 matching release screenshots, 20 matching audits, and package verification')
print('Validation:',target)
