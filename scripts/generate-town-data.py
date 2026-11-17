"""Extract documented town costs, dependency expressions and drawing layers offline.
Only factual metadata is emitted; third-party rule implementations are not copied.
"""
import json,re,pathlib
from rule_html import Document
root=pathlib.Path(__file__).resolve().parents[1]
def js(path):return json.loads(re.sub(r',\s*([}\]])',r'\1',re.sub(r'//[^\n]*','',path.read_text())))
def px(style,key):
 m=re.search(r'(?:^|;)\s*'+key+r':\s*(-?\d+)px',style);return int(m.group(1))if m else None
resources=['Gold','Wood','Ore','Mercury','Sulfur','Crystal','Gem']
roster=json.load(open(root/'data/native/creatures.json'));results=[]
for faction in roster['factions']:
 key=faction['key'];tree=Document((root/'references/rules/wiki'/f'{key.title()}.html').read_text()).root;costRows=[]
 for group in tree.walk('div'):
  if 'position:relative;z-index:5' not in group.attrs.get('style',''):continue
  labels={};costs={}
  for child in group.children:
   style=child.attrs.get('style','');top=px(style,'top')
   if child.tag=='span' and px(style,'left')==156 and top is not None and child.text():labels[top]=child.text()
   if child.tag=='div'and top is not None and'position:absolute' in style:
    images=[i.attrs.get('src','')for i in child.walk('img')]
    for ri,resource in enumerate(resources):
     if any('/'+resource+'_symbol_for_text.png'in src for src in images):
      nums=re.findall(r'\d+',child.text());value=int(nums[-1])if nums else 0;costs.setdefault(top,[0]*7)[ri]=value
  for y,name in labels.items():
   candidate=costs.get(y-18)
   if candidate is not None:costRows.append({'name':name,'cost':candidate})
 town=js(root/'references/rules/vcmi/config/factions'/f'{key}.json')[key]['town'];rows=[];dwellingRows=[c for c in costRows if c['name'].startswith('Upg. ')]
 # Exactly fourteen interleaved base/upgrade rows are present in each creature dwelling table.
 if len(dwellingRows)<7:raise ValueError((key,'missing dwelling cost rows',costRows))
 for tier,pair in enumerate(town['creatures']):
  i=tier*2
  for upgraded in [False,True]:
   item=costRows[i+int(upgraded)].copy();item['key']=('dwellingUpLvl' if upgraded else'dwellingLvl')+str(tier+1);item['unitKey']=pair[int(upgraded)];rows.append(item)
 result={'key':key,'name':faction['name'],'dwellingCosts':rows,'allCostRows':costRows,'buildings':town['buildings'],'structures':town['structures'],'background':town['townBackground'],'mapTemplates':town['mapObject']['templates'],'maxMageGuild':town['mageGuild']}
 results.append(result)
(root/'data/native/towns.json').write_text(json.dumps({'edition':'Classic Complete / Shadow of Death','sources':['references/rules/sources.json','references/rules/wiki-sources.json'],'towns':results},ensure_ascii=False,indent=2)+'\n')
for f in results:print(f['key'],len(f['dwellingCosts']),'dwelling costs;',len(f['structures']),'drawing layers;',len(f['buildings']),'building entries')
