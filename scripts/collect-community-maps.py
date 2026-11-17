#!/usr/bin/env python3
"""Collect selected public SoD map data, never executables or ad redirects."""
import concurrent.futures
import argparse
import gzip
import hashlib
import html
import io
import json
import re
import struct
import subprocess
import urllib.request
import zipfile
from pathlib import Path
from h3m_terrain import read_terrain

ROOT = Path(__file__).resolve().parents[1]
SITE = 'https://www.maps4heroes.com'
SELECTED = [
    (791, 'wayfarer', 'Wayfarer (E)', 'ustcers', 'Impossible', '高难寻路、资源和战术解谜'),
    (974, 'lord-of-the-rings', 'Lord of The Rings 3.0', 'Vallex', 'Expert', '魔戒剧情与探索；保留戒指并阅读事件提示'),
    (369, 'empire-of-the-world-2', 'The Empire Of The World 2', 'Darklord', 'Impossible', '长篇单人冒险；注意压缩包内说明及攻略'),
    (907, 'ira-the-mage', 'Ira the Mage', 'Liu Xue Lian', 'Expert', '中文剧情；作者要求按说明设置电脑并使用 200% 难度'),
    (1610, 'to-tame-a-land', 'To Tame a Land', 'Jaakko Minkkinen', 'Impossible', '高难生存、寻路和战术解谜'),
    (1756, 'war-of-seven-realms', 'War of the 7 realms', 'Barbuta', 'Expert', '多国对抗与大型地图探索'),
]
parser = argparse.ArgumentParser()
parser.add_argument('--batch', choices=['0.37', '0.38'], default='0.37')
args = parser.parse_args()
if args.batch == '0.38':
    SELECTED = [
        (67, 'the-lone-knight', 'The Lone Knight', 'Hans C.', 'Impossible', '四个可选人类势力、剧情探索与支线；长篇地图'),
        (12, 'seven-lakes', 'Seven Lakes', 'Oscar Sedell', 'Normal', '湖区探索与多方对抗；注意此版本作者为 Oscar Sedell，不是同名 HotA 地图'),
        (1847, 'jennifers-dream-journey', "Jennifer's Dream Journey (CHI)", 'grizliukas', 'Expert', '中文长篇剧情；隐藏路径多，难度很高，建议熟悉原版后再挑战'),
        (888, 'jedi-story', 'Jedi Story', 'fourzy', 'Expert', '单人长篇剧情与战术挑战；需阅读地图事件'),
    ]
REFERENCE = ROOT/('references/community-maps-'+args.batch)
REFERENCE.mkdir(parents=True, exist_ok=True)

def fetch(url, limit=32*1024*1024):
    if not url.startswith(SITE+'/'):
        raise ValueError('Download host changed: '+url)
    request = urllib.request.Request(url, headers={'User-Agent':'Mozilla/5.0'})
    with urllib.request.urlopen(request, timeout=45) as response:
        if not response.url.startswith(SITE+'/'):
            raise ValueError('Unexpected redirect')
        result = response.read(limit+1)
    if len(result)>limit:
        raise ValueError('Oversized download')
    return result

