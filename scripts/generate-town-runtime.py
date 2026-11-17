"""Compile collected classic town facts into the native, offline rule table.

Building identifiers 0..26 stay compatible with the first native save format.
No VCMI engine implementation is used. Dependency expressions and coordinates
are factual metadata; unavailable art remains explicitly listed in the report.
"""
import json,pathlib,re
root=pathlib.Path(__file__).resolve().parents[1]
def js(path):return json.loads(re.sub(r',\s*([}\]])',r'\1',re.sub(r'//[^\n]*','',path.read_text())))
quote=lambda s:json.dumps(s,ensure_ascii=False)
keys=['townHall','cityHall','capitol','fort','citadel','castle','tavern','marketplace','blacksmith','mageGuild1','mageGuild2','mageGuild3']+['dwellingLvl'+str(i)for i in range(1,8)]+['dwellingUpLvl'+str(i)for i in range(1,8)]+['horde1','resourceSilo','shipyard','special1','special2','special3','special4','mageGuild4','mageGuild5','horde2','villageHall']
ids={k:i for i,k in enumerate(keys)}
common=js(root/'references/rules/vcmi/config/buildingsLibrary.json')
towns=json.load(open(root/'data/native/towns.json'))['towns']
units=json.load(open(root/'data/native/creatures.json'))['units']
manifest=json.load(open(root/'assets/manifest.json'))
demoPath=root/'assets/demo/manifest.json'
demo=json.load(open(demoPath))if demoPath.exists()else{'def':{}}
interfacePath=root/'assets/demo/town-ui.json'
interface=json.load(open(interfacePath))if interfacePath.exists()else{'towns':{}}
names=['内政厅','议会','国会','堡垒','大本营','城堡','酒馆','市场','铁匠铺','一级魔法行会','二级魔法行会','三级魔法行会']
costs=[[2500],[5000],[10000],[5000,20,20],[2500,0,5],[5000,0,10],[500,5],[500,5],[1000,5],[2000,5,5],[1000,5,5,4,4,4,4],[1000,5,5,6,6,6,6]]
dwellingNames=[
 ['岗楼','射手塔','狮鹫塔','兵营','修道院','骑兵营','天使之门'],
 ['半人马厩','矮人村舍','木屋','魔泉','枯木拱门','独角兽林地','龙崖'],
 ['精怪作坊','怪物塔楼','魔像工厂','法师塔','神怪殿','金色大殿','云中城'],
 ['怪物之巢','恶鬼殿','犬舍','恶魔之门','深渊之门','火精灵神殿','遗弃殿堂'],
 ['邪庙','坟地','灵墓','墓园','陵墓','暗黑殿','龙之顶'],
 ['洞穴','鹰身女妖之楼','毒眼之柱','美杜莎神殿','迷宫','蝎狮巢穴','龙穴'],
 ['大耳怪营地','恶狼骑士营','半兽人塔','食人魔堡','雷鸟崖','独眼巨人洞','比蒙巨兽崖'],
 ['狼人小屋','蜥蜴巢穴','毒蝇巢穴','蜥蜴地穴','蛮牛巢穴','飞龙巢穴','九头蛇潭'],
 ['魔法灯','气元素祭坛','水元素祭坛','火元素祭坛','土元素祭坛','精神元素祭坛','烈火神坛']]
# Costs not present in the dwelling image tables: common buildings, horde
# buildings and Stables. Values are checked against the stored wiki snapshots.
specials=[
 [('Lighthouse','灯塔',[2000,0,10]),('Stables','马厩',[2000,10]),('Brotherhood of the Sword','兄弟会',[500,5]),None],
 [('Mystic Pond','神秘池塘',[2000,2,2,2,2,2,2]),('Fountain of Fortune','幸运泉',[1500,0,0,0,0,10]),('Treasury','金库',[5000,5,10]),None],
 [('Artifact Merchants','宝物商店',[10000]),('Lookout Tower','瞭望塔',[1000,5]),('Library','图书馆',[1500,5,5,5,5,5,5]),('Wall of Knowledge','知识之墙',[1000,0,5])],
 [None,('Brimstone Stormclouds','硫磺烟云',[1000,0,0,0,5]),('Castle Gate','城堡之门',[10000,5,5]),('Order of Fire','烈火神殿',[1000,5])],
 [('Cover of Darkness','黑暗天幕',[1000]),('Necromancy Amplifier','招魂塔',[1000]),('Skeleton Transformer','骷髅转换场',[1000]),None],
 [('Artifact Merchants','宝物商店',[10000]),('Mana Vortex','魔力漩涡',[1000]),('Portal of Summoning','召唤之门',[2500,0,5]),('Battle Scholar Academy','战斗学院',[1000,5,5])],
 [('Escape Tunnel','逃跑地道',[2000,5,5]),("Freelancer's Guild",'自由职业者公会',[1000]),('Ballista Yard','弩车作坊',[1000,5]),('Hall of Valhalla','英烈祠',[1000])],
 [('Cage of Warlords','战神之笼',[1000]),('Blood Obelisk','鲜血方尖塔',[1000]),('Glyphs of Fear','恐怖浮雕',[1000]),None],
 [('Artifact Merchants','宝物商店',[10000]),('Magic University','魔法大学',[5000,10,10]),None,None]]
