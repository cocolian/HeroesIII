"""Generate factual classic creature data from a recorded table + faction metadata.
Does not import a third-party engine implementation. Regeneration is offline.
"""
import json,re,pathlib,hashlib
from rule_html import creature_rows
root=pathlib.Path(__file__).resolve().parents[1]
def readjson(path):
 s=path.read_text();s=re.sub(r'//[^\n]*','',s);return json.loads(re.sub(r',\s*([}\]])',r'\1',s))
def norm(s):return re.sub('[^a-z0-9]','',s.lower())
def quote(s):return json.dumps(s,ensure_ascii=False)
factions=['castle','rampart','tower','inferno','necropolis','dungeon','stronghold','fortress','conflux']
zh=[
'枪兵 戟兵 弓箭手 神射手 狮鹫 皇家狮鹫 剑士 十字军 僧侣 祭司 骑士 冠军骑士 天使 大天使',
'半人马 半人马首领 矮人 战斗矮人 木精灵 大精灵 飞马 银飞马 枯木卫士 枯木战士 独角兽 独角神兽 绿龙 金龙',
'小妖精 大妖精 石像鬼 石像怪 石人 铁人 法师 大法师 神怪 神怪主 娜迦 娜迦女王 巨人 泰坦巨人',
'小怪物 怪物 歌革 玛各 地狱猎犬 地狱三首犬 恶鬼 长角恶鬼 邪神 邪神王 火精灵 烈火精灵 恶魔 大恶魔',
'骷髅兵 骷髅勇士 行尸 僵尸 幽灵 阴魂 吸血鬼 吸血鬼王 尸巫 尸巫王 暗黑骑士 恐怖骑士 骨龙 鬼龙',
'洞穴人 地狱洞穴人 鹰身女妖 鹰身女巫 邪眼 毒眼 美杜莎 美杜莎女王 牛头怪 牛头王 狮蝎 毒狮蝎 赤龙 黑龙',
'大耳怪 大耳怪王 恶狼骑士 恶狼斗士 半兽人 半兽人首领 食人魔 食人魔王 大雕 雷鸟 独眼巨人 独眼王 比蒙巨兽 远古比蒙',
'狼人 大狼人 蜥蜴人 蜥蜴勇士 毒蝇 龙蝇 蛇蜥 巨蜥 蛇皮兽 蛮牛 飞龙 飞龙王 九头怪 终极九头怪',
'小精灵 精灵 气元素 雷元素 水元素 冰元素 火元素 烈火元素 土元素 石元素 精神元素 魔法元素 火鸟 凤凰']
neutral=['goldGolem','diamondGolem','halfling','peasant','boar','mummy','nomad','rogue','troll','enchanter','sharpshooter','faerieDragon','rustDragon','crystalDragon','azureDragon']
neutralZh=['金人','钻石人','半身人','农民','野猪','木乃伊','游牧民','盗贼','穴居巨魔','魔幻法师','幻影射手','仙女龙','锈龙','水晶龙','圣龙']
rows=creature_rows((root/'references/rules/wiki/List_of_creatures.html').read_text())
byname={norm(r[0]['images'][0]):r for r in rows if r[0]['images'] and len(r)>=14}
supplemental=[{'name':'Beholder','files':['assets/original/beholder/Beholder.png']},{'name':'Evil Eye','files':['assets/original/evil-eye/EvilEye.png']}]
assets=json.load(open(root/'references/downloads.json'));byasset={norm(r['name']):r for r in assets+supplemental}
allDefs={};factionData=[]
for f in factions+['neutral']:
 allDefs.update(readjson(root/'references/rules/vcmi/config/creatures'/f'{f}.json'))
