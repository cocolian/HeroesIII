#!/usr/bin/env python3
"""Collect a bounded public SoD campaign excerpt for frame-by-frame review."""
import argparse
import hashlib
import json
import subprocess
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT/'references/video-review-0.38'
OUT.mkdir(parents=True, exist_ok=True)
parser = argparse.ArgumentParser()
parser.add_argument('--start', type=int, default=360)
parser.add_argument('--duration', type=int, default=240)
args = parser.parse_args()
if not 0 <= args.start <= 12000 or not 1 <= args.duration <= 240:
    raise ValueError('Excerpt outside bounded range')

def request(endpoint):
    req = urllib.request.Request('https://api.bilibili.com/'+endpoint,
                                 headers={'User-Agent':'Mozilla/5.0'})
    with urllib.request.urlopen(req, timeout=30) as response:
        result = json.load(response)
    if result['code'] != 0:
        raise ValueError('Public API rejected request')
    return result['data']

bv = 'BV16K4y1x7Gj'
view = request('x/web-interface/view?bvid='+bv)
page = view['pages'][0]
play = request(f'x/player/playurl?bvid={bv}&cid={page["cid"]}&qn=64&fnval=0&platform=html5')
clip = Path(f'/private/tmp/h3-038-{args.start}-{args.duration}.mp4')
with open('/private/tmp/heroesiii-0.38-video.log', 'w') as log:
    subprocess.run(['/opt/homebrew/bin/ffmpeg','-hide_banner','-loglevel','warning','-y',
                    '-user_agent','Mozilla/5.0','-headers','Referer: https://www.bilibili.com/\r\n',
                    '-rw_timeout','20000000','-ss',str(args.start),'-i',play['durl'][0]['url'],
                    '-t',str(args.duration),'-an','-c:v','copy',str(clip)],
                   check=True, stdout=log, stderr=log, timeout=180)
(OUT/f'clip-{args.start}.json').write_text(json.dumps({
    'video':'https://www.bilibili.com/video/'+bv+'/', 'uploader':view['owner']['name'],
    'title':view['title'], 'part':page['part'], 'cid':page['cid'],
    'requestedSourceSeconds':[args.start,args.start+args.duration],
    'timestampPrecision':'Approximate: stream copied from preceding keyframe',
    'sha256':hashlib.sha256(clip.read_bytes()).hexdigest(),
    'bytes':clip.stat().st_size, 'collectedOn':'2026-11-15',
    'scope':'SoD campaign content; HD interface present; not an unmodified binary claim.'
}, ensure_ascii=False, indent=2)+'\n')
print('Downloaded', clip, flush=True)
