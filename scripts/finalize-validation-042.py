#!/usr/bin/env python3
"""Archive completed 0.42 validation without changing historical releases."""
import hashlib
import json
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TMP = Path('/private/tmp')
OUT = ROOT/'references/validation/0.42'
OUT.mkdir(parents=True,exist_ok=True)
builds = {name:TMP/f'heroesiii-0.42-{name}'
          for name in ['native','sanitized','release']}
manifests = {}
audits = None
for name,directory in builds.items():
    log = TMP/f'heroesiii-0.42-{name}.log'
    assert 'Native UI flow passed. Rendered 455 screens' in log.read_text(),name
    files = sorted(directory.glob('*.png'))
    assert len(files)==455,name
    manifests[name] = {p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
    values = {p.name:json.loads(p.read_text()) for p in directory.glob('*audit*.json')}
    assert len(values)==22,name
    if audits is not None:
        assert values==audits,name+' audit mismatch'
    audits = values
assert manifests['native']==manifests['sanitized']==manifests['release']
animation_pixels = {name:json.loads((directory/'animation-pixels.json').read_text())
                    for name,directory in builds.items()}
assert animation_pixels['native']==animation_pixels['sanitized']==animation_pixels['release']
for path in builds['release'].iterdir():
    if path.suffix in ('.png','.json'):
        shutil.copy2(path,OUT/path.name)
for path in TMP.glob('heroesiii-0.42-*.log'):
    shutil.copy2(path,OUT/path.name)
assert 'is VALID' in (TMP/'heroesiii-0.42-dmg-verify.log').read_text()
for name in ['native','sanitized']:
    assert '100% tests passed' in (TMP/f'heroesiii-0.42-{name}-ctest.log').read_text()
assert '100% tests passed' in (TMP/'heroesiii-0.42-release-build.log').read_text()

for name,directory in [('native','native'),('sanitized','native-sanitized'),('release','distribution')]:
    path = ROOT/'build'/directory/'Testing/Temporary/LastTest.log'
    results = path.read_text()
    for expected in ['321 passed, 0 failed','22 audio checks passed, 0 failed',
                     'Ran 14 tests','Ran 8 tests']:
        assert expected in results,(name,expected)
    shutil.copy2(path,OUT/(name+'-detailed-test-results.log'))

source_files = [ROOT/'CMakeLists.txt',ROOT/'assets/demo/terrain.json',ROOT/'assets/demo/adventure.json',ROOT/'assets/demo/puzzle.json',ROOT/'assets/demo/map-library.json']
source_files += sorted((ROOT/'assets/demo/map-previews').glob('*.png'))
source_files += sorted((ROOT/'native/generated').glob('adventure_*.inc'))
source_files += sorted((ROOT/'native/tests').glob('*.inc'))
source_files += sorted((ROOT/'native').rglob('*.cpp'))+sorted((ROOT/'native').rglob('*.hpp'))
source_files += [ROOT/'scripts'/n for n in ['h3m_terrain.py','calibrate_terrain.py',
    'generate-terrain-art.py','validate-terrain-art.py','test_h3m_terrain.py',
    'collect-community-maps.py','generate-adventure-geometry.py','generate-map-library.py','stage-runtime.py','build-release-macos.sh','verify-adventure-animations.py','finalize-validation-042.py']]
source_hashes = {str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest()
                 for p in source_files}
(OUT/'source-sha256.json').write_text(json.dumps(source_hashes,indent=2)+'\n')
(OUT/'render-sha256.json').write_text(json.dumps(manifests,indent=2)+'\n')

package = json.loads((ROOT/'dist/macos-package.json').read_text())
assert package['version']=='0.42.0'
for name,info in package['files'].items():
    path = ROOT/'dist'/name
    assert path.stat().st_size==info['bytes']
    assert hashlib.sha256(path.read_bytes()).hexdigest()==info['sha256']
terrain = ROOT/'build/distribution/HeroesIII.app/Contents/Resources/GameData/assets/demo/terrain.json'
assert terrain.read_bytes()==(ROOT/'assets/demo/terrain.json').read_bytes()
shutil.copy2(ROOT/'dist/macos-package.json',OUT/'macos-package.json')

runtime = terrain.parents[2]
library = json.loads((runtime/'assets/demo/map-library.json').read_text())
assert library==json.loads((ROOT/'assets/demo/map-library.json').read_text())
assert len(library['entries'])==15
assert sum(len(e['previews']) for e in library['entries'])==27
for entry in library['entries'][1:]:
    assert not entry['playable']
    assert hashlib.sha256((runtime/entry['file']).read_bytes()).hexdigest()==entry['sha256']
    for preview in entry['previews']:
        assert (runtime/preview).read_bytes()==(ROOT/preview).read_bytes()
assert len(audits['scenario-selection-audit.json']['townSelections'])==27
assert len(audits['scenario-selection-audit.json']['downloadedMaps'])==14
shutil.copy2(ROOT/'assets/demo/map-library.json',OUT/'map-library.json')


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
    'version':'0.42.0','recordedOn':'2026-11-16',
    'changes':['Whole illustrated town cards select all nine factions, with correct initial town and army',
               'New map library entry with paging, ground and underground terrain previews',
               '14 SHA-verified original community H3M files and 27 terrain previews bundled with the app',
               'Unsupported community-map starts disabled; practice map remains selectable after browsing'],
    'rules':{'allThreeBuildsPassed':True,'h3mFormatAndReferenceTests':8,
             'resourceFormatCases':14,'audioCases':22,
             'coreRegressionCases':321},
    'render':{'developer':455,'asanUbsan':455,'universalRelease':455,
              'allSharedPngHashesIdentical':True,'all22SharedAuditsIdentical':True,
              'screenshotHashes':str((OUT/'render-sha256.json').relative_to(ROOT))},
    'terrain':audits['terrain-audit.json'],
    'independentTerrainValidation':heldout,
    'campaign':{k:campaign[k] for k in ['completed','explicitScenarioResultAcknowledged','day','movementSteps','spells','levelChoices','xp','encounters']},
    'communityMaps':{'groups':10,'h3mFiles':14,'newGroups':0,'newH3mFiles':0,
                     'terrainBlocksValidated':True,'fullScenarioImported':False,
                     'runtimeBundled':True,'selectableTerrainPreviews':True,'previewLayers':27,
                     'playtestedInThisEngine':False,'bundle':bundle_info},
    'package':package,'dmgChecksumVerified':True,'bundledTerrainMatchesSource':True,
    'buttonHelp':audits['button-help-audit.json'],'guardEncounterFlow':audits['encounter-flow-audit.json'],
    'outcome':audits['outcome-audit.json'],'combatCursors':audits['combat-cursor-audit.json'],
    'adventureCursors':audits['cursor-audit.json'],
    'objectVisitFlow':audits['adventure-visits-audit.json'],
    'postVisitAnimationPixels':json.loads((builds['release']/'animation-pixels.json').read_text()),
    'objectRuleReferences':'references/object-review-0.41/source.json',
    'scenarioSelection':audits['scenario-selection-audit.json'],
    'manualMacSelectionObservation':{'isolatedSaveDirectory':True,'towerImageSelected':True,
                                     'downloadedMapPreviewVisible':True,'fullManualCampaign':False},
    'sourceHashes':str((OUT/'source-sha256.json').relative_to(ROOT)),
    'scope':{'fullOriginalParity':False,'manualMacWindowPlaythroughThisRelease':False,
             'automatedOffscreenPlaythrough':True,'ipadDeviceValidationDeferred':True,
             'remainingGaps':['Full H3M object/event import','Other terrain types and wider corner context',
                              'Battle obstacle blocking','Neutral creature join/negotiate/flee encounters',
                              'Original victory movie and complete scoring screen',
                              'Mouse-selected melee approach side and full cursor coverage outside gameplay',
                              'Chest gold/experience choice and complete artifact equipment',
                              'Nine faction puzzle artwork and Grail buildings',
                              'Per-object animation timing against unmodified original executable']}}
target = ROOT/'data/native/validation-0.42.json'
target.write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n')
print('Archived 455 matching release screenshots, 22 matching audits, and package verification')
print('Validation:',target)
