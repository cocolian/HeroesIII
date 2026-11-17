#!/usr/bin/env python3
"""Archive completed 0.46 runs without claiming original game parity."""
import hashlib
import json
import shutil
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
TMP=Path('/private/tmp')
OUT=ROOT/'references/validation/0.46'
OUT.mkdir(parents=True,exist_ok=True)
def read(path):return json.loads(path.read_text())
def digest(path):
    h=hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda:stream.read(1024*1024),b''):h.update(block)
    return h.hexdigest()
def copy(source,target):
    target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,target)
def archive(source,label,suffixes=('.json','.png','.jsonl')):
    for path in source.iterdir():
        if path.is_file() and path.suffix in suffixes:copy(path,OUT/label/path.name)

ui={}
for kind,count,audit_name in [('selection',58,'scenario-selection-audit.json'),('siege',50,'siege-ui-audit.json'),('kingdom',9,'kingdom-ui-audit.json')]:
    hashes={};audits={}
    for build in ['native','sanitized','release']:
        # The final siege pass also covers the repositioned boat flag fixtures.
        ending='portraits'
        directory=TMP/f'heroesiii-0.46-{build}-{kind}-{ending}'
        screens=sorted(directory.glob('*.png'));assert len(screens)==count,(directory,len(screens))
        log=TMP/(directory.name+'.log');assert 'passed' in log.read_text(),str(log)
        hashes[build]={p.name:digest(p) for p in screens};audits[build]=read(directory/audit_name)
        copy(log,OUT/log.name)
    assert hashes['native']==hashes['sanitized']==hashes['release'],kind
    assert audits['native']==audits['sanitized']==audits['release'],kind
    if kind=='selection':assert len(audits['release']['downloadedMaps'])==15
    if kind=='siege':assert len(audits['release']['originalTownArtwork'])==9 and audits['release']['boatOwnerColors']==8
    (OUT/(kind+'-render-sha256.json')).write_text(json.dumps(hashes,indent=2)+'\n')
    ending='portraits'
    archive(TMP/f'heroesiii-0.46-release-{kind}-{ending}',kind)
    ui[kind]={'screensPerBuild':count,'matchingPngHashes':True,'matchingAudits':True,'audit':audits['release']}

bundled=TMP/'heroesiii-0.46-bundled-siege-portraits'
assert 'passed' in (TMP/(bundled.name+'.log')).read_text()
assert {p.name:digest(p) for p in bundled.glob('*.png')}==read(OUT/'siege-render-sha256.json')['release']
copy(TMP/(bundled.name+'.log'),OUT/'bundled-siege.log')

for build,label in [('native','native'),('native-sanitized','sanitized'),('distribution','release')]:
    detail=ROOT/'build'/build/'Testing/Temporary/LastTest.log'
    for expected in ['321 passed, 0 failed','40 kingdom checks passed','85 siege, story and strategy checks passed','169 scenario checks passed across 15 downloaded maps','Ran 14 tests','Ran 9 tests','22 audio checks passed, 0 failed']:
        assert expected in detail.read_text(),(build,expected)
    log=TMP/f'heroesiii-0.46-{label}-ctest.log';assert '100% tests passed' in log.read_text()
    copy(detail,OUT/(build+'-detailed-test-results.log'));copy(log,OUT/log.name)
assert '85 siege, story and strategy checks passed' in (TMP/'heroesiii-0.46-intel-siege.log').read_text()
copy(TMP/'heroesiii-0.46-intel-siege.log',OUT/'intel-siege.log')

campaigns={}
for policy in ['current','legacy']:
    directory=TMP/f'heroesiii-0.46-{policy}-final'
    audit=read(directory/'campaign-audit.json');state=read(directory/'final.json')
    assert audit['map']==state['scenarioId']=='dead-and-buried-1'
    assert audit['normalGameplayOnly'] and audit['reloads']==0
    assert digest(ROOT/'maps/community/dead-and-buried/1.h3m')==audit['sourceSha256']
    if policy=='current':
        assert audit['completed'] and state['result']==1 and audit['day']==35
        assert not any(h['owner']>0 for _,h in state['heroes'])
        assert not any(o['type']==0 and o['owner']>0 for o in state['objects'])
        assert any(b['fort']>0 and b['won'] for b in audit['battles'])
    else:assert state['result']==-1 and audit['day']==75
    archive(directory,policy+'-campaign')
    copy(TMP/(directory.name+'.log'),OUT/(policy+'-campaign.log'))
    campaigns[policy]={k:audit[k] for k in ['map','sourceSha256','completed','result','day','moves','visits','reloads','normalGameplayOnly','policy','difficulty','startingHero','faction']}
    if policy=='current':assert (TMP/'heroesiii-0.46-current-courier/campaign-audit.json').read_bytes()==(directory/'campaign-audit.json').read_bytes()
    campaigns[policy].update({'battles':len(audit['battles']),'wins':sum(b['won'] for b in audit['battles']),'sieges':sum(b['fort']>0 for b in audit['battles']),'computerHeroTurns':len(audit['computerActivity']),'computerEncounters':sum(a['encounters'] for a in audit['computerActivity']),'computerWins':sum(a['wins'] for a in audit['computerActivity']),'computerManaSpent':sum(a['manaSpent'] for a in audit['computerActivity']),'storiesRecorded':len(state['scenarioRules'].get('journal',[]))})

