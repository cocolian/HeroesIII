"""Compile implemented classic spell facts; no third-party engine code is imported."""
import copy,json,pathlib,re
from rule_html import Document
from spell_catalog import catalog
root=pathlib.Path(__file__).resolve().parents[1]
def read(path):
 s=path.read_text();return json.loads(re.sub(r',\s*([}\]])',r'\1',re.sub(r'//[^\n]*','',s)))
def merge(a,b):
 out=copy.deepcopy(a)
 for k,v in b.items():out[k]=merge(out.get(k,{}),v)if isinstance(v,dict)else copy.deepcopy(v)
 return out
def first_value(node):
 if isinstance(node,dict):
  if 'val'in node:return abs(node['val'])
  for child in node.values():
   value=first_value(child)
   if value is not None:return value
 return None
defs={}
for name in ['offensive','timed','other']:defs.update(read(root/'references/rules/vcmi/config/spells'/f'{name}.json'))
# Preserve original native IDs 0..8. Coefficients and flat damage/healing values
# are legacy facts absent from VCMI JSON and are checked against recorded pages.
inputs=[
 ('magicArrow','Magic_Arrow','魔法神箭',1,True,15,10,[10,10,20,30]),
 ('cure','Cure','疗伤',1,False,4,5,[10,10,20,30]),
 ('haste','Haste','加速',1,False,1,0,None),('slow','Slow','迟缓',1,True,8,0,None),
 ('shield','Shield','护体神盾',1,False,8,0,None),('stoneSkin','Stone_Skin','护体石肤',1,False,8,0,None),
 ('bless','Bless','祝福',1,False,4,0,None),('curse','Curse','恶咒附身',1,True,2,0,None),
 ('lightningBolt','Lightning_Bolt','闪电霹雳',2,True,1,25,[10,10,20,50]),
 ('iceBolt','Ice_Bolt','霹雳寒冰',2,True,4,20,[10,10,20,50]),
 ('implosion','Implosion','雷鸣爆弹',5,True,8,75,[100,100,200,300]),
 ('chainLightning','Chain_Lightning','连锁闪电',4,True,1,40,[25,25,50,100]),
 ('frostRing','Frost_Ring','寒冰魔环',3,True,4,10,[15,15,30,60]),
 ('fireball','Fireball','连珠火球',3,True,2,10,[15,15,30,60]),
 ('inferno','Inferno_(spell)','地狱烈焰',4,True,2,10,[20,20,40,80]),
 ('meteorShower','Meteor_Shower','流星火雨',4,True,8,25,[25,25,50,100]),
 ('deathRipple','Death_Ripple','死亡波纹',2,True,8,5,[10,10,20,30]),
 ('destroyUndead','Destroy_Undead','亡灵杀手',3,True,1,10,[10,10,20,50]),
 ('armageddon','Armageddon','末日审判',4,True,2,50,[30,30,60,120]),
 ('titanBolt','Titan%27s_Lightning_Bolt','泰坦之箭',5,True,1,0,[600,600,600,600]),
 ('airShield','Air_Shield','护体气盾',3,False,1,0,None),
 ('fireShield','Fire_Shield','烈火神盾',4,False,2,0,None),
 ('protectAir','Protection_from_Air','御气奇术',2,False,1,0,None),
 ('protectFire','Protection_from_Fire','御火奇术',1,False,2,0,None),
 ('protectWater','Protection_from_Water','御水奇术',1,False,4,0,None),
 ('protectEarth','Protection_from_Earth','御土奇术',3,False,8,0,None),
 ('antiMagic','Anti-Magic','抗魔大法',3,False,8,0,None),
 ('bloodlust','Bloodlust','嗜血奇术',1,False,2,0,None),
 ('precision','Precision','百发百中',2,False,1,0,None),
 ('weakness','Weakness','虚弱无力',2,True,4,0,None),
 ('disruptingRay','Disrupting_Ray','毁灭之光',2,True,1,0,None),
 ('prayer','Prayer','祈祷',4,False,4,0,None),
 ('counterstrike','Counterstrike','反戈一击',4,False,1,0,None),
 ('forgetfulness','Forgetfulness','失忆大法',3,True,4,0,None),
 ('dispel','Dispel','驱魔大法',1,False,4,0,[0,0,0,0])]
