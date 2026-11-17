#!/usr/bin/env python3
"""Package native iPad build inputs; this deliberately does not claim to be an IPA."""
import hashlib,json,plistlib,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];version='0.28.0';out=ROOT/'dist';out.mkdir(exist_ok=True)
archive=out/f'HeroesIII-{version}-iPad-native-build-kit.zip';prefix=f'HeroesIII-iPad-{version}'
files={ROOT/'CMakeLists.txt':'CMakeLists.txt',ROOT/'native/ios/README.md':'README.md'}
for folder in ['native/src','native/include','native/generated','native/macos','native/ios','native/tests','cmake']:
 for p in (ROOT/folder).rglob('*'):
  if p.is_file():files[p]=str(p.relative_to(ROOT))
for name in ['build-ios.py','stage-runtime.py']:
 p=ROOT/'scripts'/name;files[p]=f'scripts/{name}'
for p in (ROOT/'build/vendor-downloads').iterdir():
 if p.suffix in ['.gz','.json']:files[p]=str(p.relative_to(ROOT))
for p in (ROOT/'build/staged-runtime/GameData').rglob('*'):
 if p.is_file():files[p]='GameData/'+str(p.relative_to(ROOT/'build/staged-runtime/GameData'))
with zipfile.ZipFile(archive,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=6,allowZip64=True) as z:
 for p,name in sorted(files.items()):z.write(p,prefix+'/'+name)
 z.writestr(prefix+'/BUILD-STATUS.json',json.dumps({'version':version,'platform':'iPadOS native C++ / SDL2','target':'iPad mini 7','minimumDeploymentTarget':'18.0','isInstallableIpa':False,'iOSCompileVerified':False,'deviceTested':False,'blockedBy':['Full compatible Xcode with iOS SDK is not installed','No Apple code-signing identity exists on this Mac'],'instructions':'Install compatible Xcode, log in using your Apple account, supply your Team ID to scripts/build-ios.py; then compile and test on your iPad.'},ensure_ascii=False,indent=2)+'\n')
record={'version':version,'file':archive.name,'bytes':archive.stat().st_size,'sha256':hashlib.sha256(archive.read_bytes()).hexdigest(),'isInstallableIpa':False,'files':len(files)+1,'includesPinnedDependencyArchives':True,'includesRuntimeAssets':True}
(out/'ipad-build-kit.json').write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n');print(json.dumps(record,ensure_ascii=False,indent=2))
