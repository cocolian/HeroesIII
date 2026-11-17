#!/usr/bin/env python3
"""Archive only the completed 0.45 campaign, rules and UI checks."""
import hashlib
import json
import shutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
TMP=Path('/private/tmp')
OUT=ROOT/'references/validation/0.45'
OUT.mkdir(parents=True,exist_ok=True)
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def read(p):return json.loads(p.read_text())
def copy(p,d):
    d.parent.mkdir(parents=True,exist_ok=True)
    shutil.copy2(p,d)
def archive_directory(source,label):
    for p in source.iterdir():
        if p.is_file() and p.suffix in ('.png','.json'):copy(p,OUT/label/p.name)
hashes={};audits={};kingdom_hashes={};kingdom_audits={}
for build in ['native','sanitized','release']:
    directory=TMP/f'heroesiii-0.45-{build}-selection-verified'
    assert 'Scenario cards and downloaded map library passed' in (TMP/(directory.name+'.log')).read_text()
    screens=sorted(directory.glob('*.png'));assert len(screens)==55
    hashes[build]={p.name:digest(p) for p in screens}
    audits[build]=read(directory/'scenario-selection-audit.json')
    assert len(audits[build]['downloadedMaps'])==14
    directory=TMP/f'heroesiii-0.45-{build}-kingdom-verified'
    assert 'Multihero, tavern, meeting and original-map outcome passed' in (TMP/(directory.name+'.log')).read_text()
    screens=sorted(directory.glob('*.png'));assert len(screens)==9
    kingdom_hashes[build]={p.name:digest(p) for p in screens}
    kingdom_audits[build]=read(directory/'kingdom-ui-audit.json')
    assert all(kingdom_audits[build].values())
assert hashes['native']==hashes['sanitized']==hashes['release']
assert audits['native']==audits['sanitized']==audits['release']
assert kingdom_hashes['native']==kingdom_hashes['sanitized']==kingdom_hashes['release']
assert kingdom_audits['native']==kingdom_audits['sanitized']==kingdom_audits['release']
for build in ['native','native-sanitized','distribution']:
    p=ROOT/'build'/build/'Testing/Temporary/LastTest.log'
    for expected in ['321 passed, 0 failed','40 kingdom checks passed','160 scenario checks passed across 14 downloaded maps','Ran 14 tests','Ran 8 tests','22 audio checks passed, 0 failed']:
        assert expected in p.read_text(),(build,expected)
    copy(p,OUT/(build+'-detailed-test-results.log'))
walkthrough=TMP/'heroesiii-0.45-release-walkthrough'
campaign=read(walkthrough/'campaign-audit.json');state=read(walkthrough/'final.json')
assert campaign['completed'] and state['result']==1 and state['scenarioId']=='seven-lakes-1'
assert campaign['noResourceOrArmyInjection'] and campaign['noTeleportOrFogReveal'] and campaign['realMapCollisionDataUnchanged']
assert campaign['sourceSha256']=='7c40c456bc04d55fd709942cb387f42e42ec1b7d0f428c45ce7dbd77afa38c15'
assert digest(ROOT/'maps/community/seven-lakes/1.h3m')==campaign['sourceSha256']
assert state['scenarioRules']['additional']['victory_condition']['type']==255
assert 'Downloaded map completed through normal gameplay' in (TMP/'heroesiii-0.45-release-walkthrough.log').read_text()
assert not any(h['owner']>0 for _,h in state['heroes'])
assert not any(o['type']==0 and o['owner']>0 for o in state['objects'])
for name in ['campaign-audit.json','final.json','actions.jsonl']:copy(walkthrough/name,OUT/'downloaded-map-walkthrough'/name)
interaction=TMP/'heroesiii-0.45-native-interaction-verified'
assert 'Interaction, runtime and campaign flow passed' in (TMP/(interaction.name+'.log')).read_text()
practice=read(interaction/'campaign-flow-audit.json')
assert practice['completed'] and practice['explicitScenarioResultAcknowledged']
archive_directory(TMP/'heroesiii-0.45-release-selection-verified','selection')
archive_directory(TMP/'heroesiii-0.45-release-kingdom-verified','kingdom')
archive_directory(interaction,'interaction')
for name in ['native-ctest','sanitized-ctest','release-ctest','native-selection-verified','sanitized-selection-verified','release-selection-verified','native-kingdom-verified','sanitized-kingdom-verified','release-kingdom-verified','native-interaction-verified','release-walkthrough','bundle-verify','package','dmg-verify']:
    p=TMP/('heroesiii-0.45-'+name+'.log');copy(p,OUT/p.name)
