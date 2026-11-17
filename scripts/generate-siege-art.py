"""Index verified original siege PCX at independently checked classic coordinates."""
import json,re,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
manifest=json.loads((ROOT/'assets/demo/manifest.json').read_text())
published=json.loads((ROOT/'assets/manifest.json').read_text())
out={'schema':2,'factions':{},'publishedArchive':{'source':'https://spritedatabase.net/files/pc/2195/Background/siegebg.zip','sha256':hashlib.sha256((ROOT/'assets/original/siege-backgrounds/siegebg.zip').read_bytes()).hexdigest()}}
def asset(name):
    if name in manifest['pcx']:
        return {**manifest['pcx'][name],'name':name}
    entry=published.get('siege-backgrounds/'+name[:-4]+'.png')
    if entry:return {'name':name,'file':entry['url'].lstrip('/'),'width':entry['w'],'height':entry['h'],'source':'publishedArchive'}
for faction,name in enumerate(['castle','rampart','tower','inferno','necropolis','dungeon','stronghold','fortress','conflux']):
    source=ROOT/'references/siege-rules-0.46'/f'{name}.json'
    text=re.sub(r'//[^\n]*|/\*[\s\S]*?\*/','',source.read_text())
    cfg=json.loads(re.sub(r',\s*([}\]])',r'\1',text))
    siege=cfg[name]['town']['siege'];prefix=siege['imagePrefix'].lower();layers=[]
    def add(suffix,point,part=-1):
        candidates=[prefix+suffix+'.pcx'] if part<0 else [prefix+suffix+str(n)+'.pcx' for n in [1,2,3]]
        assets=[asset(n) for n in candidates]
        if any(a is None for a in assets):return
        layers.append({'names':candidates,'assets':assets,'x':point['x'],'y':point['y'],'part':part})
    add('back',{'x':0,'y':0})
    if siege['moat']:
        add('moat',siege['moat']['moat'],-3);add('mlip',siege['moat'].get('bank',siege['moat']['moat']),-3)
    add('tpwl' if asset(prefix+'tpwl.pcx') else 'tpw1',siege['static']['background']);add('wa5',siege['static']['top']);add('wa2',siege['static']['bottom'])
    for key,suffix,part in [('upper','wa6',4),('upperMid','wa4',5),('bottomMid','wa3',6),('bottom','wa1',7)]:add(suffix,siege['walls'][key],part)
    add('arch',siege['gate']['arch']);add('drw',siege['gate']['gate'],3)
    for key,suffix,part in [('keep','man',0),('top','tw2',1),('bottom','tw1',2)]:
        point=siege['towers'][key]['tower'];names=[prefix+suffix+str(n)+'.pcx' for n in [1,2]]
        if all(asset(n) for n in names):layers.append({'names':names,'assets':[asset(n) for n in names],'x':point['x'],'y':point['y'],'part':part})
    assert len(layers)>=13,(name,len(layers))
    out['factions'][str(faction)]={'layers':layers,'coordinateSource':'https://github.com/vcmi/vcmi/blob/develop/config/factions/'+name+'.json','coordinateSourceSHA256':hashlib.sha256(source.read_bytes()).hexdigest()}
(ROOT/'assets/demo/siege.json').write_text(json.dumps(out,ensure_ascii=False,indent=2)+'\n')
print('Indexed original siege artwork for',len(out['factions']),'towns')
