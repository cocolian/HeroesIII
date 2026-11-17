#!/usr/bin/env python3
"""Download a bounded public gameplay excerpt for manual visual review."""
import hashlib
import argparse
import json
import subprocess
import urllib.request
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'references/video-review-0.37'
OUT.mkdir(parents=True,exist_ok=True)
BV='BV16K4y1x7Gj'
parser=argparse.ArgumentParser();parser.add_argument('--start',type=int,default=0);parser.add_argument('--duration',type=int,default=180);args=parser.parse_args()
if not 0<=args.start<=12000 or not 1<=args.duration<=240:
    raise ValueError('Excerpt outside bounded reference range')
def request(endpoint):
    url='https://api.bilibili.com/'+endpoint
    with urllib.request.urlopen(urllib.request.Request(url,headers={'User-Agent':'Mozilla/5.0'}),timeout=30) as response:
        data=json.load(response)
    if data['code']!=0:
        raise ValueError('Public API rejected '+endpoint)
    return data
view=request('x/web-interface/view?bvid='+BV)
(OUT/'view.json').write_text(json.dumps(view,ensure_ascii=False,indent=2)+'\n')
page=view['data']['pages'][0]
play=request(f'x/player/playurl?bvid={BV}&cid={page["cid"]}&qn=64&fnval=0&platform=html5')
url=play['data']['durl'][0]['url']
clip=Path(f'/private/tmp/h3-new-video-p01-{args.start}-{args.duration}.mp4')
with open('/private/tmp/heroesiii-0.37-video-clip.log','w') as log:
    subprocess.run(['/opt/homebrew/bin/ffmpeg','-hide_banner','-loglevel','warning','-y',
                    '-user_agent','Mozilla/5.0','-headers','Referer: https://www.bilibili.com/\r\n',
                    '-rw_timeout','20000000','-ss',str(args.start),'-i',url,'-t',str(args.duration),'-an','-c:v','copy',str(clip)],
                   check=True,stdout=log,stderr=log,timeout=180)
(OUT/f'clip-source-{args.start}-{args.duration}.json').write_text(json.dumps({'video':'https://www.bilibili.com/video/'+BV+'/',
    'uploader':view['data']['owner']['name'],'title':view['data']['title'],
    'page':1,'cid':page['cid'],'part':page['part'],'requestedSourceSeconds':[args.start,args.start+args.duration],
    'timestampPrecision':'Copied excerpt may start at the preceding video keyframe; review timestamps are approximate.',
    'sha256':hashlib.sha256(clip.read_bytes()).hexdigest(),'bytes':clip.stat().st_size,
    'scope':'Public gameplay reference only. SoD campaign; HD interface may be present.',
    'collectedOn':'2026-11-15'},ensure_ascii=False,indent=2)+'\n')
print('Downloaded bounded excerpt for manual encounter review',args.start,args.duration,flush=True)