hordes=[[(2,3,'狮鹫堡垒'),None],[(1,4,'矿工工会'),(4,2,'枯木幼苗')],[(1,4,'雕塑家之翼'),None],[(0,8,'繁殖池'),(2,3,'犬笼')],[(0,6,'骷髅墓穴'),None],[(0,7,'蘑菇环'),None],[(0,8,'食堂'),None],[(0,6,'军官宿舍'),None],[(0,10,'生命之园'),None]]
supportedSpecials={(0,1),(2,1),(2,2),(2,3),(3,3),(5,3),(6,3),(7,0),(8,1)}
def dnf(expr):
 if not expr:return [[]]
 if isinstance(expr,str):
  if expr not in ids:raise ValueError('Unmapped prerequisite '+expr)
  return [[ids[expr]]]
 if expr[0]=='allOf':
  result=[[]]
  for child in expr[1:]:result=[a+b for a in result for b in dnf(child)]
  return result
 if expr[0]=='anyOf':return [item for child in expr[1:]for item in dnf(child)]
 return dnf(['allOf']+expr)
def ints(values):return '{'+','.join(map(str,values))+'}'
def options(rows):return '{'+','.join(ints(r)for r in rows)+'}'
output=[];compiled=[];missing=[]
for fi,town in enumerate(towns):
 rules=town['buildings'];rows=[]
 for bi,key in enumerate(keys):
  override=rules.get(key,{})
  available=override is not None
  rule={**common.get(key,{}),**(override or{})}
  req=rule.get('requires',[])
  if rule.get('upgrades'):req=['allOf',req,[rule['upgrades']]]
  requirement=dnf(req);cost=[0]*7;unit=-1;implemented=True
  name=key;desc='';english=key
  if bi<12:
   name=names[bi];cost[:len(costs[bi])]=costs[bi]
   desc=['每日收入 1000 金币','每日收入 2000 金币','每日收入 4000 金币','允许建设生物巢穴','基础生物周增长 +50%','基础生物周增长 +100%','城镇酒馆','资源交易市场','战争机器工坊','教授一级法术；过夜恢复魔法','二级魔法行会','三级魔法行会'][bi]
  elif key.startswith('dwelling'):
   tier=int(key[-1])-1;up=key.startswith('dwellingUp');unit=fi*14+tier*2+int(up) if not up else -1
   ref=town['dwellingCosts'][tier*2+int(up)];cost=ref['cost'];english=ref['name'];name=('高级'if up else'')+dwellingNames[fi][tier]
   desc='可招募'+units[fi*14+tier*2+int(up)]['name']
  elif key.startswith('mageGuild'):
   level=int(key[-1]);name=['','','','','四级魔法行会','五级魔法行会'][level];cost=[1000,5,5]+[2*level]*4;desc='教授'+str(6-level)+'种'+str(level)+'级法术'
  elif key in ('horde1','horde2'):
   h=hordes[fi][key=='horde2'];available=available and h is not None
   if h:name=h[2];desc=units[fi*14+h[0]*2]['name']+'每周增长 +'+str(h[1]);cost[0]=2000 if fi==1 and key=='horde2'else 1000
  elif key=='resourceSilo':name='资源仓库';cost=[5000,0,5,0,0,0,0];desc='每天生产本城所需资源'
  elif key=='shipyard':name='船坞';cost=[2000,20,0,0,0,0,0];desc='船只系统待接入';implemented=False
  elif key=='villageHall':name='村议事堂';desc='每日收入 500 金币';implemented=False
  elif key.startswith('special'):
   si=int(key[-1])-1;special=specials[fi][si];available=available and special is not None
   if special:
    english,name,c=special;cost[:len(c)]=c
    actual=next((r['cost']for r in town['allCostRows'][14:]if r['name']==english),None)
    if actual is not None:assert actual==cost,(town['key'],key,cost,actual)
   implemented=(fi,si)in supportedSpecials;desc='每层魔法行会增加一种法术'if(fi,si)==(2,2)else'城镇特殊建筑'if implemented else'特殊建筑效果待接入'
  requirement=[sorted(set(r))for r in requirement]
  row={'key':key,'name':name,'sourceName':english,'description':desc,'cost':cost,'requirements':requirement,'unit':unit,'available':available,'implemented':implemented,'icon':rule.get('id',-1),'upIcon':common.get(key+'Upgr',{}).get('id',-1)}
  rows.append(row)
 # Merge shared and faction-specific upgrade facts. Shared fortification,
 # civic and guild chains are absent from many faction override objects.
 upgradeParent={}
 for building in keys:
  override=rules.get(building,{})
  if override is not None:
   parent={**common.get(building,{}),**override}.get('upgrades')
   if parent:upgradeParent[building]=parent
 # A structure replaces every ancestor in its own upgrade chain.
 structures=town['structures'];layers=[]
 for key,v in structures.items():
  canonical=key.replace('Upgr','')
  if canonical not in ids:continue
  need=[ids[canonical]]
  if key.endswith('Upgr'):
   h=int(key[5])-1;tier=hordes[fi][h][0];need.append(ids['dwellingUpLvl'+str(tier+1)])
  excluded=[]
  for other in keys:
   parent=upgradeParent.get(other);seen=set()
   while parent and parent not in seen:
    if parent==key:
     excluded.append(ids[other]);break
    seen.add(parent);parent=upgradeParent.get(parent)
  if key in ('horde1','horde2'):
   tier=hordes[fi][int(key[-1])-1][0];excluded.append(ids['dwellingUpLvl'+str(tier+1)])
  stem=v.get('border',v.get('area',''))[2:].split('.')[0].lower()
  candidates=['tb'+stem,v.get('animation','').split('.')[0].lower()]
  if fi==0 and key=='dwellingUpLvl6':candidates.insert(0,'tbcsc201')
  if fi==0 and key=='special1':candidates.insert(0,'tacslt01')
  if fi==8:
   for prefix in ('dwellingLvl','dwellingUpLvl'):
    if key.startswith(prefix):candidates.insert(0,('tbeldw'if prefix=='dwellingLvl'else'tbelup')+str(int(key[-1])-1))
   if key=='grail':candidates.insert(0,'tbelhly')
  folder=town['key']+'-town/'
  asset=None
  for candidate in candidates:
   options_=sorted(k for k in manifest if k.startswith(folder)and(k==folder+candidate+'.png'or k==folder+candidate+'a.png'))
   if options_:asset=options_[0];break
  animation=v.get('animation','').lower()
  if asset is None and animation not in demo['def']:missing.append({'faction':town['key'],'structure':key,'candidates':candidates});continue
  ui=interface['towns'].get(town['key'],{}).get('layers',{}).get(key,{})
  layers.append({'structure':key,'art':asset or '', 'animation':animation,'pos':[v['x'],v['y']],'z':v.get('z',0),'needs':need,'excludes':excluded,'mask':ui.get('mask',''),'outline':ui.get('outline',''),'originalMask':ui.get('original',False)})
 layers.sort(key=lambda l:(l['z'],l['pos'][1]))
 source=js(root/'references/rules/vcmi/config/factions'/f"{town['key']}.json")[town['key']]['town']
 silo=[0]*7;resource=source.get('primaryResource')
 if resource:silo[['gold','wood','ore','mercury','sulfur','crystal','gems'].index(resource)]=1
 else:silo[1]=silo[2]=1
 ht=[h[0]if h else-1 for h in hordes[fi]];hg=[h[1]if h else 0 for h in hordes[fi]]
 hallSlots=[[[ids[key]for key in dict.fromkeys(name.replace('Upgr','')for name in group)if key in ids]for group in row]for row in source['hallSlots']]
 hallIcons=source['buildingsIcons'].lower();hallBackground=source['hallBackground'].rsplit('.',1)[0].lower()+'.pcx'
 compiled.append({'key':town['key'],'buildings':rows,'layers':layers,'silo':silo,'hordeTier':ht,'hordeGrowth':hg,'maxMageGuild':town['maxMageGuild'],'hallIcons':hallIcons,'hallBackground':hallBackground,'hallSlots':hallSlots})
 cppRows=['{'+','.join([quote(b['name']),quote(b['description']),ints(b['cost']),options(b['requirements']),str(b['unit']),str(b['available']).lower(),str(b['implemented']).lower(),str(b['icon']),str(b['upIcon'])])+'}'for b in rows]
 cppLayers=['{'+','.join([quote(l['art']),ints(l['pos']),str(l['z']),ints(l['needs']),ints(l['excludes']),quote(l['animation']),quote(l['mask']),quote(l['outline']),str(l['originalMask']).lower()])+'}'for l in layers]
 output.append('TownDef{.buildings={{'+','.join(cppRows)+'}},.layers={'+','.join(cppLayers)+'},.silo='+ints(silo)+',.hordeTier='+ints(ht)+',.hordeGrowth='+ints(hg)+',.maxMageGuild='+str(town['maxMageGuild'])+',.hallIcons='+quote(hallIcons)+',.hallBackground='+quote(hallBackground)+',.hallSlots={'+','.join('{'+','.join(ints(group)for group in row)+'}'for row in hallSlots)+'}},')
(root/'native/generated/towns.inc').write_text('// Generated by scripts/generate-town-runtime.py; do not edit.\n'+'\n'.join(output)+'\n')
(root/'data/native/town-runtime.json').write_text(json.dumps({'towns':compiled,'missingArtwork':missing},ensure_ascii=False,indent=2)+'\n')
print('Compiled',len(compiled),'towns;',len(keys),'stable building IDs;',len(missing),'unresolved town artwork layers')
