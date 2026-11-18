#include "h3/game.hpp"
#include <algorithm>
#include <cmath>
#include <set>

namespace h3 {
namespace {
const std::array<CreatureAbility,CreatureUnitCount> Abilities={{
#include "../generated/creature_abilities.inc"
}};
enum Proc{Disease=70,Bind,Petrify,Paralyze,Poison,Age,Acid,AcidDamage,DeathStare,LifeDrain,Rebirth,Thunderbolt,DispelHelpful};
const CombatStack* abilityTarget(const Game&g,Point p,bool dead){if(auto*t=g.combatAt(p))return t;if(dead&&g.battle)for(const auto&u:g.battle->units)if(u.corpse){auto cells=g.footprint(u,{u.x,u.y});if(std::find(cells.begin(),cells.end(),p)!=cells.end())return &u;}return nullptr;}
CombatStack* find(Battle&b,int id){auto i=std::find_if(b.units.begin(),b.units.end(),[id](auto&u){return u.id==id;});return i==b.units.end()?nullptr:&*i;}
}
const CreatureAbility& creatureAbility(int unit){static const CreatureAbility empty;return unit>=0&&unit<CreatureUnitCount?Abilities[unit]:empty;}
int Game::stackHealth(const CombatStack&u)const{int n=u.baseHealth>0?u.baseHealth:Units[u.unit].hp;if(u.aged>0)n=n/2;n=n*(100-u.poison)/100;return std::max(1,n);}
void Game::setupStackHealth(CombatStack&u){u.baseHealth=combatHero(u.team)?artifactHealth(*combatHero(u.team),u.unit):Units[u.unit].hp;u.hp=u.count*stackHealth(u);u.casts=creatureAbility(u.unit).casts;}
void Game::refreshStackHealth(CombatStack&u,int old){if(u.count<=0)return;int top=std::max(1,u.hp-(u.count-1)*old);u.hp=(u.count-1)*stackHealth(u)+std::min(top,stackHealth(u));}
void Game::clearCreatureEffects(CombatStack&u){int old=stackHealth(u);u.poison=u.poisonTurns=u.aged=u.disease=u.petrified=u.paralyzed=0;refreshStackHealth(u,old);}
int Game::magicResistance(const CombatStack&u)const{
 int base=u.team==0?hero.skills[Resistance]:battle?battle->defenderSkills[Resistance]:0;int value=combatHero(u.team)?int(secondaryEffect(*combatHero(u.team),Resistance,std::array<int,4>{0,5,10,20}[base])):0;
 value=std::min(100,value+Units[u.unit].magicResistance);int aura=0;
 if(battle)for(const auto&ally:battle->units)if(ally.hp>0&&ally.team==u.team&&distance(u,ally)<=1)aura=std::max(aura,creatureAbility(ally.unit).aura);
 return 100-(100-std::clamp(value,0,100))*(100-aura)/100;
}
std::vector<int> Game::creatureAttackTargets(const CombatStack&a,const CombatStack&t,bool ranged)const{
 std::vector<int> targets{t.id};if(!battle)return targets;int flags=creatureAbility(a.unit).flags;
 if(!ranged&&(flags&3))for(const auto&u:battle->units)if(u.id!=t.id&&u.id!=a.id&&u.hp>0&&u.team!=a.team&&distance(a,u)==1&&((flags&1)||distance(t,u)<=1))targets.push_back(u.id);
 if(ranged&&(flags&24))for(const auto&u:battle->units)if(u.id!=t.id&&u.hp>0&&distance(t,u)<=1&&((flags&8)||Units[u.unit].living))targets.push_back(u.id);
 if(!ranged&&(flags&4)){
  auto axial=[](Point p){return Point{p.x-(p.y+(p.y&1))/2,p.y};};
  for(auto from:footprint(a,{a.x,a.y}))for(auto hit:footprint(t,{t.x,t.y}))if(hexDistance(from,hit)==1){auto f=axial(from),h=axial(hit);Point end{2*h.x-f.x,2*h.y-f.y};end.x+=(end.y+(end.y&1))/2;if(auto*u=combatAt(end);u&&u->id!=a.id&&u->id!=t.id)targets.push_back(u->id);}
 }
 std::set<int> unique;std::erase_if(targets,[&](int id){return !unique.insert(id).second;});return targets;
}
void Game::creatureAfterAttack(int attacker,int target,bool ranged,int dealt){
 if(!battle)return;auto*a=find(*battle,attacker);auto*t=find(*battle,target);if(!a||!t||a->hp<=0||dealt<=0)return;
 const auto&traits=creatureAbility(a->unit);
 auto cue=[&](CombatStack&u,int effect){emit({GameEventType::CombatCreatureEffect,u.unit,effect,{u.x,u.y},u.id,a->id,{},u});};
 for(auto[effect,chance]:traits.procs){
  if(effect==LifeDrain){if(!Units[t->unit].living||a->origin==StackOrigin::Clone)continue;int amount=std::min(dealt,a->initialCount*stackHealth(*a)-a->hp);if(amount>0){a->hp+=amount;a->count=(a->hp+stackHealth(*a)-1)/stackHealth(*a);a->permanentCount=a->count;cue(*a,LifeDrain);battle->log.push_back("吸血复生 "+std::to_string(amount)+" 点生命。");}continue;}
  if(t->hp<=0)continue;
  if(effect==DeathStare){if(ranged||!Units[t->unit].living)continue;int killed=0,cap=(a->count+9)/10;for(int i=0;i<a->count&&killed<cap;++i)if(random()*100<chance)++killed;if(killed){cue(*t,DeathStare);hurt(*t,std::min(t->count,killed)*stackHealth(*t));}continue;}
  if(random()*100>=chance)continue;
  if(effect<SpellCount){if(spellImmune(*t,effect))continue;t->effects[effect]=3;t->effectMastery[effect]=effect==Blind?3:2;t->effectBonuses[effect]=0;emit({GameEventType::CombatSpellEffect,t->unit,effect,{t->x,t->y},t->id,a->id,{},*t});continue;}
  bool livingOnly=effect==Disease||effect==Poison||effect==Age;
  if(livingOnly&&!Units[t->unit].living)continue;
  int level=effect==Disease?2:effect==Poison?1:effect==Petrify?3:effect==Paralyze?4:effect==Age?5:0;
  if(level&&Units[t->unit].spellImmunity>=level)continue;
  int old=stackHealth(*t);
  switch(effect){
   case Disease:t->disease=3;break;
   case Bind:t->boundBy=a->id;break;
   case Petrify:t->petrified=3;break;
   case Paralyze:t->paralyzed=3;break;
   case Poison:t->poison=std::max(10,t->poison);t->poisonTurns=3;break;
   case Age:t->aged=3;break;
   case Acid:t->defenseReduction+=3;break;
   case AcidDamage:cue(*t,effect);hurt(*t,a->count*25);continue;
   case Thunderbolt:if(!spellImmune(*t,LightningBolt)){cue(*t,effect);int saved=spellTeam_;spellTeam_=a->team;innateSpell_=true;hurt(*t,spellDamage(*t,LightningBolt,a->count*10));innateSpell_=false;spellTeam_=saved;}continue;
   case DispelHelpful:for(int spell=0;spell<SpellCount;++spell)if(!Spells[spell].enemy&&!Spells[spell].persistent)t->effects[spell]=t->effectMastery[spell]=t->effectBonuses[spell]=0;break;
   default:continue;
  }
  refreshStackHealth(*t,old);cue(*t,effect);
 }
 refreshQueue();
}
void Game::weeklyCreatureIncome(){if(day<2||(day-1)%7)return;for(int owner=0;owner<8;++owner){bool crystal=false;if(!imported()&&owner==0)crystal=std::any_of(hero.army.begin(),hero.army.end(),[](auto&s){return !s.empty()&&s.unit==139;});for(const auto&[id,h]:heroes)if(h.owner==owner){const auto&army=id==activeHeroId?hero.army:h.value.army;crystal|=std::any_of(army.begin(),army.end(),[](auto&s){return !s.empty()&&s.unit==139;});}if(crystal){auto&bank=owner==0?resources:playerResources[owner];bank[Crystal]=std::min(1000000000,bank[Crystal]+3);}}}
void Game::creatureNewRound(){
 if(!battle)return;for(auto&u:battle->units){int old=stackHealth(u);for(int*turns:{&u.disease,&u.aged,&u.petrified,&u.paralyzed})if(*turns>0)--*turns;
  if(u.poisonTurns>0){if(--u.poisonTurns==0)u.poison=0;else u.poison=std::min(30,u.poison+10);}refreshStackHealth(u,old);
  if(u.boundBy>=0){auto*source=find(*battle,u.boundBy);if(!source||source->hp<=0||distance(u,*source)>1)u.boundBy=-1;}
 }
}
std::vector<int> Game::genieSpells(const CombatStack&caster,const CombatStack&target)const{
 std::vector<int> out;if(target.team!=caster.team||target.id==caster.id||target.hp<=0)return out;
 const int spells[]={Bless,Bloodlust,Cure,Haste,Shield,StoneSkin,Fortune,Precision,AirShield,AntiMagic,Mirth,Counterstrike,FireShield,Frenzy,Prayer,Slayer,MagicMirror};
 bool shooters=false,kings=false;for(const auto&u:battle->units)if(u.hp>0&&u.team!=caster.team){shooters|=Units[u.unit].shots>0&&u.shots>0;kings|=(creatureAbility(u.unit).flags&2048)||(u.unit>=96&&u.unit<=97)||(u.unit>=110&&u.unit<=111)||(u.unit>=12&&u.unit<=13)||(u.unit>=54&&u.unit<=55);}
 for(int spell:spells){if(target.effects[spell]||spellImmune(target,spell))continue;if(spell==Precision&&Units[target.unit].shots==0)continue;if(spell==AirShield&&!shooters)continue;if(spell==Slayer&&!kings)continue;if(spell==Bless&&Units[target.unit].minDamage==Units[target.unit].maxDamage)continue;if(spell==Bloodlust&&Units[target.unit].shots>0&&canShoot(target))continue;
  if(spell==Cure){bool negative=target.poison||target.aged||target.disease||target.petrified||target.paralyzed;for(int e=0;e<SpellCount;++e)negative|=target.effects[e]>0&&Spells[e].enemy;if(target.hp==target.count*stackHealth(target)&&!negative)continue;}out.push_back(spell);
 }return out;
}
int Game::creatureCastingSpell(const CombatStack&u)const{
 const auto&t=creatureAbility(u.unit);if(t.flags&4096)return -1;if(u.casts==0||(u.casts<0&&t.casts==0))return -1;
 if(u.creatureSpell>=0)return u.creatureSpell;if(t.flags&8192)return 1001;if(!t.spells.empty())return t.spells.front()[0];return -1;
}
bool Game::canCreatureCast(const CombatStack&u,Point p)const{
 if(!battle||u.hp<=0||u.effects[Blind]>0||u.effects[Berserk]>0||u.petrified||u.paralyzed||battle->noMagic||battle->tacticsTeam>=0)return false;
 int spell=creatureCastingSpell(u);if(spell<0)return false;
 const auto*target=abilityTarget(*this,p,spell==Resurrection||spell==1000);
 if(spell==Resurrection&&(!target||target->team!=u.team||!Units[target->unit].living||target->count>=target->initialCount))return false;
 if(spell==1001)return target&&!genieSpells(u,*target).empty();
 if(spell==1000)return target&&target->team==u.team&&target->hp==0&&Units[target->unit].living&&target->origin==StackOrigin::Army;
 if(!target||((u.unit==13||(creatureAbility(u.unit).flags&8192))&&target->id==u.id)||target->team==u.team&&Spells[spell].enemy||target->team!=u.team&&!Spells[spell].enemy||spellImmune(*target,spell))return false;
 if(Spells[spell].kind==SpellKind::Timed&&target->effects[spell]>0)return false;return true;
}
std::string Game::castCreature(Point p){
 auto*u=current();if(!u||!canCreatureCast(*u,p))return "这里没有可施放生物能力的有效目标。";
 int actor=u->id,spell=creatureCastingSpell(*u);const auto&traits=creatureAbility(u->unit);auto*target=const_cast<CombatStack*>(abilityTarget(*this,p,spell==Resurrection||spell==1000));
 if(!target)return "没有有效目标。";if(spell==1001){auto choices=genieSpells(*u,*target);if(choices.empty())return "这支部队没有可接受的增益法术。";spell=choices[size_t(random()*choices.size())];}if(u->casts<0)u->casts=traits.casts;--u->casts;u->acted=true;emit({GameEventType::SpellCast,u->unit,spell==1000?Resurrection:spell,p,u->id,-1,{},*u});
 if(spell==1000){int count=std::min({target->initialCount,Units[target->unit].hp*target->initialCount/Units[48].hp,u->count*50/Units[48].hp});if(count<=0){++u->casts;u->acted=false;return "尸体的生命值不足以召唤恶鬼。";}target->unit=48;target->count=target->initialCount=target->permanentCount=count;target->baseHealth=Units[48].hp;target->hp=count*stackHealth(*target);target->corpse=false;target->deathOrder=0;target->effects={};target->effectMastery={};target->effectBonuses={};target->casts=0;clearCreatureEffects(*target);emit({GameEventType::CombatSpellEffect,target->unit,Resurrection,p,target->id,actor,{},*target});}
 else if(spell==Resurrection){int quantity=u->count*100;revive(*target,quantity,true,true);emit({GameEventType::CombatSpellEffect,target->unit,Resurrection,p,target->id,actor,{},*target});}
 else if(spell==Cure){clearCreatureEffects(*target);for(int e=0;e<SpellCount;++e)if(Spells[e].enemy&&!Spells[e].persistent)target->effects[e]=target->effectMastery[e]=target->effectBonuses[e]=0;target->hp=std::min(target->count*stackHealth(*target),target->hp+6*Spells[Cure].power+Spells[Cure].values[2]);emit({GameEventType::CombatSpellEffect,target->unit,Cure,p,target->id,actor,{},*target});}
 else if(Spells[spell].kind==SpellKind::Damage){int old=spellTeam_;spellTeam_=u->team;innateSpell_=true;int raw=Spells[spell].power*(u->count*traits.power/100)+Spells[spell].values[2];std::vector<int>victims;
  if(Spells[spell].target==SpellTarget::Chain){int currentTarget=target->id;for(int hop=0;hop<5&&currentTarget>=0;++hop){victims.push_back(currentTarget);auto&previous=*find(*battle,currentTarget);int nearest=1000;std::vector<int>next;for(const auto&t:battle->units)if(t.hp>0&&std::find(victims.begin(),victims.end(),t.id)==victims.end()&&!spellImmune(t,spell)){int d=distance(previous,t);if(d<nearest){nearest=d;next={t.id};}else if(d==nearest)next.push_back(t.id);}currentTarget=next.empty()?-1:next[size_t(random()*next.size())];}}
  else for(const auto&t:battle->units)if(t.hp>0&&!spellImmune(t,spell)){bool within=t.id==target->id;if(Spells[spell].target==SpellTarget::Radius||Spells[spell].target==SpellTarget::Ring){within=false;for(auto cell:footprint(t,{t.x,t.y})){int d=hexDistance(cell,p);within|=d<=Spells[spell].radius&&(Spells[spell].target!=SpellTarget::Ring||d>0);}}if(within)victims.push_back(t.id);}
  for(int id:victims){auto&t=*find(*battle,id);hurt(t,spellDamage(t,spell,raw));emit({GameEventType::CombatSpellEffect,t.unit,spell,{t.x,t.y},t.id,actor,{},t});if(Spells[spell].target==SpellTarget::Chain)raw/=2;}
  innateSpell_=false;spellTeam_=old;
 }else{target->effects[spell]=traits.duration?traits.duration:6;target->effectMastery[spell]=traits.spells.empty()?2:traits.spells.front()[1];target->effectBonuses[spell]=0;emit({GameEventType::CombatSpellEffect,target->unit,spell,p,target->id,actor,{},*target});}
 battle->log.push_back(Units[u->unit].name+"施放了"+(spell==1000?std::string("召唤恶鬼"):Spells[spell].name)+"。");refreshQueue();completeAction(false);return {};
}
bool Game::autoCreatureCast(){
 auto*u=current();if(!u||creatureCastingSpell(*u)<0)return false;int chosen=-1;double best=0;
 auto offensiveValue=[&](const CombatStack&target,int spell){
  const auto&traits=creatureAbility(u->unit);int raw=Spells[spell].power*(u->count*traits.power/100)+Spells[spell].values[2];double value=0;std::vector<int>chain;
  if(Spells[spell].target==SpellTarget::Chain){int next=target.id;for(int hop=0;hop<5&&next>=0;++hop){chain.push_back(next);const auto&last=*find(*battle,next);int nearest=1000;next=-1;for(const auto&t:battle->units)if(t.hp>0&&std::find(chain.begin(),chain.end(),t.id)==chain.end()&&!spellImmune(t,spell)){int d=distance(last,t);if(d<nearest||(d==nearest&&t.team==u->team)){nearest=d;next=t.id;}}}}
  int old=spellTeam_;bool innate=innateSpell_;spellTeam_=u->team;innateSpell_=true;
  for(const auto&t:battle->units)if(t.hp>0&&!spellImmune(t,spell)){bool hit=t.id==target.id;int amount=raw;
   if(Spells[spell].target==SpellTarget::Chain){auto at=std::find(chain.begin(),chain.end(),t.id);hit=at!=chain.end();if(hit)amount>>=int(at-chain.begin());}
   else if(Spells[spell].target==SpellTarget::Radius||Spells[spell].target==SpellTarget::Ring){hit=false;for(auto cell:footprint(t,{t.x,t.y})){int d=hexDistance(cell,{target.x,target.y});hit|=d<=Spells[spell].radius&&(Spells[spell].target!=SpellTarget::Ring||d>0);}}
   if(hit)value+=std::min(t.hp,spellDamage(t,spell,amount))*(Units[t.unit].cost/std::max(1.,double(stackHealth(t))))*(t.team==u->team?-1.2:1.);
  }innateSpell_=innate;spellTeam_=old;return value;
 };
 for(const auto&t:battle->units){Point p{t.x,t.y};if(!canCreatureCast(*u,p))continue;int spell=creatureCastingSpell(*u);double value=spell==Resurrection?std::min(t.initialCount*stackHealth(t)-t.hp,u->count*100):spell==1000?std::min(t.initialCount*Units[t.unit].hp,u->count*50):spell==1001?t.count*(Units[t.unit].minDamage+Units[t.unit].maxDamage)/2.:Spells[spell].kind==SpellKind::Damage?offensiveValue(t,spell):t.count*(Units[t.unit].minDamage+Units[t.unit].maxDamage)/2.;if(value>best){best=value;chosen=t.id;}}
 if(chosen<0)return false;auto*t=find(*battle,chosen);return castCreature({t->x,t->y}).empty();
}
void Game::prepareCreature(){
 auto*u=current();if(!u||!battle)return;const auto&traits=creatureAbility(u->unit);if(u->casts<0)u->casts=traits.casts;
 if(traits.flags&8192)u->creatureSpell=-1;
 else if(u->unit==137&&!traits.spells.empty()){int weight=0;for(auto s:traits.spells)weight+=s[2];int pick=int(random()*weight);for(auto s:traits.spells){if(pick<s[2]){u->creatureSpell=s[0];break;}pick-=s[2];}}
 if((traits.flags&4096)&&!battle->noMagic&&u->casts>0&&u->abilityRound<=battle->round){
  auto eligible=[&](const CombatStack&t,int spell){if(t.hp<=0||(t.team==u->team)==Spells[spell].enemy||spellImmune(t,spell)||t.effects[spell])return false;if(spell!=Cure)return true;bool negative=t.poison||t.aged||t.disease||t.petrified||t.paralyzed;for(int e=0;e<SpellCount;++e)negative|=t.effects[e]>0&&Spells[e].enemy;return negative||t.hp<t.count*stackHealth(t);};
  std::vector<int>pool;for(auto s:traits.spells)if(std::any_of(battle->units.begin(),battle->units.end(),[&](auto&t){return eligible(t,s[0]);}))for(int n=0;n<s[2];++n)pool.push_back(s[0]);
  if(!pool.empty()){int spell=pool[size_t(random()*pool.size())];--u->casts;u->abilityRound=battle->round+3;emit({GameEventType::SpellCast,u->unit,spell,{u->x,u->y},u->id,-1,{},*u});
   for(auto&t:battle->units)if(eligible(t,spell)){if(spell==Cure){clearCreatureEffects(t);for(int e=0;e<SpellCount;++e)if(Spells[e].enemy&&!Spells[e].persistent)t.effects[e]=t.effectMastery[e]=t.effectBonuses[e]=0;t.hp=std::min(t.count*stackHealth(t),t.hp+3*Spells[Cure].power+Spells[Cure].values[3]);}else{t.effects[spell]=3;t.effectMastery[spell]=3;t.effectBonuses[spell]=0;}emit({GameEventType::CombatSpellEffect,t.unit,spell,{t.x,t.y},t.id,u->id,{},t});}
   battle->log.push_back("魔幻法师自动施放"+Spells[spell].name+"。");refreshQueue();}
 }
 if(Units[u->unit].living&&u->unit!=140){int fear=0;for(auto&t:battle->units)if(t.hp>0&&t.team!=u->team)fear=std::max(fear,creatureAbility(t.unit).fear);if(fear&&random()*100<fear){u->acted=u->moraleUsed=true;emit({GameEventType::CombatCreatureEffect,u->unit,83,{u->x,u->y},u->id,-1,{},*u});battle->log.push_back(Units[u->unit].name+"因圣龙威慑而无法行动。");nextAction(false);}}
}
void Game::openingArtifacts(){
 if(!battle)return;for(int team=0;team<2;++team)if(auto*h=combatHero(team)){
  if(wearsArtifact(*h,129))for(auto&u:battle->units)if(u.hp>0&&u.team==team&&!spellImmune(u,Prayer)){u.effects[Prayer]=11;u.effectMastery[Prayer]=3;emit({GameEventType::CombatSpellEffect,u.unit,Prayer,{u.x,u.y},u.id,-1,{},u});}
  if(wearsArtifact(*h,132))for(auto&u:battle->units)if(u.hp>0&&u.team!=team)for(int spell:{Slow,Curse,Weakness,Misfortune})if(!spellImmune(u,spell)){u.effects[spell]=51;u.effectMastery[spell]=3;emit({GameEventType::CombatSpellEffect,u.unit,spell,{u.x,u.y},u.id,-1,{},u});}
 }
}
bool Game::creatureSiegeAttack(Point p){
 auto*u=current();if(!u||u->shots<=0||!(creatureAbility(u->unit).flags&64)||!battle->siege.fort||u->team!=battle->siege.attackerTeam)return false;
 int part=siegePartAt(p);if(part<0||battle->siege.hp[part]<=0)return false;int shots=creatureAbility(u->unit).flags&128?2:1;--u->shots;
 for(int i=0;i<shots&&battle->siege.hp[part]>0;++i){emit({GameEventType::CombatAttack,u->unit,2,p,u->id,-1,{},*u});double accuracy=part==0?.05:part<3?.1:part==3?.5:.75;if(random()<accuracy)battle->siege.hp[part]=std::max(0,battle->siege.hp[part]-(random()<.5?1:2));emit({GameEventType::SiegeHit,-1,battle->siege.hp[part],p,-1,part});}
 u->acted=true;completeAction(false);return true;
}
}
