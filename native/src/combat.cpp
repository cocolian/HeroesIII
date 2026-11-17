#include "h3/game.hpp"
#include "h3/battle_layout.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <queue>
#include <set>
namespace h3 {
namespace {CombatStack* byId(Battle& b,int id){auto it=std::find_if(b.units.begin(),b.units.end(),[id](auto&u){return u.id==id;});return it==b.units.end()?nullptr:&*it;} }
CombatStack* Game::current(){if(!battle||battle->queue.empty())return nullptr;return byId(*battle,battle->queue.front());}
const CombatStack* Game::current()const{return const_cast<Game*>(this)->current();}
std::vector<Point> Game::footprint(const CombatStack&u,Point p)const{std::vector<Point>out{p};if(Units[u.unit].size==2)out.push_back({p.x+(u.team==0?-1:1),p.y});return out;}
CombatStack* Game::combatAt(Point p){if(!battle)return nullptr;for(auto&u:battle->units)if(u.hp>0){auto cells=footprint(u,{u.x,u.y});if(std::find(cells.begin(),cells.end(),p)!=cells.end())return &u;}return nullptr;}
const CombatStack* Game::combatAt(Point p)const{return const_cast<Game*>(this)->combatAt(p);}
int Game::hexDistance(Point a,Point b){auto cube=[](Point p){int q=p.x-(p.y+(p.y&1))/2;return std::array<int,3>{q,-q-p.y,p.y};};auto ac=cube(a),bc=cube(b);return std::max({std::abs(ac[0]-bc[0]),std::abs(ac[1]-bc[1]),std::abs(ac[2]-bc[2])});}
int Game::distance(const CombatStack&a,const CombatStack&b)const{int d=100;for(auto p:footprint(a,{a.x,a.y}))for(auto q:footprint(b,{b.x,b.y}))d=std::min(d,hexDistance(p,q));return d;}
int Game::speed(const CombatStack&u)const{int n=Units[u.unit].speed+spellEffect(u,Prayer);if(u.effects[Haste]>0)n+=spellEffect(u,Haste);if(u.effects[Slow]>0)n=n*(100-spellEffect(u,Slow))/100;return std::max(1,n);}
int Game::defense(const CombatStack&u)const{return Units[u.unit].defense+(u.team==0?primarySkill(1):battle?battle->defenderPrimary[1]:0)+spellEffect(u,StoneSkin)+spellEffect(u,Prayer)-u.defenseReduction+(u.defending?u.defendingBonus:0)-(inMoat(u,{u.x,u.y})&&battle->siege.faction!=2?3:0);}
std::vector<Point> Game::battleRoute(const CombatStack&u,Point dest)const{
 if(!battle||u.x<0||u.x>=BattleW||u.y<0||u.y>=BattleH)return {};
 auto available=[&](Point p){for(Point cell:footprint(u,p)){if(cell.x<0||cell.x>=BattleW||cell.y<0||cell.y>=BattleH)return false;if(!siegeCellAllowed(u,cell))return false;auto*other=combatAt(cell);if(other&&other->id!=u.id)return false;}return true;};
 if(!available(dest))return {};Point start{u.x,u.y};if(dest==start)return {start};if(Units[u.unit].flying)return {start,dest};
 std::array<int,BattleW*BattleH> parent;parent.fill(-2);auto index=[](Point p){return p.y*BattleW+p.x;};parent[index(start)]=-1;
 std::queue<Point> queue;queue.push(start);
 while(!queue.empty()){
  auto p=queue.front();queue.pop();
  if(p==dest){std::vector<Point> route;for(int i=index(dest);i>=0;i=parent[i])route.push_back({i%BattleW,i/BattleW});std::reverse(route.begin(),route.end());return route;}
  if(p!=start&&inMoat(u,p)&&!inMoat(u,start))continue;
  const Point offsets[]={{-1,0},{1,0},{p.y%2?-1:0,-1},{p.y%2?0:1,-1},{p.y%2?-1:0,1},{p.y%2?0:1,1}};
  for(auto off:offsets){Point next{p.x+off.x,p.y+off.y};if(available(next)&&parent[index(next)]==-2){parent[index(next)]=index(p);queue.push(next);}}
 }return {};
}
std::optional<int> Game::battlePath(const CombatStack&u,Point dest)const{auto route=battleRoute(u,dest);if(route.empty())return {};return Units[u.unit].flying?hexDistance(route.front(),route.back()):int(route.size())-1;}
bool Game::canShoot(const CombatStack&u)const{if(!battle||Units[u.unit].shots==0||u.shots<=0||spellEffect(u,Forgetfulness)>=100)return false;for(auto&t:battle->units)if(t.hp>0&&t.team!=u.team&&distance(u,t)==1)return false;return true;}
std::optional<AttackPlan> Game::attackPlan(const CombatStack&u,const CombatStack&t)const{
 if(!battle||u.hp<=0||t.hp<=0||u.team==t.team)return {};
 AttackPlan plan{{u.x,u.y},0,canShoot(u)&&distance(u,t)>1};
 if(plan.ranged||distance(u,t)==1)return plan;
 int best=999;
 for(int y=0;y<BattleH;++y)for(int x=0;x<BattleW;++x){
  auto candidate=u;candidate.x=x;candidate.y=y;
  if(distance(candidate,t)!=1)continue;
  auto cost=battlePath(u,{x,y});
  if(cost&&*cost<=speed(u)&&*cost<best){best=*cost;plan.from={x,y};plan.traveled=*cost;}
 }
 return best==999?std::nullopt:std::optional<AttackPlan>(plan);
}
int Game::damage(const CombatStack&a,const CombatStack&t,bool ranged,int traveled,double roll,int sampledBase,bool ignoreReductions,bool lucky)const{const auto&u=Units[a.unit];int attack=std::max(0,u.attack+(a.team==0?primarySkill(0):battle?battle->defenderPrimary[0]:0)+spellEffect(a,Prayer)-spellEffect(a,Weakness)+(ranged?spellEffect(a,Precision):spellEffect(a,Bloodlust))),defense_=defense(t);defense_=defense_*(100-u.enemyDefenseReduction)/100;
 int leader=a.team==0?hero.originalId:battle&&heroes.contains(battle->targetId)?heroes.at(battle->targetId).value.originalId:-1;if(leader==131&&(a.unit==116||a.unit==117))attack+=2;
 int delta=attack-defense_,increase=(delta>=0?std::min(60,delta)*5:0)+(lucky?100:0);
 for(auto[target,bonus]:u.hates)if(target==t.unit)increase+=bonus;
 const std::array<int,SkillCount> noSkills{};const auto&skills=a.team==0?hero.skills:battle?battle->defenderSkills:noSkills;increase+=ranged?std::array<int,4>{0,10,25,50}[skills[Archery]]:skills[Offense]*10;
 if(!ranged&&u.joust&&t.unit!=Pikeman&&t.unit!=Halberdier)increase+=traveled*5;
 double base=u.minDamage+(u.maxDamage-u.minDamage)*std::clamp(roll,0.,1.);if(a.effects[Bless]>0)base=u.maxDamage+spellEffect(a,Bless);if(a.effects[Curse]>0)base=std::max(1,u.minDamage-spellEffect(a,Curse));
 int64_t numerator=1,denominator=1;
 auto factor=[&](int top,int bottom){int64_t common=std::gcd(numerator,int64_t(bottom));numerator/=common;bottom/=common;common=std::gcd(denominator,int64_t(top));denominator/=common;top/=common;numerator*=top;denominator*=bottom;};
 factor(100+increase,100);
 if(!ignoreReductions){
  if(delta<0)factor(40-std::min(28,-delta),40);
  factor(20-(t.team==0?hero.skills[Armorer]:battle?battle->defenderSkills[Armorer]:0),20);
  if(!ranged&&t.effects[Shield]>0)factor(100-spellEffect(t,Shield),100);
  if(ranged&&t.effects[AirShield]>0)factor(100-spellEffect(t,AirShield),100);
  if(ranged&&!u.noDistancePenalty&&distance(a,t)>10)factor(1,2);
  if(ranged&&wallPenalty(a,t))factor(1,2);
  if(!ranged&&u.shots>0&&!u.noMeleePenalty)factor(1,2);
  if(u.shots>0&&a.effects[Forgetfulness]>0)factor(1,2);
 }
 if(sampledBase>=0){int64_t common=std::gcd(int64_t(sampledBase),denominator);return std::max(1,int((sampledBase/common)*numerator/(denominator/common)));}
 return std::max(1,int(std::floor(static_cast<long double>(a.count)*base*numerator/denominator)));
}

int Game::sampleBaseDamage(const CombatStack&stack){const auto&u=Units[stack.unit];if(stack.effects[Bless]>0)return stack.count*(u.maxDamage+spellEffect(stack,Bless));if(stack.effects[Curse]>0)return stack.count*std::max(1,u.minDamage-spellEffect(stack,Curse));if(u.minDamage==u.maxDamage)return stack.count*u.minDamage;int samples=std::min(stack.count,10),total=0;for(int i=0;i<samples;i++)total+=u.minDamage+int(random()*(u.maxDamage-u.minDamage+1));return int(int64_t(total)*stack.count/samples);}
void Game::startBattle(int id,int defendingTown,int returnHero,int heroDefendingTown){if(battle)return;auto*o=objectById(id);if(!o)return;Battle b;if(heroes.contains(id)){auto previous=hero;hero=heroes[id].value;for(int stat=0;stat<4;++stat)b.defenderPrimary[stat]=primarySkill(stat);b.defenderSkills=hero.skills;b.defenderMana=hero.mana;b.defenderSpells=hero.spells;b.defenderSpellbook=hero.spellbook;hero=previous;}b.units.reserve(MaxBattleStacksPerSide*2);b.targetId=id;for(int team=0;team<2;team++){const auto&a=team==0?hero.army:o->army;int stacks=armyStacks(a),rank=0;for(int i=0;i<ArmySlots;i++){if(a[i].empty())continue;CombatStack s;s.id=team*100+i;s.slot=i;s.unit=a[i].unit;s.team=team;s.count=s.initialCount=a[i].count;s.permanentCount=s.count;s.hp=s.count*Units[s.unit].hp;auto position=battleDeployment(team,Units[s.unit].size,stacks,rank++,team==0&&hero.tightFormation);s.x=position.x;s.y=position.y;s.shots=Units[s.unit].shots;b.units.push_back(s);}}b.log.push_back("与"+o->name+"的战斗开始。");b.defendingTown=defendingTown;b.returnHero=returnHero;battle=std::move(b);if(defendingTown)setupSiege(defendingTown,1);else if(heroDefendingTown)setupSiege(heroDefendingTown,1);else if(o->type==ObjectType::Town)setupSiege(o->id,0);else if(heroes.contains(id)){for(const auto&city:objects)if(city.type==ObjectType::Town&&city.owner==o->owner&&city.level==o->level&&city.pos==o->pos){setupSiege(city.id,0);break;}}report.reset();emit({GameEventType::CombatStart,-1,id});nextRound();}
void Game::prepareCurrent(){
 while(battle){auto*u=current();if(!u||u->preparedRound==battle->round)return;u->preparedRound=battle->round;int preparing=u->id;moatHit(preparing);if(finishBattle())return;refreshQueue();if(battle->queue.empty()){nextAction();return;}u=current();if(!u||u->id!=preparing)continue;const auto&def=Units[u->unit];
  if(def.regeneration>0){int healing=std::min(def.regeneration,u->count*def.hp-u->hp);u->hp+=healing;if(healing)battle->log.push_back(def.name+"再生，恢复 "+std::to_string(healing)+" 点生命。");}
  if(u->team==1&&def.manaDrain>0){int drain=std::min(hero.mana,def.manaDrain);hero.mana-=drain;if(drain)battle->log.push_back(def.name+"吸取 "+std::to_string(drain)+" 点魔法值。");}
  int value=morale(*u);if(value>=0||u->moraleUsed||random()*12>=-value)return;
  u->moraleUsed=true;u->acted=true;battle->log.push_back(def.name+"因士气低落失去行动。");emit({GameEventType::CombatMorale,u->unit,-1,{u->x,u->y},u->id,-1,{},*u});nextAction(false);
 }
}
void Game::nextRound(bool prepare){if(!battle)return;auto&b=*battle;b.queue.clear();b.waiting.clear();b.waitPhase=b.moralePhase=false;b.moraleWaiting.clear();for(auto&u:b.units){u.moraleUsed=false;u.waited=false;u.acted=false;u.defending=false;u.defendingBonus=0;u.retaliations=0;if(u.origin==StackOrigin::Clone&&u.hp>0&&--u.cloneTurns<=0){b.log.push_back(Units[u.unit].name+"的镜像到期消失。");removeStack(u,false);}for(int s=0;s<SpellCount;s++)if(u.effects[s]>0&&--u.effects[s]==0)u.effectMastery[s]=0;if(u.hp>0)b.queue.push_back(u.id);}refreshQueue();b.log.push_back("第 "+std::to_string(b.round)+" 轮。");siegeTowers();if(!battle)return;if(!finishBattle()&&prepare)prepareCurrent();}
void Game::refreshQueue(){if(!battle)return;auto&b=*battle;auto alive=[&](int id){auto*u=byId(b,id);return u&&u->hp>0;};std::erase_if(b.queue,[&](int id){return !alive(id);});std::erase_if(b.waiting,[&](int id){return !alive(id);});std::erase_if(b.moraleWaiting,[&](int id){return !alive(id);});if(!b.moralePhase)std::stable_sort(b.queue.begin(),b.queue.end(),[&](int a,int c){auto*u=byId(b,a);auto*v=byId(b,c);int as=speed(*u),bs=speed(*v);if(as!=bs)return b.waitPhase?as<bs:as>bs;if(u->team!=v->team)return u->team<v->team;return u->slot<v->slot;});}
void Game::hurt(CombatStack&t,int n){
 int before=t.count;
 if(n>0&&t.origin==StackOrigin::Clone)removeStack(t,false);
 else{t.hp=std::max(0,t.hp-n);t.count=(t.hp+Units[t.unit].hp-1)/Units[t.unit].hp;t.permanentCount=std::min(t.permanentCount,t.count);if(t.hp==0)removeStack(t,true);}
 if(n>0)emit({t.hp>0?GameEventType::CombatHit:GameEventType::CombatDeath,t.unit,t.hp>0?n:int(t.origin),{t.x,t.y},t.id,-1,{},t});
 battle->log.push_back(Units[t.unit].name+"受到 "+std::to_string(n)+" 点伤害，损失 "+std::to_string(before-t.count)+" 个单位。");
}
void Game::strike(int aid,int tid,bool ranged,int traveled,bool counter){
 auto&a=*byId(*battle,aid);auto&t=*byId(*battle,tid);
 if(a.hp<=0||t.hp<=0||(ranged&&a.shots<=0))return;
 auto hit=[&](int distance_){
  int rating=luck(a);bool lucky=rating>0&&random()*24<rating;
  if(lucky){battle->log.push_back(Units[a.unit].name+"发动幸运攻击。");emit({GameEventType::CombatLuck,a.unit,1,{a.x,a.y},a.id,t.id,{},a});}
  int base=sampleBaseDamage(a),hpBefore=t.hp;
  int dealt=damage(a,t,ranged,distance_,.5,base,false,lucky);
  int reflectedBase=damage(a,t,false,distance_,.5,base,true,lucky);
  if(ranged)a.shots--;
  emit({GameEventType::CombatAttack,a.unit,ranged?1:0,{a.x,a.y},a.id,t.id,{},a});
  int shield=std::max(Units[t.unit].fireShield,spellEffect(t,FireShield));
  hurt(t,dealt);
  if(!ranged&&dealt>3&&shield>0&&(Units[a.unit].schoolImmunity&FireSchool)==0){
   if(t.hp==0)reflectedBase=std::min(reflectedBase,hpBefore);
   int reflected=spellDamage(a,FireShield,int(int64_t(reflectedBase)*shield/100));
   if(reflected>0){battle->log.push_back(Units[t.unit].name+"的烈火神盾反射伤害。");hurt(a,reflected);}
  }
 };
 battle->log.push_back(Units[a.unit].name+(counter?"反击":"攻击")+Units[t.unit].name+"。");
 hit(traveled);
 if(!ranged&&!counter&&!Units[a.unit].noRetaliation&&a.hp>0&&t.hp>0&&t.retaliations<Units[t.unit].retaliations+spellEffect(t,Counterstrike)){t.retaliations++;strike(tid,aid,false,0,true);}
 if(!counter&&Units[a.unit].doubleAttack&&(Units[a.unit].shots==0||ranged)&&a.hp>0&&t.hp>0&&(!ranged||a.shots>0)){battle->log.push_back(Units[a.unit].name+"发动第二次攻击。");hit(0);}
}
bool Game::finishBattle(){
 if(!battle)return false;auto&b=*battle;bool blue=false,red=false;
 for(auto&u:b.units)if(u.hp>0)(u.team==0?blue:red)=true;
 if(blue&&red)return false;
 BattleReport r;int64_t xp=0;hero.army={};
 for(auto&u:b.units)if(u.origin==StackOrigin::Army){
  if(u.team==0){r.before[u.slot]={u.unit,u.initialCount};int retained=std::min(u.count,u.permanentCount);if(retained>0)hero.army[u.slot]={u.unit,retained};}
  else{r.enemies[u.slot]={u.unit,u.initialCount};xp+=int64_t(Units[u.unit].hp)*u.initialCount;}
 }
 r.won=blue&&armyCount(hero.army)>0;r.after=hero.army;r.xp=r.won?int(std::min<int64_t>(xp,1000000000)):0;
 consumeAdventureBonuses();
 if(heroes.contains(b.targetId))heroes[b.targetId].value.mana=b.defenderMana;
 int id=b.targetId,deferred=b.deferredVisitId,defendingTown=b.defendingTown,returnHero=b.returnHero,heroDefendingTown=!b.defendingTown&&b.siege.attackerTeam==1?b.siege.townId:0;if(!r.won&&heroes.contains(id)){Army survivors{};for(const auto&u:b.units)if(u.team==1&&u.count>0)survivors[u.slot]={u.unit,u.count};heroes[id].value.army=survivors;if(auto*o=objectById(id))o->army=survivors;}battle.reset();report=r;emit({GameEventType::CombatEnd,-1,r.won?1:0});
 if(defendingTown){if(r.won){heroes.erase(id);std::erase_if(objects,[id](const auto&o){return o.id==id;});if(settlements.contains(defendingTown))settlements[defendingTown].garrison=r.after;if(activeTownId==defendingTown)town.garrison=r.after;if(auto*o=objectById(defendingTown))o->army=r.after;}else if(auto*o=objectById(defendingTown)){o->owner=heroes.contains(id)?heroes[id].owner:1;o->army={};settlements[defendingTown].garrison={};}activeHeroId=returnHero;if(heroes.contains(returnHero)){hero=heroes[returnHero].value;mapLevel=heroes[returnHero].level;}syncHeroObjects();checkScenarioVictory();return true;}
 if(heroDefendingTown&&!r.won){if(auto*city=objectById(heroDefendingTown)){city->owner=heroes.contains(id)?heroes[id].owner:1;city->army={};settlements[heroDefendingTown].garrison={};if(activeTownId==heroDefendingTown)town.garrison={};if(heroes.contains(id)){heroes[id].value.pos=city->pos;heroes[id].level=city->level;}}}
 if(!r.won){result=actingOwner==0?-1:0;message(hero.name+"没有可带离战场的部队，战役失败。");}
 else{gainXP(r.xp);auto*o=objectById(id);if(imported()&&o&&o->properties.contains("absod_id"))scenarioRules["defeated"].push_back(o->properties["absod_id"]);if(o&&o->type==ObjectType::Town){o->owner=actingOwner;o->army={};if(imported()){if(settlements.contains(id))settlements[id].garrison={};selectTown(id);checkScenarioVictory();message("城镇已被占领。");}else{result=1;message("黑鸦堡已被占领！边境重归和平。");}}else if(imported()&&o&&o->type!=ObjectType::Monster&&o->type!=ObjectType::EnemyHero){o->army={};applyVisit(id);}else{heroes.erase(id);std::erase_if(objects,[id](auto&o){return o.id==id;});message("战斗胜利，获得 "+std::to_string(xp)+" 点经验。");if(imported())checkScenarioVictory();}}
 if(!r.won&&imported()&&activeHeroId){heroes.erase(activeHeroId);auto next=std::find_if(heroes.begin(),heroes.end(),[&](const auto&p){return p.second.owner==actingOwner;});if(next!=heroes.end()){activeHeroId=next->first;hero=next->second.value;mapLevel=next->second.level;result=0;}else{activeHeroId=0;if(actingOwner==0&&hasTown()){result=0;hero=Hero{};hero.army={};hero.movement=0;hero.name="请从酒馆招募英雄";hero.pos=town.pos;mapLevel=town.level;}}syncHeroObjects();}
 if(r.won&&deferred&&!guardingMonster(hero.pos)){auto*object=objectById(deferred);if(object&&object->visitableAt(hero.pos)&&!(object->type==ObjectType::Town&&object->owner!=0))applyVisit(deferred);}
 return true;
}
void Game::nextAction(bool prepare){
 if(finishBattle())return;auto&b=*battle;if(!b.queue.empty())b.queue.erase(b.queue.begin());refreshQueue();
 if(b.queue.empty()){
  if(!b.waitPhase&&!b.moralePhase&&!b.moraleWaiting.empty()){b.queue=std::move(b.moraleWaiting);b.moraleWaiting.clear();b.moralePhase=true;}
  else if(!b.waitPhase&&!b.waiting.empty()){b.queue=std::move(b.waiting);b.waiting.clear();b.moralePhase=false;b.waitPhase=true;refreshQueue();}
  else{siegeCatapult();if(!battle)return;b.round++;nextRound(false);}
 }
 if(!battle)return;if(prepare)prepareCurrent();if(battle&&battle->log.size()>100)battle->log.erase(battle->log.begin(),battle->log.end()-80);
}
std::string Game::act(const std::string&action,Point p){auto*u=current();if(!u||!battle)return "没有正在进行的战斗";if(action=="wait"){if(u->waited||battle->waitPhase||battle->moralePhase)return "本轮已经等待过";u->waited=true;bool moraleWait=u->moraleUsed&&u->acted;if(moraleWait)battle->moraleWaiting.push_back(u->id);else battle->waiting.push_back(u->id);battle->log.push_back(Units[u->unit].name+(moraleWait?"等待，将在普通行动后按等待顺序进行士气额外行动。":"等待，稍后按速度从慢到快行动。"));nextAction();return "";}if(action=="defend"){u->defendingBonus=std::max(0,defense(*u))/5;u->defending=true;u->acted=true;battle->log.push_back(Units[u->unit].name+"防御 +20%。");completeAction(false);return "";}
 if(action=="move"){auto path=battleRoute(*u,p);int distance_=path.empty()?0:Units[u->unit].flying?hexDistance(path.front(),path.back()):int(path.size())-1;if(distance_==0||distance_>speed(*u))return "无法到达此位置";u->x=p.x;u->y=p.y;int moving=u->id;moatHit(moving);if(finishBattle())return "";u=current();if(!u||u->id!=moving||u->hp<=0){nextAction();return "";}emit({GameEventType::CombatMove,u->unit,0,p,u->id,-1,std::move(path)});u->acted=true;battle->log.push_back(Units[u->unit].name+"移动。");completeAction(true);return "";}
 if(action=="attack"){auto*t=combatAt(p);if(!t||t->team==u->team)return "请选择敌军";auto plan=attackPlan(*u,*t);if(!plan)return "敌军不在移动攻击范围内";bool ranged=plan->ranged;int traveled=plan->traveled;if(traveled){auto path=battleRoute(*u,plan->from);u->x=plan->from.x;u->y=plan->from.y;int moving=u->id;moatHit(moving);if(finishBattle())return "";u=current();if(!u||u->id!=moving||u->hp<=0){nextAction();return "";}emit({GameEventType::CombatMove,u->unit,0,plan->from,u->id,t->id,std::move(path)});}u->acted=true;int aid=u->id,tid=t->id;strike(aid,tid,ranged,traveled);completeAction(true);return "";}return "未知指令";}
void Game::legacyBattleAi(){auto*u=current();if(!battle||!u)return;std::vector<int>targets;for(auto&t:battle->units)if(t.hp>0&&t.team!=u->team)targets.push_back(t.id);std::sort(targets.begin(),targets.end(),[&](int a,int c){return distance(*u,*byId(*battle,a))<distance(*u,*byId(*battle,c));});for(int id:targets){auto*t=byId(*battle,id);if(act("attack",{t->x,t->y}).empty())return;}
 if(targets.empty())return;auto target=*byId(*battle,targets.front());int best=999,bestCost=-1;Point dest;for(int y=0;y<BattleH;y++)for(int x=0;x<BattleW;x++){auto d=battlePath(*u,{x,y});if(!d||*d==0||*d>speed(*u))continue;auto c=*u;c.x=x;c.y=y;int score=distance(c,target);if(score<best||(score==best&&*d>bestCost)){best=score;bestCost=*d;dest={x,y};}}if(best<999)act("move",dest);else act("defend");}
std::string Game::retreat(){if(!battle)return "没有正在进行的战斗";return "撤退重雇需要完整酒馆英雄系统，当前请完成战斗或读取存档。";}
}
