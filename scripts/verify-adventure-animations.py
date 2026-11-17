#!/usr/bin/env python3
"""Verify actual post-visit PNG pixels, excluding the nearby hero's flag."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('directory',type=Path)
args = parser.parse_args()
manifest = json.loads((ROOT/'assets/demo/manifest.json').read_text())['def']
templates = json.loads((ROOT/'assets/demo/adventure.json').read_text())['templates']
items = [(6,'avxwelg0.def',False),(10,'avmwndd0.def',True),
         (11,'avmwwhl0.def',True),(8,'avxoblg.def',False),(5,'avxl1sh0.def',True)]
results = []
for kind,asset,animated in items:
    canvas = manifest[asset]['groups']['0'][0]['canvas']
    dx,dy = templates[asset]['visitOffset']
    # Fixture: 1280x800, zoom=1, object=(12,10), camera clamped to
    # (16.75,11.625). Sample the rotor/head, away from the hero at (10,12).
    x,y = 408-dx*32+16-canvas[0],344-dy*32+16-canvas[1]
    w,h = canvas
    if w>64:
        h-=32
    hashes = []
    for frame in [0,1]:
        png = args.directory/f'visit-animation-{kind}-{frame}.png'
        pixels = subprocess.check_output(['/opt/homebrew/bin/ffmpeg','-v','error',
            '-i',str(png),'-vf',f'crop={w}:{h}:{x}:{y}',
            '-f','rawvideo','-pix_fmt','rgba','-'])
        hashes.append(hashlib.sha256(pixels).hexdigest())
    assert (hashes[0]!=hashes[1])==animated,(asset,hashes)
    results.append({'asset':asset,'crop':[x,y,w,h],'afterVisit':True,
                    'expectedAnimated':animated,'framePixelHashes':hashes})
record = {'sameFramesAfterVisited':True,'originalStaticObjectsRemainStatic':True,
          'windmillWaterWheelAndShrinePixelsChange':True,'objects':results}
(args.directory/'animation-pixels.json').write_text(json.dumps(record,indent=2)+'\n')
print('Verified actual post-visit pixels for 5 original map objects')
