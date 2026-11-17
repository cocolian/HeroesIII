#!/usr/bin/env python3
"""Export opaque iPad icon PNGs from the existing game icon."""
import json,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1];base=root/'native/ios/Assets.xcassets';icons=base/'AppIcon.appiconset';icons.mkdir(parents=True,exist_ok=True)
with tempfile.TemporaryDirectory(prefix='heroesiii-icons-') as temporary:
 subprocess.run(['xcrun','swift','-module-cache-path',temporary,str(root/'native/ios/export-icon.swift'),str(root/'native/assets/app-icon.png'),str(icons)],check=True)
 images=[]
 for size,scale in [(20,1),(20,2),(29,1),(29,2),(40,1),(40,2),(76,1),(76,2),(83.5,2),(1024,1)]:
  pixels=int(size*scale);name=f'icon-{pixels}.png';target=icons/name
  details=subprocess.check_output(['sips','-g','hasAlpha',str(target)],text=True);assert 'hasAlpha: no' in details
  images.append({'size':f'{size}x{size}','idiom':'ios-marketing' if size==1024 else 'ipad','filename':name,'scale':f'{scale}x'})
 (icons/'Contents.json').write_text(json.dumps({'images':images,'info':{'version':1,'author':'xcode'}},indent=2)+'\n')
 (base/'Contents.json').write_text(json.dumps({'info':{'version':1,'author':'xcode'}},indent=2)+'\n')
print('Exported 10 opaque iPad app icons')