facts=catalog()
implementedKeys={row[0]for row in inputs}
additionalNames={
 'summonBoat':'召船术','scuttleBoat':'毁船术','visions':'透视之眼','viewEarth':'透视大地',
 'disguise':'伪装大法','viewAir':'透视大气','fly':'飞行奇术','waterWalk':'凌波微步',
 'dimensionDoor':'异次元之门','townPortal':'时空之门','quicksand':'流沙陷阱',
 'landMine':'埋设地雷','forceField':'大力神盾','fireWall':'烈火魔墙','earthquake':'地动山摇',
 'magicMirror':'魔法神镜','resurrection':'转世重生','animateDead':'聚灵奇术','sacrifice':'牺牲',
 'mirth':'欢欣鼓舞','sorrow':'悲痛欲绝','fortune':'幸运之神','misfortune':'大难临头',
 'slayer':'屠戮成性','frenzy':'孤注一掷','berserk':'丧心病狂','hypnotize':'蛊惑人心',
 'blind':'双目失明','teleport':'瞬间移动','removeObstacle':'驱除障碍','clone':'镜像大法',
 'fireElemental':'召唤火元素','earthElemental':'召唤土元素','waterElemental':'召唤水元素','airElemental':'召唤气元素'}
for fact in sorted(facts.values(),key=lambda row:row['originalId']):
 key=fact['key']
 if key not in implementedKeys:inputs.append((key,fact['page'],additionalNames[key],fact['level'],defs.get(key,{}).get('flags',{}).get('negative',False),fact['school'],0,[0,0,0,0]))
# Adventure entries provide identity/learning metadata; their behavior is separate.
for name in ['adventure']:defs.update(read(root/'references/rules/vcmi/config/spells'/f'{name}.json'))
newBehavior={
 **{key:('Special',0,[0,0,0,0])for key in ['quicksand','landMine','forceField','fireWall','teleport','removeObstacle']},
 'berserk':('Timed',0,[0,0,1,2]),'hypnotize':('Timed',25,[10,10,20,50]),
 **{key:('Adventure',0,[0,0,0,0])for key in ['summonBoat','scuttleBoat','visions','viewEarth','viewAir','disguise','fly','waterWalk','dimensionDoor','townPortal']},
 'blind':('Timed',0,[50,50,25,0]),'slayer':('Timed',0,[8,8,8,8]),
 'frenzy':('Timed',0,[100,100,150,200]),'magicMirror':('Timed',0,[20,20,30,40]),
 'earthquake':('Special',0,[2,2,3,4]),
 'mirth':('Timed',0,[1,1,2,2]),'sorrow':('Timed',0,[1,1,2,2]),
 'fortune':('Timed',0,[1,1,2,2]),'misfortune':('Timed',0,[1,1,2,2]),
 'resurrection':('Resurrection',50,[40,40,80,160]),
 'animateDead':('Resurrection',50,[30,30,60,160]),
 'sacrifice':('Sacrifice',0,[3,3,6,10]),
 'clone':('Clone',0,[5,5,6,10]),
 'fireElemental':('Summon',0,[2,2,3,4]),
 'earthElemental':('Summon',0,[2,2,3,4]),
 'waterElemental':('Summon',0,[2,2,3,4]),
 'airElemental':('Summon',0,[2,2,3,4])}