units=[]
for fi,f in enumerate(factions):
 town=readjson(root/'references/rules/vcmi/config/factions'/f'{f}.json')[f]['town'];order=[k for tier in town['creatures'] for k in tier];names=zh[fi].split();assert len(order)==len(names)==14
 factionData.append({'key':f,'units':order,'name':['城堡','壁垒','塔楼','地狱','墓园','地下城','据点','要塞','元素城'][fi]})
 units.extend((k,names[i],fi,i//2,i%2==1) for i,k in enumerate(order))
units.extend((k,neutralZh[i],-1,-1,False) for i,k in enumerate(neutral))
unitIds={u[0]:i for i,u in enumerate(units)}
spellKeys=[s['key']for s in json.load(open(root/'data/native/spells.json'))['spells']]
demoPath=root/'assets/demo/manifest.json'
demo=json.load(open(demoPath))if demoPath.exists()else{'def':{}}
portraits=demo['def'].get('twcrport.def',{}).get('groups',{}).get('0',[])
result=[];inc=['// Generated factual data. Run scripts/generate-creature-data.py; see data/native/creatures.json for provenance.']
for internal,(key,name,faction,tier,upgraded) in enumerate(units):
 d=allDefs[{'faerieDragon':'fairieDragon'}.get(key,key)];nkey=norm(key);wikiKey={'efreet':'efreeti','ogremagi':'ogremage','zombie':'walkingdead','zombielord':'zombie','goblinwolfrider':'wolfrider','hobgoblinwolfrider':'wolfraider','ogremage':'ogremagi','cyclop':'cyclops','cyclopking':'cyclopsking','firedragonfly':'dragonfly','fairiedragon':'faeriedragon'}.get(nkey,nkey);wikiKey={'ogremagi':'ogremage'}.get(wikiKey,wikiKey);r=byname[wikiKey];abilities=d.get('abilities',{});types={v['type']:v for v in abilities.values() if isinstance(v,dict) and 'type'in v}
 def numeric(cell):
  values=re.findall(r'\d+',r[cell]['text']);assert values,(key,cell)
  # The classic column is explicitly identified in mixed HotA/SoD cells.
  return int(values[-1] if any('Shadow of Death' in a for a in r[cell]['images']) else values[0])
 asset=byasset[{'efreet':'efreeti','zombie':'walkingdead','zombielord':'zombie','goblinwolfrider':'wolfrider','hobgoblinwolfrider':'wolfraider','ogremage':'ogremagi','cyclop':'cyclops','cyclopking':'cyclopsking','firedragonfly':'dragonfly','fairiedragon':'faeriedragon'}.get(nkey,nkey)];files=[p for p in asset['files'] if p.lower().endswith('.png')];assert len(files)==1,(key,files);assetPath=str(pathlib.Path(files[0]).relative_to('assets/original'));portraitOnly=not (root/files[0]).exists();assetPath='creature-portraits/CreaturePortraits.png' if portraitOnly else assetPath
 cost=[numeric(11),0,0,0,0,0,0];extra=r[12];resource={'Wood':1,'Ore':2,'Mercury':3,'Sulfur':4,'Crystal':5,'Gem':6,'Gems':6};
 if extra['text']:
  res=[resource[a] for a in extra['images'] if a in resource];assert len(res)==1,(key,extra);cost[res[0]]=numeric(12)
 labels={'FLYING':'飞行','SHOOTER':'远程攻击','ADDITIONAL_ATTACK':'二次攻击','ADDITIONAL_RETALIATION':'额外反击','UNLIMITED_RETALIATIONS':'无限反击','BLOCKS_RETALIATION':'敌人无法反击','NO_MELEE_PENALTY':'无近战惩罚','UNDEAD':'亡灵','NON_LIVING':'非生命体','MAGIC_RESISTANCE':'魔法抵抗','SPELL_DAMAGE_REDUCTION':'减少法术伤害','LEVEL_SPELL_IMMUNITY':'法术等级免疫','HP_REGENERATION':'生命再生','ATTACKS_ALL_ADJACENT':'攻击周围敌军','TWO_HEX_ATTACK_BREATH':'龙息攻击','JOUSING':'骑兵冲锋','CHARGE_IMMUNITY':'免疫冲锋','SPELL_AFTER_ATTACK':'攻击附带法术','MANA_DRAIN':'吸取魔法值','MIND_IMMUNITY':'免疫心智法术','HATE':'仇恨特定敌军','FEARFUL':'恐惧','DOUBLE_DAMAGE_CHANCE':'致命一击','SPELLCASTER':'施法','SIEGE_WEAPON':'攻城能力','NO_DISTANCE_PENALTY':'无距离惩罚','NO_WALL_PENALTY':'无城墙惩罚'}
 summary=' · '.join(labels[t] for t in types if t in labels);noRetal='BLOCKS_RETALIATION' in types;flying='FLYING' in types;double='ADDITIONAL_ATTACK' in types
 immune=types.get('LEVEL_SPELL_IMMUNITY',{}).get('val',0);resist=types.get('MAGIC_RESISTANCE',{}).get('val',0)
 reduction=types.get('SPELL_DAMAGE_REDUCTION',{}).get('val',0);regen=types.get('HP_REGENERATION',{}).get('val',0)
 out={'id':internal,'key':key,'name':name,'originalId':d['index'],'faction':faction,'tier':tier,'upgraded':upgraded,'hp':numeric(7),'attack':numeric(3),'defense':numeric(4),'minDamage':numeric(5),'maxDamage':numeric(6),'speed':numeric(8),'growth':numeric(9),'cost':cost,'shots':d.get('shots',0),'size':2 if d.get('doubleWide',False) else 1,'retaliations':999 if 'UNLIMITED_RETALIATIONS' in types else (1+types.get('ADDITIONAL_RETALIATION',{}).get('val',0)),'flying':flying,'doubleAttack':double,'noMeleePenalty':'NO_MELEE_PENALTY' in types,'noRetaliation':noRetal,'undead':'UNDEAD' in types,'nonLiving':any(t in types for t in ['UNDEAD','NON_LIVING','SIEGE_WEAPON']),'joust':'JOUSTING' in types,'spellImmunity':immune,'magicResistance':resist,'spellDamageReduction':reduction,'regeneration':regen,'manaDrain':types.get('MANA_DRAIN',{}).get('val',0),'asset':assetPath,'portraitOnly':portraitOnly,'abilityTypes':list(types),'special':summary}
 # VCMI spells use bonus identifiers that are stable across versions; keep all remaining abilities as explicit metadata.
 if key in ['griffin','royalGriffin']:out['retaliations']=2 if key=='griffin' else 999
 if key in ['cavalier','champion']:out['joust']=True
 out.update(enemyDefenseReduction=types.get('ENEMY_DEFENCE_REDUCTION',{}).get('val',0),spellCostDiscount=types.get('CHANGES_SPELL_COST_FOR_ALLY',{}).get('val',0),enemySpellCostIncrease=types.get('CHANGES_SPELL_COST_FOR_ENEMY',{}).get('val',0),schoolImmunity=0,noDistancePenalty='NO_DISTANCE_PENALTY'in types,hates=[],spellVulnerabilities=[],spellImmunities=[])
 for ability in abilities.values():
  kind=ability.get('type');sub=ability.get('subtype');value=ability.get('val',0)
  if kind=='HATE':out['hates'].append([unitIds[sub],value])
  if kind=='SPELL_IMMUNITY'and sub in spellKeys:out['spellImmunities'].append(spellKeys.index(sub))
  if kind=='MORE_DAMAGE_FROM_SPELL'and sub in spellKeys:out['spellVulnerabilities'].append([spellKeys.index(sub),value])
  if kind=='SPELL_SCHOOL_IMMUNITY':out['schoolImmunity']|={'air':1,'fire':2,'water':4,'earth':8}[sub]
 if out['undead']:out['spellImmunities']=sorted(set(out['spellImmunities']+[spellKeys.index(k)for k in ['bless','curse','deathRipple']if k in spellKeys]))
 out['fireShield']=next((a.get('val',0)for a in abilities.values()if a.get('type')=='COMBAT_EVENT_TRIGGER'and a.get('subtype')=='fireShield'),0)
 out['mindImmune']=out['nonLiving']or'MIND_IMMUNITY'in types or'GARGOYLE'in types
 out['living']=not out['nonLiving']and'GARGOYLE'not in types
 out['level']=d.get('level',tier+1)
 out['animation']=d.get('graphics',{}).get('animation','').lower()
 # DEF starts with blank/unknown placeholders. Match the original embedded
 # filename instead of treating a creature ID as a sequence index.
 matches=[i for i,frame in enumerate(portraits)if re.match(r'^cprl%03d[a-z]\.pcx$'%out['originalId'],frame['name'],re.I)]
 if portraits:assert len(matches)==1,(key,out['originalId'],matches)
 out['portraitFrame']=matches[0]if matches else -1
 out.update(allyMorale=0,enemyMorale=0,enemyLuck=0,moraleFloor=-3,luckFloor=0)
 for ability in abilities.values():
  if not isinstance(ability,dict):continue
  kind=ability.get('type');value=ability.get('val',0)
  if kind=='MORALE':
   if ability.get('valueType')=='INDEPENDENT_MAX':out['moraleFloor']=max(out['moraleFloor'],value)
   elif value>0:out['allyMorale']=max(out['allyMorale'],value)
   else:out['enemyMorale']=min(out['enemyMorale'],value)
  if kind=='LUCK':
   if ability.get('valueType')=='INDEPENDENT_MAX':out['luckFloor']=max(out['luckFloor'],value)
   elif value<0:out['enemyLuck']=min(out['enemyLuck'],value)
 result.append(out)
 fields={k:out[k] for k in ['name','asset','hp','attack','defense','minDamage','maxDamage','speed','growth']}
 fields.update(cost=cost[0],shots=out['shots'],size=out['size'],retaliations=out['retaliations'],flying=flying,doubleAttack=double,noMeleePenalty=out['noMeleePenalty'],joust=out['joust'],gemCost=cost[6],faction=faction,tier=tier,originalId=out['originalId'],upgraded=upgraded,noRetaliation=noRetal,undead=out['undead'],nonLiving=out['nonLiving'],spellImmunity=immune,magicResistance=resist,spellDamageReduction=reduction,regeneration=regen,manaDrain=out['manaDrain'],extraCost=cost[1:],abilitySummary=summary,portraitOnly=portraitOnly)
 fields.update({k:out[k]for k in ['enemyDefenseReduction','spellCostDiscount','enemySpellCostIncrease','schoolImmunity','noDistancePenalty','hates','spellVulnerabilities','spellImmunities','fireShield','mindImmune','living','level','animation','portraitFrame','allyMorale','enemyMorale','enemyLuck','moraleFloor','luckFloor']})
 def cpp(k,v):
  if k=='extraCost':return '{0,'+','.join(map(str,v))+'}'
  if k in ['hates','spellVulnerabilities']:return '{'+','.join('{'+','.join(map(str,p))+'}'for p in v)+'}'
  if k=='spellImmunities':return '{'+','.join(map(str,v))+'}'
  if isinstance(v,bool):return 'true'if v else'false'
  return quote(v) if isinstance(v,str) else str(v)
 inc.append('UnitDef{'+', '.join('.'+k+'='+cpp(k,v) for k,v in fields.items())+'},')
(root/'native/generated/creatures.inc').write_text('\n'.join(inc)+'\n')
record={'edition':'Heroes III Complete / Shadow of Death (excluding HotA changes)','sources':['references/rules/sources.json','references/rules/wiki-sources.json'],'factions':factionData,'units':result}
(root/'data/native/creatures.json').write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n')
print('Generated',len(result),'original creature definitions in 9 factions + neutral roster')
