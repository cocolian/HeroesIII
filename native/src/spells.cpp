#include "h3/game.hpp"
#include <algorithm>

namespace h3 {
// Classic Complete/SoD values. The four entries are none/basic/advanced/expert.
// Provenance and manual errata are recorded in data/native/spells.json.
const std::array<SpellDef,SpellCount> Spells={{
#include "../generated/spells.inc"
}};

int Game::casterPower()const{return siegeBonus(spellTeam_,2)+(spellTeam_==1&&battle?battle->defenderPrimary[2]:primarySkill(2));}
int& Game::casterMana(){return spellTeam_==1?battle->defenderMana:hero.mana;}
int& Game::casterRound(){return spellTeam_==1?battle->defenderSpellRound:battle->spellRound;}
int Game::spellMastery(int spell)const {
 if(spell<0||spell>=SpellCount)return 0;
 int mastery=0;
 for(int school=0;school<4;school++)
  if(Spells[spell].school&(1<<school))mastery=std::max(mastery,(spellTeam_==1&&battle?battle->defenderSkills:hero.skills)[AirMagic+school]);
 if((spell==SummonBoat||spell==ScuttleBoat)&&combatHero(spellTeam_)&&wearsArtifact(*combatHero(spellTeam_),123))mastery=3;return mastery;
}

bool Game::spellMass(int spell)const {
 if(spell<0||spell>=SpellCount)return false;
 // Classic Complete has the advanced Forgetfulness mass-targeting bug.
 return (Spells[spell].massAtExpert&&spellMastery(spell)==3)||(spell==Forgetfulness&&spellMastery(spell)>=2);
}
bool Game::spellAutomatic(int spell)const {
 return spell>=0&&spell<SpellCount&&(spellMass(spell)||Spells[spell].target==SpellTarget::Global);
}
int Game::spellCost(int spell,int team)const {
 if(team<0)team=spellTeam_;
 if(spell<0||spell>=SpellCount)return 0;
 if(Spells[spell].cost==0)return 0;
 int mastery=0;for(int school=0;school<4;++school)if(Spells[spell].school&(1<<school))mastery=std::max(mastery,(team==1&&battle?battle->defenderSkills:hero.skills)[AirMagic+school]);
 int cost=mastery>0?Spells[spell].skilledCost:Spells[spell].cost;
 int discount=0,increase=0;
 if(battle){for(const auto&u:battle->units)if(u.hp>0||u.origin==StackOrigin::Army){if(u.team==team)discount=std::max(discount,Units[u.unit].spellCostDiscount);else increase=std::max(increase,Units[u.unit].enemySpellCostIncrease);}}
 else if(team==0)for(const auto&stack:hero.army)if(!stack.empty())discount=std::max(discount,Units[stack.unit].spellCostDiscount);
 return std::max(1,cost-discount+increase);
}
bool Game::spellImmune(const CombatStack&target,int spell)const {
 if(spell<0||spell>=SpellCount)return true;
 if(spell==Dispel)return false;
 if(artifactSpellImmune(spell,target.team))return true;if(combatHero(target.team)&&wearsArtifact(*combatHero(target.team),134)&&Spells[spell].level<=4)return true;
 const auto&u=Units[target.unit];
 if(target.origin==StackOrigin::WarMachine&&Spells[spell].kind!=SpellKind::Damage)return true;
 if((spell==Mirth||spell==Sorrow)&&u.nonLiving)return true;
 if((spell==Forgetfulness||spell==Blind||spell==Frenzy||spell==Berserk||spell==Hypnotize)&&(u.mindImmune||u.nonLiving))return true;
 return std::max(u.spellImmunity,spellEffect(target,AntiMagic))>=Spells[spell].level
  ||(u.schoolImmunity&Spells[spell].school)!=0
  ||std::find(u.spellImmunities.begin(),u.spellImmunities.end(),spell)!=u.spellImmunities.end();
}
int Game::spellEffect(const CombatStack&unit,int spell)const {
 if(spell<0||spell>=SpellCount||unit.effects[spell]<=0)return 0;
 return Spells[spell].values[unit.effectMastery[spell]]+(spell==Bless?0:unit.effectBonuses[spell]);
}
int Game::spellDamage(const CombatStack&target,int spell,int raw)const {
 const auto&unit=Units[target.unit];int vulnerability=0,protection=0;
 for(auto[s,bonus]:unit.spellVulnerabilities)if(s==spell)vulnerability=bonus;
 for(int school=0;school<4;school++)if(Spells[spell].school&(1<<school))protection=std::max(protection,spellEffect(target,ProtectAir+school));
 int sorcery=spellTeam_==0?hero.skills[Sorcery]:battle?battle->defenderSkills[Sorcery]:0;if(!innateSpell_){double bonus=combatHero(spellTeam_)?secondaryEffect(*combatHero(spellTeam_),Sorcery,5*sorcery):0;raw=int(raw*(100+bonus)/100.);if(auto*h=combatHero(spellTeam_))raw=heroSpellEffect(*h,spell,target.unit,raw);}
 int64_t damage=int64_t(raw)*(100+vulnerability)*(100-unit.spellDamageReduction)*(100-protection)/1000000;
 return std::max(0,int(damage));
}
namespace {
 bool hasEffects(const CombatStack&unit,bool negativeOnly){for(int s=0;s<SpellCount;s++)if(unit.effects[s]>0&&(!negativeOnly||Spells[s].enemy))return true;return false;}
 void clearEffects(CombatStack&unit,bool negativeOnly){for(int s=0;s<SpellCount;s++)if(!Spells[s].persistent&&(!negativeOnly||Spells[s].enemy))unit.effects[s]=unit.effectMastery[s]=unit.effectBonuses[s]=0;if(!unit.effects[Hypnotize])unit.team=unit.id/100;}
 const CombatStack* stackById(const Battle&battle,int id){auto at=std::find_if(battle.units.begin(),battle.units.end(),[id](const auto&unit){return unit.id==id;});return at==battle.units.end()?nullptr:&*at;}
}
std::vector<Point> Game::spellArea(int spell,Point center)const {
 std::vector<Point>area;if(spell<0||spell>=SpellCount||center.x<0||center.x>=BattleW||center.y<0||center.y>=BattleH)return area;
 const auto&def=Spells[spell];if(def.target!=SpellTarget::Radius&&def.target!=SpellTarget::Ring)return {center};
 for(int y=0;y<BattleH;y++)for(int x=0;x<BattleW;x++){int distance=hexDistance(center,{x,y});if(distance<=(spell==Berserk?def.values[spellMastery(spell)]:def.radius)&&(def.target!=SpellTarget::Ring||distance>0))area.push_back({x,y});}
 return area;
}
std::vector<int> Game::spellTargets(int spell,Point point)const {
 std::vector<int>targets;if(!battle||spell<0||spell>=SpellCount)return targets;
 const auto&def=Spells[spell];const auto*selected=spellTarget(spell,point);
 auto filter=def.filter;if(spell==Dispel&&spellMastery(spell)>=2)filter=SpellFilter::Any;
 bool area=def.target==SpellTarget::Radius||def.target==SpellTarget::Ring;
 auto cells=area?spellArea(spell,point):std::vector<Point>{};
 for(const auto&unit:battle->units){
  if((unit.hp<=0&&spell!=Dispel&&def.kind!=SpellKind::Resurrection&&def.kind!=SpellKind::Sacrifice)||spellImmune(unit,spell))continue;
  if((def.kind==SpellKind::Resurrection||def.kind==SpellKind::Sacrifice)&&!revivalEligible(unit,spell))continue;
  if(def.kind==SpellKind::Sacrifice&&sacrificeVictims(unit.id).empty())continue;
  if(def.kind==SpellKind::Clone){if(unit.origin==StackOrigin::Clone||Units[unit.unit].level>def.values[spellMastery(spell)])continue;if(std::any_of(battle->units.begin(),battle->units.end(),[&](const auto&copy){return copy.hp>0&&copy.origin==StackOrigin::Clone&&copy.cloneOf==unit.id;}))continue;}
  if(filter==SpellFilter::Enemy&&unit.team==spellTeam_)continue;
  if(filter==SpellFilter::Ally&&unit.team!=spellTeam_)continue;
  if(filter==SpellFilter::Undead&&!Units[unit.unit].undead)continue;
  if(filter==SpellFilter::NonUndead&&Units[unit.unit].undead)continue;
  if(spell==Hypnotize&&(unit.hp>casterPower()*25+def.values[spellMastery(spell)]||std::max(unit.effects[MagicMirror],creatureAbility(unit.unit).mirror)>0))continue;
  if((spell==Precision||spell==Forgetfulness)&&Units[unit.unit].shots==0)continue;
  if(area){auto footprint_=footprint(unit,{unit.x,unit.y});if(std::none_of(footprint_.begin(),footprint_.end(),[&](Point p){return std::find(cells.begin(),cells.end(),p)!=cells.end();}))continue;}
  else if(!spellAutomatic(spell)&&(!selected||selected->id!=unit.id))continue;
  if(spell==Cure&&unit.hp==unit.count*Units[unit.unit].hp&&!hasEffects(unit,true))continue;
  if(spell==Dispel&&!hasEffects(unit,false))continue;
  targets.push_back(unit.id);
 }
 return targets;
}
std::string Game::spellDescription(int spell)const {
 if(spell<0||spell>=SpellCount)return {};
 if(!Spells[spell].implemented)return "此法术效果尚未开放。";
 const auto&def=Spells[spell];int mastery=spellMastery(spell),amount=def.values[mastery];
 if(def.adventure){switch(spell){case SummonBoat:return "在岸边召来最近的空船，成功率 50% / 50% / 75% / 100%；高级以上可创造船只。";case ScuttleBoat:return "摧毁视野内选中的空船，成功率 50% / 50% / 75% / 100%。";case TownPortal:return "传送到己方城镇；初级以下仅最近城镇，高级以上可选择；消耗 300 行动力，专家消耗 200。";case DimensionDoor:return "在横向 9 格、纵向 8 格内传送；每日可施放 1 / 2 / 3 / 4 次；消耗 300 行动力，专家 200。";case Fly:return "本日可飞越地面障碍和水面，须在空闲陆地落脚；行动力额外消耗 40% / 40% / 20% / 0%。";case WaterWalk:return "本日可步行跨越水面，须回到陆地结束；行动力额外消耗 40% / 40% / 20% / 0%。";case Visions:return "本日查看范围内野兵数量和加入意愿；高级可查看英雄，专家可查看城镇。";case Disguise:return "本日向敌方隐藏真实部队：以最强部队伪装；专家还隐藏部队数量。";case ViewEarth:return "显示资源位置；高级增加矿山，专家显示完整地形，不改变探索迷雾。";case ViewAir:return "显示宝物位置；高级增加英雄，专家增加城镇，不改变探索迷雾。";default:return {};}}
 std::string target=spellAutomatic(spell)?"全部有效目标":def.target==SpellTarget::Chain?"敌军及其附近部队":def.target==SpellTarget::Radius||def.target==SpellTarget::Ring?"范围内所有部队":def.enemy?"一支敌军":"一支友军";
 std::string description=target+"：";
 if(spell==Blind)description="无法移动和行动；受到伤害后清醒。被近战唤醒时以 "+std::to_string(amount)+"% 伤害反击。";
 else if(spell==Slayer)description="攻击巨兽、九头蛇和龙类时攻击 +8；高级包含天使、恶魔，专家包含巨人、泰坦。";
 else if(spell==Frenzy)description="防御归零，并将原防御的 "+std::to_string(amount)+"% 转为攻击；本队完成一次攻击后解除。";
 else if(spell==MagicMirror)description="有 "+std::to_string(amount)+"% 几率把敌方单体法术反射到随机敌军，不连续反射。";
 else if(spell==Berserk)description="使半径 "+std::to_string(amount)+" 格内的非免疫部队失控，攻击最近的任何部队，完成自己的攻击后解除。";
 else if(spell==Hypnotize)description="控制总生命不超过 "+std::to_string(casterPower()*25+amount)+" 的非免疫敌军，持续英雄力量轮。";
 else if(spell==Quicksand||spell==LandMine)description="随机布置 "+std::to_string(std::array<int,4>{4,4,6,8}[mastery])+" 处隐藏陷阱；流沙停止地面移动，地雷对敌军触发一次伤害。";
 else if(spell==ForceField||spell==FireWall)description="放置 "+std::to_string(mastery>=2?3:2)+" 格魔法墙，持续 2 轮；大力神盾阻挡，烈火魔墙对进入及停留的部队造成伤害。";
 else if(spell==Teleport)description="先选择己方部队，再选择落脚格；高级可越过护城河，专家可越过城墙。";
 else if(spell==RemoveObstacle)description="驱除障碍；高级可移除烈火魔墙，专家可移除其他魔法障碍。";
 else if(spell==Earthquake)description="随机攻击 "+std::to_string(amount)+" 段城防，每次造成 1 点城防伤害。";
 else if(def.kind==SpellKind::Summon)description="召唤 "+std::to_string(casterPower()*amount)+" 个元素，本场战斗中只能召唤同一种元素。";
 else if(def.kind==SpellKind::Clone)description="复制一支 "+std::to_string(amount)+" 级及以下友军，镜像受伤或本体死亡即消失，持续 "+std::to_string(std::max(1,casterPower()))+" 轮。";
 else if(def.kind==SpellKind::Resurrection)description="恢复最多 "+std::to_string(casterPower()*def.power+amount)+" 点生命对应的阵亡"+std::string(spell==AnimateDead?"亡灵":"活物")+"，"+(spell==AnimateDead||mastery>=2?"永久保留。":"仅本场战斗有效。 ");
 else if(def.kind==SpellKind::Sacrifice)description="先选择复生目标，再牺牲另一支活物友军；复生生命 =（力量 + 牺牲单位生命 + "+std::to_string(amount)+"）× 牺牲数量，永久保留。";
 else if(def.kind==SpellKind::Damage)description+="造成 "+std::to_string(casterPower()*def.power+amount)+" 点基础魔法伤害。";
 else if(spell==Cure)description+="解除负面法术并恢复最多 "+std::to_string(casterPower()*def.power+amount)+" 点生命，不复活。";
 else if(spell==Dispel)description+="清除可驱散的正面和负面法术。";
 else if(spell==Haste)description+="速度 +"+std::to_string(amount)+"。";
 else if(spell==Slow)description+="速度降低 "+std::to_string(amount)+"%。";
 else if(spell==Shield||spell==AirShield)description+=(spell==Shield?"近战":"远程")+std::string("伤害降低 ")+std::to_string(amount)+"%。";
 else if(spell==FireShield)description+="反射 "+std::to_string(amount)+"% 近战基础伤害。";
 else if(spell>=ProtectAir&&spell<=ProtectEarth)description+="相应学派魔法伤害降低 "+std::to_string(amount)+"%。";
 else if(spell==AntiMagic)description+="免疫 "+std::to_string(amount)+" 级及以下法术，清除负面法术。";
 else if(spell==StoneSkin)description+="防御 +"+std::to_string(amount)+"。";
 else if(spell==Bless)description+=amount?"基础伤害取最大值 +1。":"基础伤害取最大值。";
 else if(spell==Curse)description+=amount?"基础伤害取最小值 −1，最低为 1。":"基础伤害取最小值。";
 else if(spell==Bloodlust||spell==Precision)description+=(spell==Bloodlust?"近战":"远程")+std::string("攻击 +")+std::to_string(amount)+"。";
 else if(spell==Weakness)description+="攻击 −"+std::to_string(amount)+"。";
 else if(spell==DisruptingRay)description+="防御 −"+std::to_string(amount)+"，可重复施放，持续到战斗结束。";
 else if(spell==Prayer)description+="攻击、防御、速度 +"+std::to_string(amount)+"。";
 else if(spell==Counterstrike)description+="每轮额外反击 "+std::to_string(amount)+" 次。";
 else if(spell==Forgetfulness)description+=amount>=100?"无法射击；近战伤害减半。":"射击和近战伤害减半。";
 else if(spell==Mirth||spell==Sorrow)description+="士气 "+std::string(spell==Mirth?"+":"−")+std::to_string(amount)+"。";
 else if(spell==Fortune||spell==Misfortune)description+="幸运 "+std::string(spell==Fortune?"+":"−")+std::to_string(amount)+"。";
 if(def.kind==SpellKind::Damage&&(def.target==SpellTarget::Radius||def.target==SpellTarget::Ring||def.target==SpellTarget::Global||def.target==SpellTarget::Chain))description+="可能伤及友军。";
 if(def.kind==SpellKind::Timed&&!def.persistent&&spell!=Frenzy&&spell!=Berserk)description+="持续 "+std::to_string(std::max(1,casterPower()))+" 轮。";
 return description;
}
std::string Game::cast(int spell,Point point,Point sacrifice) {
 if(!battle||!current()||current()->team!=spellTeam_)return "等待我方行动";if(battle->tacticsTeam>=0)return "请先完成战术布阵。";if(battle->noMagic)return "禁魔要塞内不能施放法术。";
 if(!(spellTeam_==1?battle->defenderSpellbook:hero.spellbook))return "英雄尚未拥有魔法书";
 if(spell<0||spell>=SpellCount||!(spellTeam_==1?battle->defenderSpells:heroKnownSpells(hero))[spell])return "尚未学会该法术";
 if(!Spells[spell].implemented)return "此法术效果尚未开放";
 if(Spells[spell].adventure)return "战斗中不能使用冒险法术";
 auto&combat=*battle;const auto&def=Spells[spell];
 if(casterRound()==combat.round)return "英雄每轮只能施法一次";
 int cost=spellCost(spell);if(casterMana()<cost)return "魔法值不足";
 if(def.kind==SpellKind::Resurrection||def.kind==SpellKind::Sacrifice||def.kind==SpellKind::Clone||def.kind==SpellKind::Summon)return castReinforcement(spell,point,sacrifice);
 if(def.kind==SpellKind::Special&&spell!=Earthquake)return castBattleSpecial(spell,point,sacrifice);
 if(spell==Earthquake){if(!combat.siege.fort)return "只能在攻城战中施放。";bool intact=std::any_of(combat.siege.hp.begin(),combat.siege.hp.end(),[](int hp){return hp>0;});if(!intact)return "城防已经全部摧毁。";int active=current()->id;casterMana()-=cost;int channel=0;for(const auto&u:combat.units)if(u.hp>0&&u.team!=spellTeam_)channel=std::max(channel,creatureAbility(u.unit).channel);if(channel&&combatHero(1-spellTeam_)){int&receiving=spellTeam_?hero.mana:combat.defenderMana;receiving=std::min(1000000,receiving+cost*channel/100);}casterRound()=combat.round;for(int i=0;i<def.values[spellMastery(spell)];++i){std::vector<int>parts;for(int j=0;j<8;++j)if(combat.siege.hp[j]>0)parts.push_back(j);if(parts.empty())break;--combat.siege.hp[parts[size_t(random()*parts.size())]];}combat.log.push_back("地动山摇破坏了城防。");emit({GameEventType::SpellCast,-1,spell,point});finishCast(active,spell);return {};}

 if(!spellAutomatic(spell)&&(def.target==SpellTarget::Single||def.target==SpellTarget::Chain)){
  auto*target=combatAt(point);if(!target)return "请选择一支部队";
  if(spellImmune(*target,spell))return "该单位免疫这一等级的魔法";
 }
 auto targets=spellTargets(spell,point);if(targets.empty())return "没有可受此法术影响的部队";
 int active=current()->id,mastery=spellMastery(spell),raw=casterPower()*def.power+def.values[mastery];
 casterMana()-=cost;int channel=0;for(const auto&u:combat.units)if(u.hp>0&&u.team!=spellTeam_)channel=std::max(channel,creatureAbility(u.unit).channel);if(channel&&combatHero(1-spellTeam_)){int&receiving=spellTeam_?hero.mana:combat.defenderMana;receiving=std::min(1000000,receiving+cost*channel/100);}casterRound()=combat.round;emit({GameEventType::SpellCast,-1,spell,point});combat.log.push_back((spellTeam_==1?std::string("对方英雄"):hero.name)+"施放"+def.name+(spellMass(spell)?"（群体）。":"。"));
 std::vector<int>reflected;
 auto apply=[&](int id,int baseDamage){
  auto&target=*std::find_if(combat.units.begin(),combat.units.end(),[id](const auto&unit){return unit.id==id;});const auto&unit=Units[target.unit];
  if(target.hp<=0&&spell!=Dispel)return true;
  if(def.enemy&&def.target==SpellTarget::Single&&!spellMass(spell)&&std::max(creatureAbility(target.unit).mirror,spellEffect(target,MagicMirror))>0&&random()*100<std::max(creatureAbility(target.unit).mirror,spellEffect(target,MagicMirror))){std::vector<int>candidates;for(const auto&u:combat.units)if(u.hp>0&&u.team==spellTeam_&&!spellImmune(u,spell))candidates.push_back(u.id);if(!candidates.empty()){reflected.push_back(candidates[size_t(random()*candidates.size())]);combat.log.push_back("魔法神镜反射了"+def.name+"。");return false;}}

  int resistance=magicResistance(target);
  if(def.enemy&&resistance>0&&random()*100<resistance){combat.log.push_back(unit.name+"抵抗了法术。");return false;}
  if(def.kind==SpellKind::Damage){hurt(target,spellDamage(target,spell,baseDamage));emit({GameEventType::CombatSpellEffect,target.unit,spell,{target.x,target.y},target.id,-1,{},target});}
  else if(spell==Cure){clearEffects(target,true);clearCreatureEffects(target);int healing=std::min(target.count*stackHealth(target)-target.hp,raw);target.hp+=healing;combat.log.push_back(unit.name+"恢复 "+std::to_string(healing)+" 点生命并解除负面效果。");}
  else if(spell==Dispel){clearEffects(target,false);clearCreatureEffects(target);}
  else if(spell==DisruptingRay)target.defenseReduction+=std::min(def.values[mastery],std::max(0,defense(target)));
  else {if(spell==AntiMagic)clearEffects(target,true);target.effects[spell]=std::max(1,casterPower()+(combatHero(spellTeam_)?artifactDuration(*combatHero(spellTeam_)):0));target.effectMastery[spell]=mastery;target.effectBonuses[spell]=combatHero(spellTeam_)?heroSpellEffect(*combatHero(spellTeam_),spell,target.unit,def.values[mastery])-def.values[mastery]:0;if(spell==Bless&&combatHero(spellTeam_)&&combatHero(spellTeam_)->originalId==9)target.effectBonuses[Bless]=300*combatHero(spellTeam_)->level/std::max(1,Units[target.unit].level);if(spell==Hypnotize){target.team=spellTeam_;target.effects[Berserk]=target.effectMastery[Berserk]=0;}if(spell==Berserk)clearHypnosis(target);int counter=spell==Haste?Slow:spell==Slow?Haste:spell==Bless?Curse:spell==Curse?Bless:spell==Bloodlust?Weakness:spell==Weakness?Bloodlust:spell==Mirth?Sorrow:spell==Sorrow?Mirth:spell==Fortune?Misfortune:spell==Misfortune?Fortune:-1;if(counter>=0)target.effects[counter]=target.effectMastery[counter]=target.effectBonuses[counter]=0;}
  if(def.kind==SpellKind::Timed)emit({GameEventType::CombatSpellEffect,target.unit,spell,{target.x,target.y},target.id,-1,{},target});
  return true;
 };
 if(def.target==SpellTarget::Chain){
  std::vector<int>visited;int id=targets.front(),jumps=mastery>=2?5:4;
  for(int hop=0;hop<jumps;hop++){
   visited.push_back(id);bool hit=apply(id,raw);if(hop==0&&!hit)break;raw/=2;
   const auto*previous=stackById(combat,id);std::vector<int>nearest;int best=1000;
   for(const auto&unit:combat.units)if(unit.hp>0&&!spellImmune(unit,spell)&&std::find(visited.begin(),visited.end(),unit.id)==visited.end()){
    int distance_=distance(*previous,unit);if(distance_<best){nearest.clear();best=distance_;}if(distance_==best)nearest.push_back(unit.id);
   }
   if(nearest.empty())break;id=nearest[nearest.size()==1?0:size_t(random()*nearest.size())];
  }
 }else for(int id:targets)apply(id,raw);
 for(int id:reflected){auto&target=*std::find_if(combat.units.begin(),combat.units.end(),[id](auto&u){return u.id==id;});if(def.kind==SpellKind::Damage)hurt(target,spellDamage(target,spell,raw));else if(spell==DisruptingRay)target.defenseReduction+=def.values[mastery];else{target.effects[spell]=std::max(1,casterPower()+(combatHero(spellTeam_)?artifactDuration(*combatHero(spellTeam_)):0));target.effectMastery[spell]=mastery;target.effectBonuses[spell]=combatHero(spellTeam_)?heroSpellEffect(*combatHero(spellTeam_),spell,target.unit,def.values[mastery])-def.values[mastery]:0;if(spell==Bless&&combatHero(spellTeam_)&&combatHero(spellTeam_)->originalId==9)target.effectBonuses[Bless]=300*combatHero(spellTeam_)->level/std::max(1,Units[target.unit].level);}}
 refreshQueue();finishCast(active,spell);
 return {};
}

std::string Game::learnMagic(int skill) {
 if(battle||result||!hero.levelChoices.empty())return "请先完成当前战斗或升级";
 if(!visiting()||town.faction!=8||!town.built[Special2])return "英雄必须进入建有魔法大学的元素城";
 if(skill<AirMagic||skill>EarthMagic)return "魔法大学只教授四系魔法";
 if(hero.skills[skill]>0)return "已经掌握这一魔法学派";
 if(std::count_if(hero.skills.begin(),hero.skills.end(),[](int level){return level>0;})>=8)return "英雄最多掌握八项第二技能";
 if(resources[Gold]<2000)return "学习需要 2000 金币";
 resources[Gold]-=2000;
 hero.skills[skill]=1;
 message("在魔法大学学习了初级"+SkillNames[skill]+"。");
 return log.back();
}
}
