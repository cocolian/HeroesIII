#!/usr/bin/env python3
"""Recolor original indexed UI using the original PLAYERS.PAL ramp."""
import json
import struct
from pathlib import Path
from h3_resources import chunk
from original_resources import OriginalResources

ROOT=Path(__file__).resolve().parents[1]
resources=OriginalResources(ROOT)
palette=resources.read('players.pal')
assert palette[:4]==b'RIFF' and struct.unpack_from('<HH',palette,20)==(0x300,256)
players=[palette[24+i*4:27+i*4] for i in range(256)]
manifest=json.loads((ROOT/'assets/demo/manifest.json').read_text())
path=ROOT/'assets/demo/ownership.json'
ownership=json.loads(path.read_text())
interfaces={}
for name in ['advmap.pcx','aresbar.pcx','heroscr4.pcx','cbar.pcx']:
    source=(ROOT/manifest['pcx'][name]['file']).read_bytes()
    outputs={}
    for color,info in ownership['colors'].items():
        if info['paletteIndex']<0: continue
        result=bytearray(source[:8]);at=8
        while at<len(source):
            length=int.from_bytes(source[at:at+4],'big');kind=source[at+4:at+8];payload=source[at+8:at+8+length]
            if kind==b'PLTE':
                modified=bytearray(payload)
                for i in range(32): modified[(224+i)*3:(225+i)*3]=players[info['paletteIndex']*32+i]
                payload=bytes(modified)
            result.extend(chunk(kind,payload));at+=12+length
        output=Path('assets/demo/interface-colors')/color/(Path(name).stem+'.png')
        (ROOT/output).parent.mkdir(parents=True,exist_ok=True);(ROOT/output).write_bytes(result)
        outputs[color]=str(output)
    interfaces[name]={'files':outputs,'source':resources.source(name),'paletteSource':resources.source('players.pal'),
                      'paletteRange':[224,255],'indexedPixelDataUnchanged':True}
# The verified Mac archive uses these letter suffixes; they are not color initials.
for color,letter in {'red':'l','blue':'g','tan':'r','green':'d','orange':'b','purple':'p','teal':'w','pink':'k'}.items():
    name='abf01'+letter+'.def'
    ownership['colors'][color]['boatFlag']=name
    ownership['colors'][color]['boatFlagSource']=resources.source(name)
ownership['interface']=interfaces
path.write_text(json.dumps(ownership,separators=(',',':'))+'\n')
print('Generated original player UI palettes for 4 panels and 8 colors')