bundle=read(TMP/'heroesiii-0.45-bundle-verify.log')
assert bundle['version']=='0.45.0' and bundle['strictSignatureValid'] and not bundle['runtime']['sourceFallbackEnabled']
assert 'is VALID' in (TMP/'heroesiii-0.45-dmg-verify.log').read_text()
package=read(ROOT/'dist/macos-package.json');assert package['version']=='0.45.0'
for name,entry in package['files'].items():
    p=ROOT/'dist'/name;assert p.stat().st_size==entry['bytes'] and digest(p)==entry['sha256']
copy(ROOT/'dist/macos-package.json',OUT/'macos-package.json')
entries={line.split(None,1)[1].strip():line.split(None,1)[0] for line in (ROOT/'dist/SHA256SUMS').read_text().splitlines() if line.strip()}
entries.update({name:entry['sha256'] for name,entry in package['files'].items()})
(ROOT/'dist/SHA256SUMS').write_text(''.join(f'{sha}  {name}\n' for name,sha in sorted(entries.items())))
sources=[ROOT/'CMakeLists.txt']+[p for p in (ROOT/'native').rglob('*') if p.is_file() and p.suffix in ('.cpp','.hpp','.mm','.inc')]
sources += [ROOT/'scripts'/n for n in ['import-community-scenarios.py','generate-map-library.py','generate-owner-colors.py','generate-artifact-rules.py','build-release-macos.sh','stage-runtime.py','package-macos.py','finalize-validation-045.py']]
(OUT/'source-sha256.json').write_text(json.dumps({str(p.relative_to(ROOT)):digest(p) for p in sorted(sources)},indent=2)+'\n')
(OUT/'scenario-data-sha256.json').write_text(json.dumps({str(p.relative_to(ROOT)):digest(p) for p in sorted((ROOT/'assets/demo/scenarios').glob('*.json'))},indent=2)+'\n')
(OUT/'selection-render-sha256.json').write_text(json.dumps(hashes,indent=2)+'\n')
(OUT/'kingdom-render-sha256.json').write_text(json.dumps(kingdom_hashes,indent=2)+'\n')
summary={k:campaign[k] for k in ['map','sourceSha256','completed','day','reloads','movesIncludingRetries','spellsIncludingRetries','visitsIncludingRetries','noResourceOrArmyInjection','noTeleportOrFogReveal','realMapCollisionDataUnchanged','usesNormalSaveLoadRetries']}
summary.update({'automation':'native rules engine normal gameplay; UI checked separately','manualEntireCampaign':False,'battlesInWinningTimeline':len(campaign['battles']),'turnsInWinningTimeline':len(campaign['turns']),'storiesRecorded':len(state['scenarioRules']['journal']),'hostileHeroesRemaining':sum(h['owner']>0 for _,h in state['heroes']),'hostileTownsRemaining':campaign['turns'][-1]['enemyTowns'],'ownedTowns':campaign['turns'][-1]['towns']})
record={'version':'0.45.0','recordedOn':'2026-11-16','downloadedFiles':14,'ctest':{'builds':['native','ASan/UBSan','universal release'],'suites':6,'coreChecks':321,'kingdomChecks':40,'scenarioChecks':160,'resourceFormatChecks':14,'h3mTerrainChecks':8,'audioChecks':22,'passed':True},'selectionUi':{'screensPerBuild':55,'matchingPngHashes':True,'matchingAudits':True,'audit':audits['release']},'kingdomUi':{'screensPerBuild':9,'matchingPngHashes':True,'matchingAudits':True,'audit':kingdom_audits['release']},'nativeIntegration':{'screens':len(list(interaction.glob('*.png'))),'audits':len(list(interaction.glob('*audit*.json'))),'practiceCampaign':{k:practice[k] for k in ['completed','explicitScenarioResultAcknowledged','day','movementSteps','spells','xp','encounters']}},'downloadedMapWalkthrough':summary,'bundle':bundle,'package':package,'dmgVerified':True,'scope':{'communityMapsCompleted':1,'fullOriginalParity':False,'realIpadTested':False,'manualMacEntireCampaign':False,'fullRenderMatrixRerun':False,'missing':['complete story/object and town event rules','full adventure AI and spell/equipment strategy','complete siege walls/towers','complete hero specialties and creature abilities'],'sevenLakesAdaptation':'Each side pays 2500 gold for an initial hero when none is placed'},'evidenceDirectory':str(OUT.relative_to(ROOT))}
(ROOT/'data/native/validation-0.45.json').write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n')
print(json.dumps(summary,ensure_ascii=False,indent=2))