interaction=TMP/'heroesiii-0.46-native-interaction-checked'
assert 'Interaction, runtime and campaign flow passed' in (TMP/(interaction.name+'.log')).read_text()
practice=read(interaction/'campaign-flow-audit.json')
assert practice['completed'] and practice['explicitScenarioResultAcknowledged']
runtime=read(interaction/'runtime-flow-audit.json')
assert runtime['naturalAdventureProgression'] and runtime['spellsCast']>=3 and runtime['defeatedGuardRemoved']
archive(interaction,'interaction');copy(TMP/(interaction.name+'.log'),OUT/'native-interaction.log')
bundle=read(TMP/'heroesiii-0.46-bundle-verify.log')
assert bundle['version']=='0.46.0' and bundle['strictSignatureValid'] and not bundle['runtime']['sourceFallbackEnabled']
assert 'is VALID' in (TMP/'heroesiii-0.46-dmg-verify.log').read_text()
package=read(ROOT/'dist/macos-package.json');assert package['version']=='0.46.0'
for name,entry in package['files'].items():
    path=ROOT/'dist'/name;assert path.stat().st_size==entry['bytes'] and digest(path)==entry['sha256']
for name in ['bundle-verify','package','dmg-verify']:
    path=TMP/f'heroesiii-0.46-{name}.log';copy(path,OUT/path.name)
copy(ROOT/'dist/macos-package.json',OUT/'macos-package.json')
entries={line.split(None,1)[1].strip():line.split(None,1)[0] for line in (ROOT/'dist/SHA256SUMS').read_text().splitlines() if line.strip()}
entries.update({name:entry['sha256'] for name,entry in package['files'].items()})
(ROOT/'dist/SHA256SUMS').write_text(''.join(f'{sha}  {name}\n' for name,sha in sorted(entries.items())))

sources=[ROOT/'CMakeLists.txt']+[p for p in (ROOT/'native').rglob('*') if p.is_file() and p.suffix in ('.cpp','.hpp','.mm','.inc')]
sources += [ROOT/'scripts'/name for name in ['community_terrain.py','audit-video-map.py','generate-siege-art.py','generate-hero-defaults.py','generate-interface-colors.py','generate-video-gallery-046.py','import-community-scenarios.py','generate-map-library.py','test_h3m_terrain.py','build-release-macos.sh','stage-runtime.py','package-macos.py','finalize-validation-046.py']]
(OUT/'source-sha256.json').write_text(json.dumps({str(p.relative_to(ROOT)):digest(p) for p in sorted(sources)},indent=2)+'\n')
(OUT/'scenario-data-sha256.json').write_text(json.dumps({str(p.relative_to(ROOT)):digest(p) for p in sorted((ROOT/'assets/demo/scenarios').glob('*.json'))},indent=2)+'\n')
record={'version':'0.46.0','recordedOn':'2026-11-16','downloadedFiles':15,'ctest':{'builds':['native','ASan/UBSan','universal release'],'suites':7,'coreChecks':321,'kingdomChecks':40,'siegeStoryStrategyChecks':85,'scenarioChecks':169,'resourceChecks':14,'terrainChecks':9,'audioChecks':22,'passed':True},'ui':ui,'bundledUiMatchesSource':True,'sameSeedCampaignRepeatedIdentically':True,'intelSiegeChecks':85,'nativeIntegration':{'screens':len(list(interaction.glob('*.png'))),'practiceCampaign':{k:practice[k] for k in ['completed','explicitScenarioResultAcknowledged','day','movementSteps','spells','xp']},'runtime':runtime},'campaigns':campaigns,'video':read(ROOT/'references/video-review-0.46/source.json'),'mapConversion':read(ROOT/'references/maps-0.46/conversion-audit.json'),'bundle':bundle,'package':package,'dmgVerified':True,'scope':{'fullOriginalParity':False,'manualEntireCampaign':False,'realIpadTested':False,'identicalRandomSeedToVideo':False,'originalComputerDifficultyBonusesImplemented':False,'videoPresetAllPlayersStartWithZeroResources':True,'all15MapsCompleted':False,'selectedMapHasNoGlobalTimedEvents':True,'humanDefenseOrientationMirrored':True,'wallLineAndTowerDamageApproximate':True,'missing':['full Ballistics and Artillery control and destructible war machines','complete gate and catapult animations','Tactics deployment and all hero specialties','all adventure spells and map object rules','original scenario/tavern/meeting/story/high-score windows'],'comparison':'Current and nearest-target battle policies share current rules and adventure policies; video player is a separate reference with unknown RNG'},'evidenceDirectory':str(OUT.relative_to(ROOT))}
(ROOT/'data/native/validation-0.46.json').write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n')
print(json.dumps({'version':record['version'],'campaigns':campaigns,'screens':{k:v['screensPerBuild'] for k,v in ui.items()},'fullOriginalParity':False},ensure_ascii=False,indent=2))
