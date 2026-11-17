"""Retrieve revision-pinned public metadata references; no game executable/code is imported."""
import concurrent.futures,hashlib,json,pathlib,subprocess,sys
root=pathlib.Path(__file__).resolve().parents[1]
recorded=json.load(open(root/'references/rules/sources.json'));revision=next(r['revision']for r in recorded if 'revision'in r);out=root/'references/rules/vcmi';out.mkdir(parents=True,exist_ok=True)
paths=list(dict.fromkeys([r['path']for r in recorded]+['config/skills.json','config/spells/offensive.json','config/spells/timed.json','config/spells/other.json','config/spells/adventure.json','config/gameConfig.json']))
hashes={r['path']:r.get('sha256')for r in recorded}
def get(p):
 url='https://raw.githubusercontent.com/vcmi/vcmi/'+revision+'/'+p
 try:
  dest=out/p
  if dest.exists()and'--refresh'not in sys.argv and hashlib.sha256(dest.read_bytes()).hexdigest()==hashes.get(p):content=dest.read_bytes()
  else:content=subprocess.check_output(['curl','-sSL','--fail','--max-time','90',url]);dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(content)
  return {'path':p,'revision':revision,'url':url,'sha256':hashlib.sha256(content).hexdigest(),'status':'downloaded'}
 except Exception as e:return {'path':p,'status':'failed','error':str(e)}
with concurrent.futures.ThreadPoolExecutor(max_workers=6) as pool:rows=list(pool.map(get,paths))
(root/'references/rules/sources.json').write_text(json.dumps(rows,indent=2))
for r in rows:print(r['status'],r['path'])
