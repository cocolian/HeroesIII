#!/usr/bin/env python3
"""Verify all embedded runtime files and independent resource discovery."""
import argparse,hashlib,json,plistlib,subprocess,tempfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('bundle',type=Path);a=p.parse_args();bundle=a.bundle.resolve();data=bundle/'Contents/Resources/GameData';binary=bundle/'Contents/MacOS/HeroesIII';manifest=json.loads((data/'runtime-manifest.json').read_text())
for name,entry in manifest['files'].items():
    f=data/name
    assert f.is_file() and f.stat().st_size==entry['bytes'] and hashlib.sha256(f.read_bytes()).hexdigest()==entry['sha256'],name
with tempfile.TemporaryDirectory(prefix='heroesiii-relocation-') as tmp:
    text=subprocess.check_output([str(binary),'--runtime-info','--save-dir',tmp],cwd=tmp,text=True);runtime=json.loads(text)
assert runtime['bundledData'] and not runtime['sourceFallbackEnabled'] and Path(runtime['dataRoot'])==data,runtime
subprocess.run(['codesign','--verify','--strict',str(bundle)],check=True)
deps=subprocess.check_output(['otool','-L',str(binary)],text=True);assert '/opt/homebrew' not in deps and '@rpath' not in deps
info=plistlib.loads((bundle/'Contents/Info.plist').read_bytes())
print(json.dumps({'bundle':str(bundle),'version':info['CFBundleShortVersionString'],'minimumMacOS':info['LSMinimumSystemVersion'],'verifiedAssetFiles':len(manifest['files']),'runtime':runtime,'systemLibrariesOnly':True,'strictSignatureValid':True},ensure_ascii=False,indent=2))
