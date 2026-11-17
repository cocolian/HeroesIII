"""Collect publicly indexed Heroes III artwork for local visual reconstruction.
Records source and SHA256. Does not claim a redistribution license.
"""
import sys
import subprocess
import concurrent.futures, hashlib, json, pathlib, re, urllib.request, urllib.parse, zipfile, io
ROOT=pathlib.Path(__file__).resolve().parents[1]
rows=json.loads((ROOT/'references/catalog.json').read_text())
selected={'Creature Portraits','Hero Portraits','Resources','Spellbook','Terrain','Landscape','Map Objects','Mines','Towns & Dwellings','Heroes (adventure)','Heroes (battle)','Primary Skills','Secondary Skills','Artifacts','Spells','Flags','Mouse Cursors','Battle Backgrounds','Siege Backgrounds','Castle Town','Rampart Town','Tower Town','Inferno Town','Necropolis Town','Dungeon Town','Stronghold Town','Fortress Town','Conflux Town','Pikeman','Archer','Griffin','Swordsman','Monk','Cavalier','Angel','Skeleton','Wolf Rider','Orc'}
def fetch(row):
 name=row['name'];slug=re.sub(r'[^a-z0-9]+','-',name.lower()).strip('-');folder=ROOT/'assets/original'/slug;folder.mkdir(parents=True,exist_ok=True)
 try:
  html=subprocess.check_output(['curl','-sSL','--fail','--max-time','40',row['source']]).decode()
  paths=re.findall(r"(?:href|src)=['\"](files/[^'\"]+\.(?:png|zip))['\"]",html,re.I)
  if not paths:raise ValueError('No indexed asset download')
  url=urllib.parse.urljoin('https://spritedatabase.net/',paths[0]);file=folder/pathlib.Path(paths[0]).name
  data=file.read_bytes() if file.exists() else subprocess.check_output(['curl','-sSL','--fail','--retry','2','--max-time','240',url])
  if len(data)>40*1024*1024:raise ValueError('Asset exceeds size limit')
  file.write_bytes(data);files=[]
  if file.suffix.lower()=='.zip':
   with zipfile.ZipFile(io.BytesIO(data)) as z:
    if sum(i.file_size for i in z.infolist())>150*1024*1024:raise ValueError('Expanded archive too large')
    for item in z.infolist():
     p=pathlib.Path(item.filename)
     if p.is_absolute() or '..' in p.parts or item.is_dir():continue
     if p.suffix.lower() not in ['.png','.jpg','.jpeg','.gif','.txt']:continue
     out=folder/p;out.parent.mkdir(parents=True,exist_ok=True);out.write_bytes(z.read(item));files.append(str(out.relative_to(ROOT)))
  else:files=[str(file.relative_to(ROOT))]
  return {**row,'download':url,'archive':str(file.relative_to(ROOT)),'sha256':hashlib.sha256(data).hexdigest(),'files':files,'status':'downloaded'}
 except Exception as e:return {**row,'status':'failed','error':str(e)}
with concurrent.futures.ThreadPoolExecutor(max_workers=6) as pool:results=list(pool.map(fetch,[r for r in rows if '--all' in sys.argv or r['name'] in selected]))
(ROOT/'references/downloads.json').write_text(json.dumps(results,ensure_ascii=False,indent=2))
for r in results:print(r['name'],r['status'],len(r.get('files',[])),r.get('error',''))