def collect(selection):
    mid, slug, title, author, difficulty, description = selection
    folder = ROOT/'maps/community'/slug
    folder.mkdir(parents=True, exist_ok=True)
    source = f'{SITE}/forum/opinions.php?game=3&map_id={mid}'
    detail_path = REFERENCE/f'{slug}.html'
    detail = detail_path.read_text() if detail_path.exists() else fetch(source).decode('utf-8')
    detail_path.write_text(detail)
    if 'Heroes 3: The Shadow of Death' not in detail:
        raise ValueError(title+': page does not identify SoD')
    score = re.search(r'map rating\s*=\s*(\d+)', detail, re.I)
    if not score or int(score[1])<=0:
        raise ValueError(title+': positive public rating not found')
    download_path = REFERENCE/f'{slug}-download.html'
    download_page = download_path.read_text() if download_path.exists() else fetch(f'{SITE}/heroes3/rating.php?testcookie=&id={mid}').decode('utf-8')
    download_path.write_text(download_page)
    links = re.findall(r'''href="(https://www\.maps4heroes\.com/heroes3/maps/[A-Za-z0-9_'\-.%()]+\.(?:zip|rar))"''', html.unescape(download_page))
    if len(links)!=1:
        raise ValueError(title+': unambiguous direct ZIP link not found')
    url = html.unescape(links[0]);archive_path=folder/('original.'+url.rsplit('.',1)[1])
    archive = archive_path.read_bytes() if archive_path.exists() else fetch(url)
    archive_path.write_bytes(archive)
    files = []
    def members():
        if archive_path.suffix=='.zip':
            with zipfile.ZipFile(io.BytesIO(archive)) as zipped:
                if len(zipped.infolist())>100:
                    raise ValueError('Excessive ZIP entries')
                for entry in zipped.infolist():
                    if entry.file_size>16*1024*1024:
                        raise ValueError('Oversized ZIP member')
                    yield entry.filename, lambda entry=entry: zipped.read(entry)
        else:
            listing = subprocess.run(['/usr/bin/tar','-tf',str(archive_path)], check=True, capture_output=True,timeout=20).stdout.decode('utf-8')
            entries = listing.splitlines()
            if len(entries)>100:
                raise ValueError('Excessive RAR entries')
            def read(name):
                process = subprocess.Popen(['/usr/bin/tar','-xOf',str(archive_path),'--',name],stdout=subprocess.PIPE,stderr=subprocess.DEVNULL)
                data = process.stdout.read(16*1024*1024+1)
                if len(data)>16*1024*1024:
                    process.kill();process.wait();raise ValueError('Oversized RAR member')
                if process.wait(timeout=20):
                    raise ValueError('RAR extraction failed')
                return data
            for name in entries:
                yield name, lambda name=name: read(name)
    for member, read in members():
            relative = Path(member)
            if relative.is_absolute() or '..' in relative.parts:
                raise ValueError('Unsafe archive member')
            if relative.suffix.lower()!='.h3m':
                continue
            data = read()
            with gzip.GzipFile(fileobj=io.BytesIO(data)) as compressed:
                raw = compressed.read(64*1024*1024+1)
            if len(raw)>64*1024*1024 or len(raw)<18:
                raise ValueError('Invalid H3M expansion size')
            version, = struct.unpack_from('<I', raw)
            size, = struct.unpack_from('<I', raw, 5)
            name_length, = struct.unpack_from('<I', raw, 10)
            if version!=28 or size not in (36,72,108,144) or name_length>4096 or 14+name_length>len(raw):
                raise ValueError('Not a classic SoD H3M header')
            description_at = 14+name_length
            description_length, = struct.unpack_from('<I',raw,description_at)
            difficulty_at = description_at+4+description_length
            if description_length>65536 or difficulty_at>=len(raw) or raw[difficulty_at]>4:
                raise ValueError('Invalid map description/difficulty header')
            path = folder/(str(len(files)+1)+'.h3m');path.write_bytes(data)
            files.append({'file':str(path.relative_to(ROOT)), 'originalArchiveMember':member,
                          'sizeBytes':len(data), 'sha256':hashlib.sha256(data).hexdigest(),
                          'formatVersion':version, 'mapSize':[size,size], 'levels':2 if raw[9] else 1,
                          'difficulty':['Easy','Normal','Hard','Expert','Impossible'][raw[difficulty_at]],
                          'runtimeImportSupported':False})
    if not files:
        raise ValueError('No SoD maps in ZIP')
    result = {'title':title,'author':author,'source':source,'download':url,
              'siteRatingLabel':'Likes (public site count, not a 5-star rating)',
              'siteRating':int(score[1]),'difficultyLabel':files[0]['difficulty'],'notes':description,
              'archive':str(archive_path.relative_to(ROOT)), 'archiveSha256':hashlib.sha256(archive).hexdigest(),
              'mapFiles':files,'downloadedOn':'2026-11-15','playtestedInThisEngine':False}
    print(title, 'SoD files:', len(files), 'Likes:',score[1],flush=True)
    (folder/'record.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
    return result

results, errors = [], []
if args.batch == '0.38':
    existing = ROOT/'maps/community/catalog.json'
    if existing.exists():
        results = json.loads(existing.read_text())['maps']
with concurrent.futures.ThreadPoolExecutor(max_workers=3) as executor:
    futures = {executor.submit(collect, selected):selected[2] for selected in SELECTED}
    for future in concurrent.futures.as_completed(futures):
        try:
            results.append(future.result())
        except Exception as error:
            errors.append({'title':futures[future],'error':str(error)})
            print('FAILED',futures[future],str(error),flush=True)
results = list({item['source']:item for item in results}.values())
for item in results:
    for file in item['mapFiles']:
        terrain = read_terrain(ROOT/file['file'])
        file['terrainBlockValidated'] = True
        file['terrainOffset'] = terrain['terrainOffset']
        file['terrainTileCount'] = len(terrain['tiles'])
        file['fullScenarioValidated'] = False
    (ROOT/item['archive']).parent.joinpath('record.json').write_text(json.dumps(item,ensure_ascii=False,indent=2)+'\n')
catalog = {'schema':1,'collectedOn':'2026-11-15','maps':results,
           'runtimeCompatibility':'Original SoD header and terrain block validated; objects/events are not parsed. This engine does not yet import H3M scenarios.',
           'archivePolicy':'Archives retained unchanged; only bounded .h3m data extracted; no downloaded code executed.', 'errors':errors}
(ROOT/'maps/community/catalog.json').write_text(json.dumps(catalog,ensure_ascii=False,indent=2)+'\n')
lines = ['# 已收集的死亡阴影社区地图', '',
         '已下载并核对 SoD 格式头、尺寸、层数和地形区块。未解析完整对象/事件规则。当前程序尚不支持导入 H3M，以下地图不能在当前程序中直接开始游戏，也未在本项目中通关。', '',
         '站点评分为 Likes 数量，不是星级；包含普通难度湖区探索，以及高难度长篇冒险。原始 ZIP/RAR 内的说明、攻略仍保留。', '',
         '| 地图 | 作者 | 站点 Likes | 玩法及说明 |', '|---|---|---:|---|']
for item in results:
    lines.append(f"| [{item['title']}]({item['source']}) | {item['author']} | {item['siteRating']} | {item['notes']} |")
lines += ['', '来源、下载地址、校验和及每张地图的格式信息见 `catalog.json`。采集日期：2026-11-15。', '']
(ROOT/'maps/community/README.md').write_text('\n'.join(lines))
