#!/usr/bin/env python3
"""Archive the original-map import checks actually run for 0.44."""
import hashlib
import json
import shutil
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
TMP=Path('/private/tmp')
OUT=ROOT/'references/validation/0.44'
OUT.mkdir(parents=True,exist_ok=True)
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def read(path):return json.loads(path.read_text())

hashes={};audits={}
for build in ['native','sanitized','release']:
    directory=TMP/f'heroesiii-0.44-{build}-selection-verified'
    assert 'Scenario cards and downloaded map library passed' in (TMP/(directory.name+'.log')).read_text()
    files=sorted(directory.glob('*.png'));assert len(files)==55
    hashes[build]={p.name:digest(p) for p in files}
    audits[build]=read(directory/'scenario-selection-audit.json')
    assert len(audits[build]['downloadedMaps'])==14
    assert audits[build]['boatFlagBlue'] and audits[build]['boatFlagBluePixels']>20
assert hashes['native']==hashes['sanitized']==hashes['release']
assert audits['native']==audits['sanitized']==audits['release']

interaction=TMP/'heroesiii-0.44-native-interaction-verified'
assert 'Interaction, runtime and campaign flow passed' in (TMP/(interaction.name+'.log')).read_text()
assert len(list(interaction.glob('*.png')))==157
campaign=read(interaction/'campaign-flow-audit.json')
assert campaign['completed'] and campaign['explicitScenarioResultAcknowledged']
for build in ['native','native-sanitized','distribution']:
    log=ROOT/'build'/build/'Testing/Temporary/LastTest.log'
    for expected in ['321 passed, 0 failed','160 scenario checks passed across 14 downloaded maps','Ran 14 tests','Ran 8 tests','22 audio checks passed, 0 failed']:
        assert expected in log.read_text(),(build,expected)
    shutil.copy2(log,OUT/(build+'-detailed-test-results.log'))
for source,label in [(TMP/'heroesiii-0.44-release-selection-verified','selection'),(interaction,'interaction')]:
    destination=OUT/label;destination.mkdir(exist_ok=True)
    for p in source.iterdir():
        if p.is_file() and p.suffix in ('.png','.json'):shutil.copy2(p,destination/p.name)
for name in ['native-ctest','sanitized-ctest','native-selection-verified','sanitized-selection-verified','release-selection-verified','native-interaction-verified','bundle-verify','package','dmg-verify']:
    p=TMP/('heroesiii-0.44-'+name+'.log');assert p.exists();shutil.copy2(p,OUT/p.name)
bundle=read(TMP/'heroesiii-0.44-bundle-verify.log')
assert bundle['version']=='0.44.0' and bundle['strictSignatureValid'] and not bundle['runtime']['sourceFallbackEnabled']
assert 'is VALID' in (TMP/'heroesiii-0.44-dmg-verify.log').read_text()
package=read(ROOT/'dist/macos-package.json');assert package['version']=='0.44.0'
for name,entry in package['files'].items():
    p=ROOT/'dist'/name;assert p.stat().st_size==entry['bytes'] and digest(p)==entry['sha256']
shutil.copy2(ROOT/'dist/macos-package.json',OUT/'macos-package.json')
entries={line.split(None,1)[1].strip():line.split(None,1)[0] for line in (ROOT/'dist/SHA256SUMS').read_text().splitlines() if line.strip()}
entries.update({name:entry['sha256'] for name,entry in package['files'].items()})
(ROOT/'dist/SHA256SUMS').write_text(''.join(f'{sha}  {name}\n' for name,sha in sorted(entries.items())))

manual={'map':'seven-lakes-1','selectionAndStartObserved':True,'twoChestPickups':True,'goldAfterPickups':10500,'day2TownIncome':500,'embarkSailDisembark':True,'shipFlagIssueFoundAndCorrected':True,'inputTraceRecorded':False,'developmentAutosaveOverwrittenDuringUiTest':True,'manualSaveUntouched':True,'testStateBackup':'/private/tmp/heroesiii-0.44-live/observed-final-autosave.json'}
state=read(TMP/'heroesiii-0.44-live/observed-final-autosave.json')
assert state['scenarioId']=='seven-lakes-1' and state['day']==2 and state['resources'][0]==11000 and not state['hero']['onBoat']
manual['finalState']={k:state[k] for k in ['scenarioId','day','resources','mapLevel']}
manual['finalHero']={k:state['hero'][k] for k in ['pos','onBoat','movement']}
(OUT/'manual-mac-ui.json').write_text(json.dumps(manual,ensure_ascii=False,indent=2)+'\n')
sources=[ROOT/'CMakeLists.txt']+[p for p in (ROOT/'native').rglob('*') if p.is_file() and p.suffix in ('.cpp','.hpp','.mm','.inc')]
sources += [ROOT/'scripts'/n for n in ['import-community-scenarios.py','generate-map-library.py','generate-artifact-rules.py','build-release-macos.sh','stage-runtime.py','package-macos.py','finalize-validation-044.py']]
(OUT/'source-sha256.json').write_text(json.dumps({str(p.relative_to(ROOT)):digest(p) for p in sorted(sources)},indent=2)+'\n')
(OUT/'scenario-data-sha256.json').write_text(json.dumps({str(p.relative_to(ROOT)):digest(p) for p in sorted((ROOT/'assets/demo/scenarios').glob('*.json'))},indent=2)+'\n')
(OUT/'selection-render-sha256.json').write_text(json.dumps(hashes,indent=2)+'\n')
record={'version':'0.44.0','recordedOn':'2026-11-16','downloadedFiles':14,'mapLevels':27,'mode':'original-exploration','ctest':{'builds':['native','ASan/UBSan','universal release'],'suites':5,'coreChecks':321,'scenarioChecks':160,'resourceFormatChecks':14,'h3mTerrainChecks':8,'audioChecks':22,'passed':True},'selectionUi':{'screensPerBuild':55,'matchingPngHashes':True,'matchingAudits':True,'builds':['native','ASan/UBSan','universal release'],'audit':audits['release']},'nativeIntegration':{'screens':157,'audits':len(list(interaction.glob('*audit*.json'))),'practiceCampaign':{k:campaign[k] for k in ['completed','explicitScenarioResultAcknowledged','day','movementSteps','spells','xp','encounters']}},'manualMacUi':manual,'bundle':bundle,'package':package,'dmgVerified':True,'scope':{'communityMapsCompleted':0,'controlledCombatAndMiningFixturesAreNotWalkthroughs':True,'fullOriginalParity':False,'realIpadTested':False,'fullRenderMatrixRerun':False,'missing':['complete story/object rules','adventure AI','multiple controllable heroes','complete artifact effects and hero classes','complete original creature abilities'],'sevenLakesAdaptation':'Automatically recruits one starting hero for 2500 gold'},'evidenceDirectory':str(OUT.relative_to(ROOT))}
(ROOT/'data/native/validation-0.44.json').write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n')
print('Archived 14 map imports, 55 matching selection screens across 3 builds, and 157 integration screens')
