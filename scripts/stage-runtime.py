#!/usr/bin/env python3
"""Stage only runtime files, excluding source downloads and tests."""
import argparse,hashlib,json,shutil
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
def stage(destination):
    destination=Path(destination).resolve()
    # Only explicit disposable staging directories may be replaced by callers.
    destination.mkdir(parents=True,exist_ok=True)
    prepacked=ROOT/'GameData'
    if (prepacked/'runtime-manifest.json').is_file():
        shutil.copytree(prepacked,destination,dirs_exist_ok=True)
        print(json.dumps({'directory':str(destination),'prepacked':True}));return
    files={ROOT/'assets/manifest.json'}
    catalog=json.loads((ROOT/'assets/manifest.json').read_text())
    files.update(ROOT/v['url'].lstrip('/') for v in catalog.values())
    for folder in ['assets/demo','assets/hd','assets/audio','assets/fonts']:
        files.update(p for p in (ROOT/folder).rglob('*') if p.is_file() and p.suffix not in ['.source','.zip'])
    for name in ['menu-original.jpg','adventure-original.png','hero-original.png']:
        files.add(ROOT/'references'/name)
    maps=json.loads((ROOT/'maps/community/catalog.json').read_text())
    files.add(ROOT/'maps/community/catalog.json')
    for group in maps['maps']:
        for entry in group['mapFiles']:
            relative=Path(entry['file'])
            if relative.is_absolute() or '..' in relative.parts or relative.suffix.lower()!='.h3m':
                raise ValueError('Unsafe runtime map path')
            source=ROOT/relative
            if hashlib.sha256(source.read_bytes()).hexdigest()!=entry['sha256']:
                raise ValueError('Map checksum mismatch: '+str(relative))
            files.add(source)
    manifest={}
    for source in sorted(files):
        relative=source.relative_to(ROOT);target=destination/relative
        if not source.is_file():raise FileNotFoundError(source)
        target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,target)
        manifest[str(relative)]={'bytes':target.stat().st_size,'sha256':hashlib.sha256(target.read_bytes()).hexdigest()}
    licenses=destination/'Licenses';licenses.mkdir(exist_ok=True)
    for name,path in {'SDL2.txt':'h3_sdl-src/LICENSE.txt','SDL2_image.txt':'h3_image-src/LICENSE.txt','SDL2_ttf.txt':'h3_ttf-src/LICENSE.txt','FreeType.txt':'h3_freetype-src/docs/FTL.TXT','nlohmann-json.txt':'h3_json-src/LICENSE.MIT'}.items():
        source=ROOT/'build/distribution/_deps'/path
        if source.is_file():shutil.copy2(source,licenses/name)
    shutil.copy2(ROOT/'assets/fonts/OFL.txt',licenses/'NotoSansSC-OFL.txt')
    shutil.copy2(ROOT/'references/h3m-import-0.44/h3mtxt-master/LICENSE',licenses/'h3mtxt-MIT.txt')
    (destination/'runtime-manifest.json').write_text(json.dumps({'schema':1,'files':manifest},ensure_ascii=False,indent=2)+'\n')
    print(json.dumps({'directory':str(destination),'files':len(manifest),'bytes':sum(v['bytes'] for v in manifest.values())}))
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('destination');args=parser.parse_args();stage(args.destination)
