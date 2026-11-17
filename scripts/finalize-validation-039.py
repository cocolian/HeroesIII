#!/usr/bin/env python3
"""Archive completed 0.39 validation without changing historical releases."""
import hashlib
import json
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TMP = Path('/private/tmp')
OUT = ROOT/'references/validation/0.39'
OUT.mkdir(parents=True,exist_ok=True)
builds = {name:TMP/f'heroesiii-0.39-{name}-full'
          for name in ['native','sanitized','release']}
manifests = {}
audits = None
for name,directory in builds.items():
    log = TMP/f'heroesiii-0.39-{name}-full.log'
    assert 'Native UI flow passed. Rendered 368 screens' in log.read_text(),name
    files = sorted(directory.glob('*.png'))
    assert len(files)==368,name
    manifests[name] = {p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
    values = {p.name:json.loads(p.read_text()) for p in directory.glob('*audit*.json')}
    assert len(values)==18,name
    if audits is not None:
        assert values==audits,name+' audit mismatch'
    audits = values
assert manifests['native']==manifests['sanitized']==manifests['release']
for path in builds['release'].iterdir():
    if path.suffix in ('.png','.json'):
        shutil.copy2(path,OUT/path.name)
for pattern in ['heroesiii-0.39-*-build.log','heroesiii-0.39-*-ctest.log',
                'heroesiii-0.39-*-full.log','heroesiii-0.39-h3m-tests.log',
                'heroesiii-0.39-dmg-verify.log']:
    for path in TMP.glob(pattern):
        shutil.copy2(path,OUT/path.name)
assert 'is VALID' in (TMP/'heroesiii-0.39-dmg-verify.log').read_text()
for name in ['native','sanitized']:
    assert '100% tests passed' in (TMP/f'heroesiii-0.39-{name}-ctest.log').read_text()
assert '100% tests passed' in (TMP/'heroesiii-0.39-release-build.log').read_text()
assert 'Ran 8 tests' in (TMP/'heroesiii-0.39-h3m-tests.log').read_text()

source_files = [ROOT/'CMakeLists.txt',ROOT/'assets/demo/terrain.json']
source_files += sorted((ROOT/'native').rglob('*.cpp'))+sorted((ROOT/'native').rglob('*.hpp'))
source_files += [ROOT/'scripts'/n for n in ['h3m_terrain.py','calibrate_terrain.py',
    'generate-terrain-art.py','validate-terrain-art.py','test_h3m_terrain.py',
    'collect-community-maps.py','collect-video-reference-039.py','finalize-validation-039.py']]
source_hashes = {str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest()
                 for p in source_files}
(OUT/'source-sha256.json').write_text(json.dumps(source_hashes,indent=2)+'\n')
(OUT/'render-sha256.json').write_text(json.dumps(manifests,indent=2)+'\n')

package = json.loads((ROOT/'dist/macos-package.json').read_text())
assert package['version']=='0.39.0'
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
    'version':'0.39.0','recordedOn':'2026-11-15',
    'changes':['Hover and held-right-button help across 21 interface scenes',
               'Deterministic safe pathfinding with corrected dirt road costs',
               'Eight-neighbour guard interception with fixed overlap priority',
               'Victory-gated object visits and direct adjacent attack scene transition',
               'Bounded SoD gameplay video reviewed with HD/quick-combat limits recorded'],
    'rules':{'allThreeBuildsPassed':True,'h3mFormatAndReferenceTests':8,
             'coreRegressionCases':306,'newAdventureRegressionCases':12},
    'render':{'developer':368,'asanUbsan':368,'universalRelease':368,
              'allSharedPngHashesIdentical':True,'all18SharedAuditsIdentical':True,
              'screenshotHashes':str((OUT/'render-sha256.json').relative_to(ROOT))},
    'terrain':audits['terrain-audit.json'],
    'independentTerrainValidation':heldout,
    'campaign':{k:campaign[k] for k in ['completed','day','movementSteps','spells','levelChoices','xp','encounters']},
    'communityMaps':{'groups':10,'h3mFiles':14,'newGroups':0,'newH3mFiles':0,
                     'terrainBlocksValidated':True,'fullScenarioImported':False,
                     'playtestedInThisEngine':False,'bundle':bundle_info},
    'package':package,'dmgChecksumVerified':True,'bundledTerrainMatchesSource':True,
    'buttonHelp':audits['button-help-audit.json'],'guardEncounterFlow':audits['encounter-flow-audit.json'],
    'videoReview':'references/video-review-0.39/source.json',
    'sourceHashes':str((OUT/'source-sha256.json').relative_to(ROOT)),
    'scope':{'fullOriginalParity':False,'manualMacWindowPlaythroughThisRelease':False,
             'automatedOffscreenPlaythrough':True,'ipadDeviceValidationDeferred':True,
             'remainingGaps':['Full H3M object/event import','Other terrain types and wider corner context',
                              'Battle obstacle blocking','Neutral creature join/negotiate/flee encounters']}}
target = ROOT/'data/native/validation-0.39.json'
target.write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n')
print('Archived 368 matching release screenshots, 18 matching audits, and package verification')
print('Validation:',target)