connectedKeys=implementedKeys|newBehavior.keys()
records=[];cpp=[]
for index,(key,page,name,level,enemy,school,power,values)in enumerate(inputs):
 d=defs[key];levels=d['levels'];implemented=key in connectedKeys;fact=facts[key]
 if key in newBehavior:power,values=newBehavior[key][1:]
 assert (level,school)==(fact['level'],fact['school']),key
 text=Document((root/'references/rules/wiki'/f'{page}.html').read_text()).root.text()if implemented and (root/'references/rules/wiki'/f'{page}.html').exists() else''
 match=re.search(r'Cost:\s*(\d+)\s*/\s*(\d+)',text)
 if not implemented or key in newBehavior or fact['adventure'] or key in {'blind','slayer','frenzy','magicMirror','earthquake'}:cost,skilled=fact['cost'],fact['skilledCost']
 elif key=='titanBolt':cost=skilled=0
 elif key in {'fortune','misfortune'}:cost,skilled=fact['cost'],fact['skilledCost']
 else:
  assert match,(key,'missing recorded cost');cost,skilled=map(int,match.groups())
 if values is None:values=[first_value(merge(levels['base'],levels.get(grade,{}))['battleEffects'])for grade in ['base','basic','advanced','expert']]
 assert all(v is not None for v in values),key
 kind='Unsupported'if not implemented else'Damage'if d.get('flags',{}).get('damage')else'Healing'if key=='cure'else'Dispel'if key=='dispel'else'Timed'
 if key in newBehavior:kind=newBehavior[key][0]
 target={'chainLightning':'Chain','frostRing':'Ring','fireball':'Radius','inferno':'Radius','meteorShower':'Radius','deathRipple':'Global','destroyUndead':'Global','armageddon':'Global'}.get(key,'Single')
 if kind=='Summon' or key in {'earthquake','quicksand','landMine'}:target='Global'
 if key=='berserk':target='Radius'
 filter_='Any'if target in ['Radius','Ring','Global']else'Enemy'if enemy else'Ally'
 if key=='deathRipple':filter_='NonUndead'
 if key=='destroyUndead':filter_='Undead'
 radius=2 if key=='inferno'else 1 if target in ['Radius','Ring']else 0
 mass=levels.get('expert',{}).get('range')=='X';persistent=d.get('flags',{}).get('persistent',False);special=d.get('flags',{}).get('special',False)
 record={'id':index,'key':key,'name':name,'originalId':d['index'],'level':level,'cost':cost,'skilledCost':skilled,'enemy':enemy,'schoolMask':school,'powerMultiplier':power,'values':values,'massAtExpert':mass,'kind':kind,'target':target,'filter':filter_,'radius':radius,'persistent':persistent,'special':special,'animation':d.get('animation',{}),'sounds':d.get('sounds',{}),'implemented':implemented,'adventure':fact['adventure'],'guildWeights':fact['guildWeights'],'reference':'references/rules/wiki/'+(page+'.html'if implemented and (root/'references/rules/wiki'/f'{page}.html').exists() else'List_of_spells.html')}
 records.append(record)
 fields=[json.dumps(name,ensure_ascii=False),str(level),str(cost),str(skilled),str(enemy).lower(),str(school),str(power),'{'+','.join(map(str,values))+'}',str(mass).lower(),str(d['index']),'SpellKind::'+kind,'SpellTarget::'+target,'SpellFilter::'+filter_,str(radius),str(persistent).lower(),str(special).lower(),str(implemented).lower(),str(fact['adventure']).lower(),'{'+','.join(map(str,fact['guildWeights']))+'}']
 cpp.append('{'+','.join(fields)+'},')
(root/'native/generated/spells.inc').write_text('// Generated by scripts/generate-spell-data.py\n'+'\n'.join(cpp)+'\n')
record={'edition':'Classic Complete / Shadow of Death','masteryOrder':['none','basic','advanced','expert'],'sources':['references/rules/sources.json','references/rules/wiki-sources.json'],'errata':['The original manual incorrectly describes basic Slow as reducing speed to 25%; the actual reduction is 25%.','The original manual describes an extra 20% reduction for advanced Curse; actual base damage is minimum minus one, bounded at one.','Fire-school immunity includes Magic Arrow even if the hero uses mastery in a different school.','Death Ripple affects non-living creatures too; only undead and independently immune targets are excluded.','Advanced Forgetfulness is already a mass spell in classic Complete, unlike the HotA fix.'],'spells':records}
(root/'data/native/spells.json').write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n')
print('Generated',len(records),'classic spell entries;',len(connectedKeys),'have connected combat behavior')
