#include "h3/game.hpp"
#include <algorithm>
#include <limits>

namespace h3 {
namespace {
CombatStack* byId(Battle& b,int id){auto it=std::find_if(b.units.begin(),b.units.end(),[=](const auto&u){return u.id==id;});return it==b.units.end()?nullptr:&*it;}
int elemental(int spell){switch(spell){case SummonFireElemental:return 118;case SummonEarthElemental:return 120;case SummonWaterElemental:return 116;case SummonAirElemental:return 114;default:return -1;}}
}

const CombatStack* Game::spellTarget(int spell,Point point)const {
 if(auto*live=combatAt(point))return live;
 if(!battle||(spell!=Resurrection&&spell!=AnimateDead&&spell!=Sacrifice))return nullptr;
 const CombatStack*top=nullptr;
 for(const auto&u:battle->units)if(u.corpse){auto cells=footprint(u,{u.x,u.y});
  if(std::find(cells.begin(),cells.end(),point)!=cells.end()&&(!top||u.deathOrder>top->deathOrder))top=&u;
 }
 return top;
}

bool Game::revivalEligible(const CombatStack&u,int spell)const {
 if(u.origin!=StackOrigin::Army||u.count>=u.initialCount||u.team!=spellTeam_)return false;
 if(spell==AnimateDead){if(!Units[u.unit].undead)return false;}
 else if(!Units[u.unit].living||spellImmune(u,Resurrection))return false;
 if(spellImmune(u,spell))return false;
 if(u.hp==0){
  if(!u.corpse)return false;
  for(auto cell:footprint(u,{u.x,u.y})){
   if(combatAt(cell))return false;
   // Classic SoD cannot select a wide corpse when its other half is covered.
   for(const auto&other:battle->units)if(other.corpse&&other.deathOrder>u.deathOrder){auto cells=footprint(other,{other.x,other.y});if(std::find(cells.begin(),cells.end(),cell)!=cells.end())return false;}
  }
 }
 if(spell!=Sacrifice&&casterPower()*Spells[spell].power+Spells[spell].values[spellMastery(spell)]<Units[u.unit].hp)return false;
 return true;
}

std::vector<int> Game::sacrificeVictims(int targetId)const {
 std::vector<int>targets;if(!battle)return targets;
 auto at=std::find_if(battle->units.begin(),battle->units.end(),[=](const auto&u){return u.id==targetId;});
 if(at==battle->units.end()||!revivalEligible(*at,Sacrifice))return targets;
 for(const auto&u:battle->units)if(u.id!=targetId&&u.team==spellTeam_&&u.hp>0&&u.origin==StackOrigin::Army&&Units[u.unit].living&&!spellImmune(u,Sacrifice))targets.push_back(u.id);
 return targets;
}

std::optional<Point> Game::reinforcementPosition(const CombatStack&unit,const CombatStack*near)const {
 for(int col=0;col<BattleW;col++)for(int y=0;y<BattleH;y++){
  int x=unit.team==0?col:BattleW-1-col;CombatStack candidate=unit;candidate.x=x;candidate.y=y;
  if(near&&distance(candidate,*near)!=1)continue;
  bool free=true;for(auto p:footprint(candidate,{x,y}))if(p.x<0||p.x>=BattleW||!siegeCellAllowed(candidate,p)||combatAt(p)){free=false;break;}
  if(free)return Point{x,y};
 }
 return {};
}

void Game::removeStack(CombatStack&u,bool leaveCorpse){
 u.hp=u.count=u.permanentCount=0;u.corpse=leaveCorpse&&u.origin!=StackOrigin::Clone;
 u.deathOrder=u.corpse?++battle->deathSequence:0;u.cloneTurns=0;u.defending=false;u.defendingBonus=0;
 u.effects={};u.effectMastery={};
 for(auto&copy:battle->units)if(copy.hp>0&&copy.origin==StackOrigin::Clone&&copy.cloneOf==u.id){
  battle->log.push_back(Units[copy.unit].name+"的镜像随本体消失。");removeStack(copy,false);
 }
}

void Game::revive(CombatStack&target,int amount,bool permanent,bool cleanse){
 bool dead=target.hp==0;int before=target.count;
 target.hp=int(std::min<int64_t>(int64_t(target.initialCount)*Units[target.unit].hp,int64_t(target.hp)+amount));
 target.count=(target.hp+Units[target.unit].hp-1)/Units[target.unit].hp;
 if(permanent)target.permanentCount=std::min(target.initialCount,target.permanentCount+target.count-before);
 if(target.hp>0){target.corpse=false;target.deathOrder=0;}
 if(cleanse)for(int s=0;s<SpellCount;s++)if(Spells[s].enemy&&!Spells[s].persistent)target.effects[s]=target.effectMastery[s]=0;
 if(dead&&!target.acted){auto&queue=target.waited&&!battle->waitPhase?battle->waiting:battle->queue;queue.push_back(target.id);}
 battle->log.push_back(Units[target.unit].name+"复生 "+std::to_string(target.count-before)+" 个单位"+(permanent?"（永久）。":"（本场战斗）。"));
}

void Game::finishCast(int active,int spell){if(battle&&spell>=0&&spell<SpellCount)battle->spellHistory[spellTeam_][spell]=true;
 if(finishBattle())return;
 refreshQueue();auto at=std::find(battle->queue.begin(),battle->queue.end(),active);
 if(at!=battle->queue.end())std::rotate(battle->queue.begin(),at,at+1);
 if(battle->queue.empty())nextAction();else prepareCurrent();
}

std::string Game::castReinforcement(int spell,Point point,Point sacrifice){
 auto&b=*battle;const auto&def=Spells[spell];int mastery=spellMastery(spell),active=current()->id;
 auto targets=spellTargets(spell,point);int selected=targets.empty()?-1:targets.front(),victim=-1;
 std::optional<Point>position;CombatStack created;
 if(def.kind==SpellKind::Summon||def.kind==SpellKind::Clone){
  int allocated=int(std::count_if(b.units.begin(),b.units.end(),[&](const auto&u){return u.team==spellTeam_;}));
  if(allocated>=MaxBattleStacksPerSide)return "本方已达到本场战斗的 20 栏上限";
  int slot=ArmySlots;while(slot<26&&std::any_of(b.units.begin(),b.units.end(),[&](const auto&u){return u.team==spellTeam_&&u.slot==slot;}))++slot;
  if(slot==26)return "本方没有可用的战场栏位";
  created.team=spellTeam_;created.slot=slot;created.id=spellTeam_*100+slot;
  if(def.kind==SpellKind::Summon){
   int unit=elemental(spell);if(b.summonedType[spellTeam_]>=0&&b.summonedType[spellTeam_]!=unit)return "经典版一场战斗中只能召唤一种元素";
   created.origin=StackOrigin::Summoned;created.unit=unit;created.count=created.initialCount=casterPower()*def.values[mastery];
   if(created.count<=0)return "法术力量不足以召唤元素";
   position=reinforcementPosition(created);
  }else{
   if(selected<0)return "请选择可以复制的友军";
   const auto&source=*byId(b,selected);created.origin=StackOrigin::Clone;created.unit=source.unit;
   created.count=created.initialCount=source.count;created.cloneOf=source.id;created.cloneTurns=std::max(1,casterPower());
   position=reinforcementPosition(created,&source);
  }
  if(!position)return "没有可容纳新部队的空位";
  created.x=position->x;created.y=position->y;created.hp=created.count*Units[created.unit].hp;created.shots=Units[created.unit].shots;
 }else{
  if(selected<0)return "请选择有阵亡单位且能复生的友军";
  if(spell==Sacrifice){
   const auto*source=combatAt(sacrifice);if(!source)return "请选择要牺牲的友军";
   auto candidates=sacrificeVictims(selected);if(std::find(candidates.begin(),candidates.end(),source->id)==candidates.end())return "这支部队不能用于牺牲";
   victim=source->id;
  }
 }
 casterMana()-=spellCost(spell);casterRound()=b.round;emit({GameEventType::SpellCast,-1,spell,point});b.log.push_back((spellTeam_?std::string("对方英雄"):hero.name)+"施放"+def.name+"。");
 if(def.kind==SpellKind::Summon||def.kind==SpellKind::Clone){
  if(def.kind==SpellKind::Summon)b.summonedType[spellTeam_]=created.unit;
  int id=created.id;b.units.push_back(std::move(created));b.queue.push_back(id);
 }else if(spell==Sacrifice){
  auto&source=*byId(b,victim);int amount=int(std::min<int64_t>(std::numeric_limits<int>::max(),int64_t(casterPower()+Units[source.unit].hp+def.values[mastery])*source.count));
  b.log.push_back(Units[source.unit].name+"被牺牲，不留下尸体。");removeStack(source,false);
  revive(*byId(b,selected),amount,true,true);
 }else revive(*byId(b,selected),casterPower()*def.power+def.values[mastery],spell==AnimateDead||mastery>=2,spell==Resurrection);
 finishCast(active,spell);return {};
}
}
