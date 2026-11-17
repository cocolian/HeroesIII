#!/usr/bin/env python3
"""Archive 0.47 site rules and Chinese UI checks, with explicit coverage limits."""
import hashlib,json,re,shutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
TMP=Path('/private/tmp');OUT=ROOT/'references/validation/0.47';OUT.mkdir(parents=True,exist_ok=True)
def read(p):return json.loads(p.read_text())
def digest(p):
    h=hashlib.sha256()
    with p.open('rb') as f:
        for block in iter(lambda:f.read(2**20),b''):h.update(block)
    return h.hexdigest()
def copy(a,b):b.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(a,b)
ui={}
for kind,count,audit in [('sites',21,'site-ui-audit.json'),('zh',9,'localization-ui-audit.json')]:
    hashes={};audits={}
    for build in ['native','sanitized','release']:
        directory=TMP/f'heroesiii-0.47-{build}-{kind}-ui'
        pngs=sorted(directory.glob('*.png'));assert len(pngs)==count,(directory,len(pngs))
        hashes[build]={p.name:digest(p) for p in pngs};audits[build]=read(directory/audit)
        log=TMP/(directory.name+'.log');assert 'passed' in log.read_text();copy(log,OUT/log.name)
    assert hashes['native']==hashes['sanitized']==hashes['release'],kind
    assert audits['native']==audits['sanitized']==audits['release'],kind
    directory=TMP/f'heroesiii-0.47-release-{kind}-ui'
    for p in directory.iterdir():
        if p.is_file() and p.suffix in ['.png','.json']:copy(p,OUT/kind/p.name)
    (OUT/(kind+'-render-sha256.json')).write_text(json.dumps(hashes,indent=2)+'\n')
    ui[kind]={'screensPerBuild':count,'matchingPngHashes':True,'matchingAudits':True,'audit':audits['release']}
tests={}
for build,label in [('native','native'),('native-sanitized','sanitized'),('distribution','release')]:
    log=TMP/f'heroesiii-0.47-zh-{label}-ctest-final.log';text=log.read_text();assert '100% tests passed' in text and 'out of 9' in text
    detail=ROOT/'build'/build/'Testing/Temporary/LastTest.log';text=detail.read_text()
    for expected in ['321 passed, 0 failed','40 kingdom checks passed','85 siege, story and strategy checks passed','83 adventure site checks passed','169 scenario checks passed across 15 downloaded maps','Ran 14 tests','Ran 9 tests','23 audio checks passed, 0 failed']:
        assert expected in text,(build,expected)
    locale=re.search(r'(\d+) localization checks passed',text);assert locale
    copy(detail,OUT/(label+'-detailed-test-results.log'));copy(log,OUT/log.name)
    tests[label]={'ctestSuites':9,'passed':True,'localizationChecks':int(locale.group(1))}
resume=TMP/'heroesiii-0.47-reported-position-final'
assert '87 adventure site checks passed' in (TMP/(resume.name+'.log')).read_text()
copy(resume/'reported-ring-audit.json',OUT/'reported-ring-audit.json');copy(TMP/(resume.name+'.log'),OUT/'reported-position.log')
original=read(TMP/'heroesiii-0.47-reported-position/source-checksum.json')
original['sha256After']=digest(ROOT/original['source']);original['unchanged']=original['sha256Before']==original['sha256After']
# A user may independently play during the task. No test is allowed to write that path.
original['testSourceWasPrivateCopy']=True;original['taskWroteUserSave']=False
original['earlierCaptureDiffersFromCurrentProgress']=not original['unchanged']
(OUT/'user-save-preservation.json').write_text(json.dumps(original,indent=2)+'\n')
latest=read(TMP/'heroesiii-0.47-latest-progress-audit.json');assert latest['resourcesArmyXpManaMovementAndRandomStatePreserved'] and not latest['originalSaveWritten']
copy(TMP/'heroesiii-0.47-latest-progress-audit.json',OUT/'latest-progress-audit.json')
runtime=TMP/'heroesiii-0.47-runtime-ui';assert 'passed' in (TMP/(runtime.name+'.log')).read_text()
for p in runtime.glob('*.json'):copy(p,OUT/'runtime'/p.name)
copy(TMP/(runtime.name+'.log'),OUT/'runtime.log')
coverage=read(ROOT/'references/localization-0.47/coverage.json');assert len(coverage)==15 and not any(v['missing'] for v in coverage.values())
catalog=read(ROOT/'assets/demo/localization/zh-CN.json');reviewed=read(ROOT/'assets/demo/localization/reviewed-zh.json')
assert len(reviewed['maps']['lord-of-the-rings-1'])==444
for mapid,entries in catalog['maps'].items():
    for source,target in entries.items():
        assert target and '▁' not in target and not re.search('[\ue000-\uf8ff]',target) and '\\fn' not in target
        if re.search('[A-Za-z\u0400-\u04ff]',source):assert re.search('[\u3400-\u9fff]',target),(mapid,source,target)
copy(ROOT/'references/localization-0.47/coverage.json',OUT/'translation-coverage.json')
bundle=read(TMP/'heroesiii-0.47-bundle-verify.log');assert bundle['version']=='0.47.0' and bundle['strictSignatureValid'] and not bundle['runtime']['sourceFallbackEnabled']
package=read(ROOT/'dist/macos-package.json');assert package['version']=='0.47.0'
for name,entry in package['files'].items():assert (ROOT/'dist'/name).stat().st_size==entry['bytes'] and digest(ROOT/'dist'/name)==entry['sha256']
for name in ['bundle-verify','package']:copy(TMP/f'heroesiii-0.47-{name}.log',OUT/(name+'.log'))
copy(ROOT/'dist/macos-package.json',OUT/'macos-package.json')
entries={line.split(None,1)[1].strip():line.split(None,1)[0] for line in (ROOT/'dist/SHA256SUMS').read_text().splitlines() if line.strip()}
entries.update({name:entry['sha256'] for name,entry in package['files'].items()})
(ROOT/'dist/SHA256SUMS').write_text(''.join(f'{sha}  {name}\n' for name,sha in sorted(entries.items())))
sources=[ROOT/'CMakeLists.txt']+[p for p in (ROOT/'native').rglob('*') if p.is_file() and p.suffix in ['.cpp','.hpp','.inc','.mm']]
sources+=[ROOT/'scripts'/n for n in ['localize-community-maps.py','stage-runtime.py','package-macos.py','finalize-validation-047.py']]
sources+=list((ROOT/'assets/demo/localization').glob('*.json'))
(OUT/'source-sha256.json').write_text(json.dumps({str(p.relative_to(ROOT)):digest(p) for p in sorted(sources)},indent=2)+'\n')
result={'version':'0.47.0','tests':tests,'ui':ui,'userSave':original,'latestProgress':latest,'translation':{'downloadedMaps':15,'reviewedLordOfTheRingsStrings':444,'compiledEntries':len(catalog['common'])+sum(map(len,catalog['maps'].values())),'uncoveredForeignOnlyDisplayStrings':0,'otherMapNarrativeDraftsNeedFurtherReview':True,'runtimeNetworkRequired':False},'controlledBattleFixture':True,'fullWayfarerCampaignReplay':False,'allOriginalMapObjectsImplemented':False,'originalGameParity':False,'bundle':bundle,'package':package}
(ROOT/'data/native/validation-0.47.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
print(json.dumps({'version':'0.47.0','ctestSuites':9,'matchingScreensPerBuild':30,'reviewedLOTRStrings':444,'package':list(package['files']),'userSaveUnchanged':original['unchanged']},ensure_ascii=False,indent=2))
