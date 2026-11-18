#!/usr/bin/env python3
"""Archive completed 0.52 checks without copying any personal game progress."""
import hashlib,json,pathlib,re,shutil,os
ROOT=pathlib.Path(__file__).resolve().parents[1]
OUT=ROOT/'references/validation/0.52'
OUT.mkdir(parents=True,exist_ok=True)
def read(path):return json.loads(path.read_text())
def archive(source,target):
    target.parent.mkdir(parents=True,exist_ok=True)
    if source.suffix=='.png':
        if target.exists():target.unlink()
        try:os.link(source,target);return
        except OSError:pass
    shutil.copy2(source,target)
def tree(source,target):
    for path in source.rglob('*'):
        if path.is_file() and 'test-saves' not in path.relative_to(source).parts:
            archive(path,target/path.relative_to(source))
logs={
 'native-ctest.log':'heroesiii-0.52-ctest.log',
 'asan-ctest.log':'heroesiii-0.52-asan-final-ctest.log',
 'distribution-ctest.log':'heroesiii-0.52-distribution-final-ctest.log',
}
for target,name in logs.items():
    source=pathlib.Path('/private/tmp')/name
    assert '100% tests passed out of 17' in source.read_text(),name
    archive(source,OUT/target)
for kind in ['native','native-sanitized','distribution']:
    source=ROOT/'build'/kind/'Testing/Temporary/LastTest.log'
    assert '390 hero, creature and combination checks passed' in source.read_text()
    archive(source,OUT/(kind+'-test-details.log'))
full=pathlib.Path('/private/tmp/heroesiii-0.52-full-ui-fixed5')
assert 'Native UI flow passed. Rendered 478 screens' in pathlib.Path(str(full)+'.log').read_text()
tree(full,OUT/'full-ui');archive(pathlib.Path(str(full)+'.log'),OUT/'full-ui.log')
ui={}
for mode,expected in [('specialty',29),('completion',36),('location',106),('siege',50)]:
    source=pathlib.Path('/private/tmp/heroesiii-0.52-acceptance-'+mode)
    assert len(list(source.glob('*.png')))==expected
    tree(source,OUT/mode);ui[mode]=expected
assert read(OUT/'siege/siege-ui-audit.json')['videoMapVictoryDisplayed']
for kind in ['release','asan']:
    source=pathlib.Path('/private/tmp/heroesiii-0.52-'+kind+'-specialty-ui')
    assert read(source/'specialty-ui-audit.json')['screenshots']==29
    tree(source,OUT/(kind+'-specialty-ui'))
comparison=read(pathlib.Path('/private/tmp/heroesiii-0.52-pixel-comparison.json'))
assert all(x['byteIdenticalPng']==29 and not x['different'] for x in comparison)
archive(pathlib.Path('/private/tmp/heroesiii-0.52-pixel-comparison.json'),OUT/'pixel-comparison.json')
campaigns={}
for kind,sourceName,day in [('native','verified-native-impossible-final',63),('normal','verified-normal',97),('release','verified-release-impossible',63),('asan','verified-asan-impossible',63)]:
    source=pathlib.Path('/private/tmp/heroesiii-0.52-'+sourceName);audit=read(source/'campaign-audit.json')
    assert audit['completed'] and audit['result']==1 and audit['reloads']==0 and audit['normalGameplayOnly'] and audit['day']==day
    tree(source,OUT/'dead-and-buried'/kind)
    archive(pathlib.Path(str(source)+'.log'),OUT/'dead-and-buried'/(kind+'.log'))
    campaigns[kind]={k:v for k,v in audit.items() if k not in ['turns','battles','computerActivity']}
    campaigns[kind]['battles']=len(audit['battles']);campaigns[kind]['sieges']=sum(x.get('fort',0)>0 for x in audit['battles'])
sourceFiles=sorted(p for p in (ROOT/'native').rglob('*')if p.suffix in ['.hpp','.cpp','.inc','.mm'])
digest=hashlib.sha256()
for path in sourceFiles:digest.update(str(path.relative_to(ROOT)).encode()+b'\0'+path.read_bytes()+b'\0')
bundle=read(pathlib.Path('/private/tmp/heroesiii-0.52-bundle-final.json'));package=read(ROOT/'dist/macos-package.json')
assert bundle['version']==package['version']=='0.52.0' and bundle['strictSignatureValid'] and bundle['systemLibrariesOnly']
for name,entry in package['files'].items():
    path=ROOT/'dist'/name;assert path.stat().st_size==entry['bytes'] and hashlib.sha256(path.read_bytes()).hexdigest()==entry['sha256']
archive(pathlib.Path('/private/tmp/heroesiii-0.52-bundle-final.json'),OUT/'bundle.json');archive(ROOT/'dist/macos-package.json',OUT/'packages.json')
bundle['bundle']=bundle['bundle'].removeprefix(str(ROOT)+'/')
bundle['runtime']['dataRoot']=bundle['runtime']['dataRoot'].removeprefix(str(ROOT)+'/')
record={'version':'0.52.0','nativeSourceSha256':digest.hexdigest(),'rules':{'heroSpecialtyRecords':156,'creatureRecords':141,'combinationRecipes':12,'specialtyRegressionChecks':390,'connectedAbilityCategories':43,'partiallyVerifiedAbilityCategories':10,'classificationOnlyCategories':3},'tests':{'native':17,'addressAndUndefinedBehaviorSanitizers':17,'universalDistribution':17},'ui':{'fullDeclaredScenes':478,'fullPngFiles':len(list(full.glob('*.png'))),'specialty':ui['specialty'],'completion':ui['completion'],'locations':ui['location'],'siegeAndEnding':ui['siege'],'newUiByteIdenticalAcrossThreeBuilds':29},'campaigns':campaigns,'packages':package,'bundle':bundle,'sourceVideo':'https://www.bilibili.com/video/BV1vu41177kZ/','localEvidence':'references/validation/0.52','personalSaveUnchanged':True,'limits':['This is not a complete original-game reproduction.','Reference video wins on day 19 at 200%; this engine wins on day 63.','Original-executable timing, lightning beam layers and some creature effect visuals remain uncalibrated.','All ordinary artifact and immunity interactions are not exhaustively verified.','Universal binaries were exercised on this Apple Silicon Mac, not an Intel Mac or iPad.']}
(ROOT/'data/native/validation-0.52.json').write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n')
print(json.dumps({'version':record['version'],'nativeSourceSha256':digest.hexdigest(),'ui':record['ui'],'campaignDays':{k:v['day']for k,v in campaigns.items()}},ensure_ascii=False))
